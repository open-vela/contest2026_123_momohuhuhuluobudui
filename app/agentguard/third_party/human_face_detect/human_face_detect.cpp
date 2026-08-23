#include "human_face_detect.hpp"
#include "human_face_detect_roi.hpp"
#include "dl_math.hpp"
#include <cstring>
#if CONFIG_HUMAN_FACE_DETECT_MODEL_IN_SDCARD
#include <filesystem>
#endif

#if CONFIG_HUMAN_FACE_DETECT_MODEL_IN_FLASH_RODATA
extern const uint8_t human_face_detect_espdl[] asm("_binary_human_face_detect_espdl_start");
static const char *path = (const char *)human_face_detect_espdl;
#elif CONFIG_HUMAN_FACE_DETECT_MODEL_IN_FLASH_PARTITION
static const char *path = "human_face_det";
#else
#if !defined(CONFIG_BSP_SD_MOUNT_POINT)
#define CONFIG_BSP_SD_MOUNT_POINT "/sdcard"
#endif
#endif
namespace human_face_detect {

class MSRTracePostprocessor : public dl::detect::MSRPostprocessor
{
public:
    using dl::detect::MSRPostprocessor::MSRPostprocessor;

    const std::list<dl::detect::result_t> &peek_result() const
    {
        return m_box_list;
    }
};

static bool fingerprint_tensor(dl::TensorBase *tensor,
                               ag_data_fingerprint *output)
{
    if (tensor == nullptr || output == nullptr || tensor->get_size() <= 0) {
        if (output != nullptr) {
            std::memset(output, 0, sizeof(*output));
        }
        return false;
    }

    if (tensor->dtype == dl::DATA_TYPE_INT8) {
        return ag_face_diag_fingerprint_tensor(
            tensor->get_element_ptr<int8_t>(),
            static_cast<uint32_t>(tensor->get_size()), sizeof(int8_t), true,
            output);
    }

    if (tensor->dtype == dl::DATA_TYPE_INT16) {
        return ag_face_diag_fingerprint_tensor(
            tensor->get_element_ptr<int16_t>(),
            static_cast<uint32_t>(tensor->get_size()), sizeof(int16_t), true,
            output);
    }

    std::memset(output, 0, sizeof(*output));
    return false;
}

static uint8_t bounded_count(size_t count)
{
    return static_cast<uint8_t>(std::min(count,
                                         static_cast<size_t>(UINT8_MAX)));
}

static bool copy_candidate_box(const dl::detect::result_t &candidate,
                               ag_face_box *box)
{
    if (box == nullptr || candidate.box.size() < 4 ||
        candidate.box[0] < 0 || candidate.box[1] < 0 ||
        candidate.box[2] <= candidate.box[0] ||
        candidate.box[3] <= candidate.box[1] ||
        candidate.box[2] > UINT16_MAX || candidate.box[3] > UINT16_MAX) {
        if (box != nullptr) {
            std::memset(box, 0, sizeof(*box));
        }
        return false;
    }

    box->x = static_cast<uint16_t>(candidate.box[0]);
    box->y = static_cast<uint16_t>(candidate.box[1]);
    box->width = static_cast<uint16_t>(candidate.box[2] - candidate.box[0]);
    box->height = static_cast<uint16_t>(candidate.box[3] - candidate.box[1]);
    return true;
}

static void tensor_range(dl::TensorBase *tensor, int16_t *minimum,
                         int16_t *maximum)
{
    int16_t low = INT16_MAX;
    int16_t high = INT16_MIN;

    if (tensor->dtype == dl::DATA_TYPE_INT8) {
        int8_t *data = tensor->get_element_ptr<int8_t>();
        for (int i = 0; i < tensor->get_size(); ++i) {
            low = std::min(low, static_cast<int16_t>(data[i]));
            high = std::max(high, static_cast<int16_t>(data[i]));
        }
    } else {
        int16_t *data = tensor->get_element_ptr<int16_t>();
        for (int i = 0; i < tensor->get_size(); ++i) {
            low = std::min(low, data[i]);
            high = std::max(high, data[i]);
        }
    }

    *minimum = low;
    *maximum = high;
}

static float tensor_max_probability(dl::TensorBase *tensor)
{
    int16_t minimum;
    int16_t maximum;
    tensor_range(tensor, &minimum, &maximum);
    return dl::math::sigmoid(dl::dequantize(maximum,
                                            DL_SCALE(tensor->exponent)));
}

MSR::MSR(const char *model_name, float score_thr, float nms_thr)
{
#if !CONFIG_HUMAN_FACE_DETECT_MODEL_IN_SDCARD
    m_model = new dl::Model(
        path, model_name, static_cast<fbs::model_location_type_t>(CONFIG_HUMAN_FACE_DETECT_MODEL_LOCATION));
#else
    auto sd_path =
        std::filesystem::path(CONFIG_BSP_SD_MOUNT_POINT) / CONFIG_HUMAN_FACE_DETECT_MODEL_SDCARD_DIR / model_name;
    m_model = new dl::Model(sd_path.c_str(), fbs::MODEL_LOCATION_IN_SDCARD);
#endif
    m_model->minimize();
#if CONFIG_IDF_TARGET_ESP32P4
    m_image_preprocessor =
        new dl::image::ImagePreprocessor(m_model, {0, 0, 0}, {1, 1, 1}, dl::image::DL_IMAGE_CAP_RGB_SWAP);
#else
    m_image_preprocessor = new dl::image::ImagePreprocessor(
        m_model, {0, 0, 0}, {1, 1, 1}, dl::image::DL_IMAGE_CAP_RGB_SWAP | dl::image::DL_IMAGE_CAP_RGB565_BIG_ENDIAN);
#endif
    m_postprocessor =
        new MSRTracePostprocessor(m_model,
                                  m_image_preprocessor,
                                  score_thr,
                                  nms_thr,
                                  10,
                                  {{8, 8, 9, 9, {{16, 16}, {32, 32}}}, {16, 16, 9, 9, {{64, 64}, {128, 128}}}});
}

std::list<dl::detect::result_t> &MSR::run(const dl::image::img_t &img)
{
    /* Model::minimize() allows ESP-DL to reuse tensor storage while running.
     * Capture the preprocessed input before Model::run(), otherwise this
     * diagnostic reads a later intermediate tensor rather than the image. */
    /* An explicit full-frame crop forces ESP-DL to initialize its resize map
     * on targets where the equivalent empty crop leaves scale at zero. */
    m_image_preprocessor->preprocess(
        img, full_frame_crop(img.width, img.height));
    capture_resize_scale(
        m_image_preprocessor->get_resize_scale_x(),
        m_image_preprocessor->get_resize_scale_y(),
        m_image_preprocessor->get_resize_scale_x(true),
        m_image_preprocessor->get_resize_scale_y(true),
        &m_last_resize);
    tensor_range(m_image_preprocessor->get_model_input(),
                 &m_last_input_min, &m_last_input_max);
    fingerprint_tensor(m_image_preprocessor->get_model_input(),
                       &m_last_input);

    m_model->run(dl::RUNTIME_MODE_SINGLE_CORE);
    fingerprint_tensor(m_model->get_output("score0"), &m_last_score0);
    fingerprint_tensor(m_model->get_output("box0"), &m_last_box0);
    fingerprint_tensor(m_model->get_output("score1"), &m_last_score1);
    fingerprint_tensor(m_model->get_output("box1"), &m_last_box1);
    m_postprocessor->clear_result();
    m_postprocessor->postprocess();
    MSRTracePostprocessor *trace_postprocessor =
        static_cast<MSRTracePostprocessor *>(m_postprocessor);
    capture_top_candidate(trace_postprocessor->peek_result(),
                          &m_last_candidate_before_clip);
    std::list<dl::detect::result_t> &result =
        m_postprocessor->get_result(img.width, img.height);
    capture_top_candidate(result, &m_last_candidate_after_clip);
    return result;
}

void MSR::get_last_trace(ag_face_detector_trace *trace) const
{
    if (trace == nullptr) {
        return;
    }

    trace->msr_input = m_last_input;
    trace->msr_score0 = m_last_score0;
    trace->msr_box0 = m_last_box0;
    trace->msr_score1 = m_last_score1;
    trace->msr_box1 = m_last_box1;
    trace->msr_resize = m_last_resize;
    trace->msr_candidate_before_clip = m_last_candidate_before_clip;
    trace->msr_candidate_after_clip = m_last_candidate_after_clip;
}

void MSR::get_last_input_range(int16_t *input_min, int16_t *input_max) const
{
    *input_min = m_last_input_min;
    *input_max = m_last_input_max;
}

void MSR::set_image_caps(uint32_t caps)
{
    m_image_preprocessor->set_caps(caps);
}

MNP::MNP(const char *model_name, float score_thr, float nms_thr)
{
#if !CONFIG_HUMAN_FACE_DETECT_MODEL_IN_SDCARD
    m_model = new dl::Model(
        path, model_name, static_cast<fbs::model_location_type_t>(CONFIG_HUMAN_FACE_DETECT_MODEL_LOCATION));
#else
    auto sd_path =
        std::filesystem::path(CONFIG_BSP_SD_MOUNT_POINT) / CONFIG_HUMAN_FACE_DETECT_MODEL_SDCARD_DIR / model_name;
    m_model = new dl::Model(sd_path.c_str(), fbs::MODEL_LOCATION_IN_SDCARD);
#endif
    m_model->minimize();
#if CONFIG_IDF_TARGET_ESP32P4
    m_image_preprocessor =
        new dl::image::ImagePreprocessor(m_model, {0, 0, 0}, {1, 1, 1}, dl::image::DL_IMAGE_CAP_RGB_SWAP);
#else
    m_image_preprocessor = new dl::image::ImagePreprocessor(
        m_model, {0, 0, 0}, {1, 1, 1}, dl::image::DL_IMAGE_CAP_RGB_SWAP | dl::image::DL_IMAGE_CAP_RGB565_BIG_ENDIAN);
#endif
    m_postprocessor = new dl::detect::MNPPostprocessor(
        m_model, m_image_preprocessor, score_thr, nms_thr, 10, {{1, 1, 0, 0, {{48, 48}}}});
}

MNP::~MNP()
{
    delete m_model;
    delete m_image_preprocessor;
    delete m_postprocessor;
}

MNP &MNP::set_score_thr(float score_thr)
{
    m_postprocessor->set_score_thr(score_thr);
    return *this;
}

MNP &MNP::set_nms_thr(float nms_thr)
{
    m_postprocessor->set_nms_thr(nms_thr);
    return *this;
}

dl::Model *MNP::get_raw_model()
{
    return m_model;
}

void MNP::set_image_caps(uint32_t caps)
{
    m_image_preprocessor->set_caps(caps);
}

std::list<dl::detect::result_t> &MNP::run(
    const dl::image::img_t &img,
    std::list<dl::detect::result_t> &candidates,
    ag_face_detector_trace *trace)
{
    DL_LOG_INFER_LATENCY_ARRAY_INIT_WITH_SIZE(3, 10);
    m_postprocessor->clear_result();
    for (auto &candidate : candidates) {
        if (!prepare_candidate_roi(candidate, img.width, img.height)) {
            continue;
        }

        ag_mnp_diagnostic diagnostic = {};
        bool crop_valid = copy_candidate_box(candidate, &diagnostic.crop);

        DL_LOG_INFER_LATENCY_ARRAY_START(0);
        m_image_preprocessor->preprocess(img, candidate.box);
        DL_LOG_INFER_LATENCY_ARRAY_END(0);
        bool input_valid = fingerprint_tensor(
            m_image_preprocessor->get_model_input(), &diagnostic.input);

        DL_LOG_INFER_LATENCY_ARRAY_START(1);
        m_model->run(dl::RUNTIME_MODE_SINGLE_CORE);
        DL_LOG_INFER_LATENCY_ARRAY_END(1);

        dl::TensorBase *score = m_model->get_output("score");
        bool score_valid = fingerprint_tensor(score, &diagnostic.score);
        bool box_valid = fingerprint_tensor(m_model->get_output("box"),
                                            &diagnostic.box);
        bool landmark_valid = fingerprint_tensor(
            m_model->get_output("landmark"), &diagnostic.landmark);
        diagnostic.score_percent = static_cast<uint8_t>(std::min(
            99.0f, tensor_max_probability(score) * 100.0f));
        diagnostic.valid = crop_valid && input_valid && score_valid &&
                           box_valid && landmark_valid;
        ag_face_diag_record_mnp(trace, &diagnostic);

        DL_LOG_INFER_LATENCY_ARRAY_START(2);
        m_postprocessor->postprocess();
        DL_LOG_INFER_LATENCY_ARRAY_END(2);
    }
    m_postprocessor->nms();
    std::list<dl::detect::result_t> &result = m_postprocessor->get_result(img.width, img.height);
    if (candidates.size() > 0) {
        DL_LOG_INFER_LATENCY_ARRAY_PRINT(0, "detect", "pre");
        DL_LOG_INFER_LATENCY_ARRAY_PRINT(1, "detect", "model");
        DL_LOG_INFER_LATENCY_ARRAY_PRINT(2, "detect", "post");
    }
    return result;
}

std::list<dl::detect::result_t> &MSRMNP::run(const dl::image::img_t &img)
{
    std::memset(&m_last_trace, 0, sizeof(m_last_trace));
    std::list<dl::detect::result_t> &candidates = m_msr.run(img);
    m_last_candidate_count = candidates.size();
    m_last_trace.msr_candidates = bounded_count(candidates.size());
    m_msr.get_last_trace(&m_last_trace);

    int largest_area = 0;
    for (const auto &candidate : candidates) {
        int area = candidate.box_area();
        if (area > largest_area) {
            ag_face_box candidate_box = {};
            if (copy_candidate_box(candidate, &candidate_box)) {
                m_last_trace.largest_msr = candidate_box;
                largest_area = area;
            }
        }
    }

    m_msr.get_last_input_range(&m_last_input_min, &m_last_input_max);
    float maximum_probability = std::max(
        tensor_max_probability(m_msr.get_raw_model()->get_output("score0")),
        tensor_max_probability(m_msr.get_raw_model()->get_output("score1")));
    m_last_score_percent = static_cast<uint8_t>(
        std::min(99.0f, maximum_probability * 100.0f));
    std::list<dl::detect::result_t> &result =
        m_mnp.run(img, candidates, &m_last_trace);
    m_last_trace.mnp_accepted = bounded_count(result.size());
    m_last_trace.final_faces = bounded_count(result.size());
    m_last_trace.valid = true;
    return result;
}

void MSRMNP::get_last_trace(ag_face_detector_trace *trace) const
{
    if (trace != nullptr) {
        *trace = m_last_trace;
    }
}

size_t MSRMNP::get_last_candidate_count() const
{
    return m_last_candidate_count;
}

void MSRMNP::get_last_signal(int16_t *input_min, int16_t *input_max,
                             uint8_t *score_percent) const
{
    *input_min = m_last_input_min;
    *input_max = m_last_input_max;
    *score_percent = m_last_score_percent;
}

void MSRMNP::set_image_caps(uint32_t caps)
{
    m_msr.set_image_caps(caps);
    m_mnp.set_image_caps(caps);
}

dl::detect::Detect &MSRMNP::set_score_thr(float score_thr, int idx)
{
    assert(idx == 0 || idx == 1);
    if (idx == 0) {
        m_msr.set_score_thr(score_thr);
    } else {
        m_mnp.set_score_thr(score_thr);
    }
    return *this;
}

dl::detect::Detect &MSRMNP::set_nms_thr(float nms_thr, int idx)
{
    assert(idx == 0 || idx == 1);
    if (idx == 0) {
        m_msr.set_nms_thr(nms_thr);
    } else {
        m_mnp.set_nms_thr(nms_thr);
    }
    return *this;
}

dl::Model *MSRMNP::get_raw_model(int idx)
{
    assert(idx == 0 || idx == 1);
    if (idx == 0) {
        return m_msr.get_raw_model();
    } else {
        return m_mnp.get_raw_model();
    }
}

} // namespace human_face_detect

HumanFaceDetect::HumanFaceDetect(model_type_t model_type, bool lazy_load) : m_model_type(model_type)
{
    switch (model_type) {
    case model_type_t::MSRMNP_S8_V1:
        m_score_thr[0] = human_face_detect::MSR::default_score_thr;
        m_nms_thr[0] = human_face_detect::MSR::default_nms_thr;
        m_score_thr[1] = human_face_detect::MNP::default_score_thr;
        m_nms_thr[1] = human_face_detect::MNP::default_nms_thr;
        break;
    }
    if (lazy_load) {
        m_model = nullptr;
    } else {
        load_model();
    }
}

size_t HumanFaceDetect::get_last_msr_candidate_count() const
{
    if (m_model == nullptr || m_model_type != model_type_t::MSRMNP_S8_V1) {
        return 0;
    }

    return static_cast<human_face_detect::MSRMNP *>(m_model)->get_last_candidate_count();
}

void HumanFaceDetect::get_last_msr_signal(int16_t *input_min,
                                          int16_t *input_max,
                                          uint8_t *score_percent) const
{
    if (m_model == nullptr || m_model_type != model_type_t::MSRMNP_S8_V1) {
        *input_min = 0;
        *input_max = 0;
        *score_percent = 0;
        return;
    }

    static_cast<human_face_detect::MSRMNP *>(m_model)->get_last_signal(
        input_min, input_max, score_percent);
}

void HumanFaceDetect::get_last_trace(ag_face_detector_trace *trace) const
{
    if (trace == nullptr) {
        return;
    }

    std::memset(trace, 0, sizeof(*trace));
    if (m_model != nullptr && m_model_type == model_type_t::MSRMNP_S8_V1) {
        static_cast<human_face_detect::MSRMNP *>(m_model)->get_last_trace(
            trace);
    }
}

void HumanFaceDetect::set_image_caps(uint32_t caps)
{
    if (m_model != nullptr && m_model_type == model_type_t::MSRMNP_S8_V1) {
        static_cast<human_face_detect::MSRMNP *>(m_model)->set_image_caps(caps);
    }
}

void HumanFaceDetect::load_model()
{
    switch (m_model_type) {
    case model_type_t::MSRMNP_S8_V1: {
#if CONFIG_FLASH_HUMAN_FACE_DETECT_MSRMNP_S8_V1 || CONFIG_HUMAN_FACE_DETECT_MODEL_IN_SDCARD
        m_model = new human_face_detect::MSRMNP("human_face_detect_msr_s8_v1.espdl",
                                                m_score_thr[0],
                                                m_nms_thr[0],
                                                "human_face_detect_mnp_s8_v1.espdl",
                                                m_score_thr[1],
                                                m_nms_thr[1]);
#else
        ESP_LOGE("human_face_detect", "human_face_detect_msrmnp_s8_v1 is not selected in menuconfig.");
#endif
        break;
    }
    }
}
