#pragma once

#include "agentguard/face_diagnostics.h"
#include "dl_detect_base.hpp"
#include "dl_detect_mnp_postprocessor.hpp"
#include "dl_detect_msr_postprocessor.hpp"
namespace human_face_detect {
class MSR : public dl::detect::DetectImpl {
private:
    int16_t m_last_input_min = 0;
    int16_t m_last_input_max = 0;
    ag_data_fingerprint m_last_input = {};
    ag_data_fingerprint m_last_score0 = {};
    ag_data_fingerprint m_last_box0 = {};
    ag_data_fingerprint m_last_score1 = {};
    ag_data_fingerprint m_last_box1 = {};

public:
    static inline constexpr float default_score_thr = 0.5;
    static inline constexpr float default_nms_thr = 0.5;
    MSR(const char *model_name, float score_thr, float nms_thr);
    std::list<dl::detect::result_t> &run(const dl::image::img_t &img) override;
    void get_last_input_range(int16_t *input_min, int16_t *input_max) const;
    void get_last_trace(ag_face_detector_trace *trace) const;
    void set_image_caps(uint32_t caps);
};

class MNP {
private:
    dl::Model *m_model;
    dl::image::ImagePreprocessor *m_image_preprocessor;
    dl::detect::MNPPostprocessor *m_postprocessor;

public:
    static inline constexpr float default_score_thr = 0.5;
    static inline constexpr float default_nms_thr = 0.5;
    MNP(const char *model_name, float score_thr, float nms_thr);
    ~MNP();
    MNP &set_score_thr(float score_thr);
    MNP &set_nms_thr(float nms_thr);
    dl::Model *get_raw_model();
    void set_image_caps(uint32_t caps);
    std::list<dl::detect::result_t> &run(const dl::image::img_t &img,
                                         std::list<dl::detect::result_t> &candidates,
                                         ag_face_detector_trace *trace);
};

class MSRMNP : public dl::detect::Detect {
private:
    MSR m_msr;
    MNP m_mnp;
    size_t m_last_candidate_count = 0;
    int16_t m_last_input_min = 0;
    int16_t m_last_input_max = 0;
    uint8_t m_last_score_percent = 0;
    ag_face_detector_trace m_last_trace = {};

public:
    MSRMNP(const char *msr_model_name,
           float msr_score_thr,
           float msr_nms_thr,
           const char *mnp_model_name,
           float mnp_score_thr,
           float mnp_nms_thr) :
        m_msr(msr_model_name, msr_score_thr, msr_nms_thr), m_mnp(mnp_model_name, mnp_score_thr, mnp_nms_thr)
    {
    }

    std::list<dl::detect::result_t> &run(const dl::image::img_t &img) override;
    Detect &set_score_thr(float score_thr, int idx) override;
    Detect &set_nms_thr(float nms_thr, int idx) override;
    dl::Model *get_raw_model(int idx) override;
    size_t get_last_candidate_count() const;
    void get_last_signal(int16_t *input_min, int16_t *input_max,
                         uint8_t *score_percent) const;
    void get_last_trace(ag_face_detector_trace *trace) const;
    void set_image_caps(uint32_t caps);
};

} // namespace human_face_detect

class HumanFaceDetect : public dl::detect::DetectWrapper {
public:
    typedef enum { MSRMNP_S8_V1 } model_type_t;

    HumanFaceDetect(model_type_t model_type = static_cast<model_type_t>(CONFIG_DEFAULT_HUMAN_FACE_DETECT_MODEL),
                    bool lazy_load = true);
    size_t get_last_msr_candidate_count() const;
    void get_last_msr_signal(int16_t *input_min, int16_t *input_max,
                             uint8_t *score_percent) const;
    void get_last_trace(ag_face_detector_trace *trace) const;
    void set_image_caps(uint32_t caps);

private:
    void load_model() override;

    model_type_t m_model_type;
};
