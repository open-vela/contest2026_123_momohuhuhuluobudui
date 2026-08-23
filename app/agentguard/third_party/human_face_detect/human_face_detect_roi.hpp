/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "agentguard/face_diagnostics.h"
#include "dl_detect_define.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <list>
#include <vector>

namespace human_face_detect {

inline std::vector<int> full_frame_crop(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return {};
    }

    return {0, 0, width, height};
}

inline int32_t scale_to_millionths(float scale)
{
    double scaled = static_cast<double>(scale) * 1000000.0;
    scaled = std::max(static_cast<double>(std::numeric_limits<int32_t>::min()),
                      std::min(static_cast<double>(std::numeric_limits<int32_t>::max()),
                               scaled));
    return static_cast<int32_t>(scaled);
}

inline bool capture_resize_scale(float scale_x, float scale_y,
                                 float inv_scale_x, float inv_scale_y,
                                 ag_resize_scale_diagnostic *diagnostic)
{
    if (diagnostic == nullptr) {
        return false;
    }

    std::memset(diagnostic, 0, sizeof(*diagnostic));
    if (!std::isfinite(scale_x) || !std::isfinite(scale_y) ||
        !std::isfinite(inv_scale_x) || !std::isfinite(inv_scale_y)) {
        return false;
    }

    diagnostic->x_millionths = scale_to_millionths(scale_x);
    diagnostic->y_millionths = scale_to_millionths(scale_y);
    diagnostic->inv_x_millionths = scale_to_millionths(inv_scale_x);
    diagnostic->inv_y_millionths = scale_to_millionths(inv_scale_y);
    diagnostic->valid = true;
    return true;
}

inline bool capture_candidate(
    const dl::detect::result_t &candidate,
    ag_face_candidate_diagnostic *diagnostic)
{
    if (diagnostic == nullptr) {
        return false;
    }

    std::memset(diagnostic, 0, sizeof(*diagnostic));
    if (candidate.box.size() < 4) {
        return false;
    }

    diagnostic->left = candidate.box[0];
    diagnostic->top = candidate.box[1];
    diagnostic->right = candidate.box[2];
    diagnostic->bottom = candidate.box[3];
    float score = std::max(0.0f, std::min(1.0f, candidate.score));
    diagnostic->score_permille =
        static_cast<uint16_t>(score * 1000.0f + 0.5f);
    diagnostic->valid = true;
    return true;
}

inline bool capture_top_candidate(
    const std::list<dl::detect::result_t> &candidates,
    ag_face_candidate_diagnostic *diagnostic)
{
    if (candidates.empty()) {
        if (diagnostic != nullptr) {
            std::memset(diagnostic, 0, sizeof(*diagnostic));
        }
        return false;
    }

    return capture_candidate(candidates.front(), diagnostic);
}

inline bool prepare_candidate_roi(dl::detect::result_t &candidate,
                                  int width, int height)
{
    if (candidate.box.size() < 4 || width <= 0 || height <= 0) {
        return false;
    }

    int center_x = (candidate.box[0] + candidate.box[2]) >> 1;
    int center_y = (candidate.box[1] + candidate.box[3]) >> 1;
    int side = DL_MAX(candidate.box[2] - candidate.box[0],
                      candidate.box[3] - candidate.box[1]);
    candidate.box[0] = center_x - (side >> 1);
    candidate.box[1] = center_y - (side >> 1);
    candidate.box[2] = candidate.box[0] + side;
    candidate.box[3] = candidate.box[1] + side;
    candidate.limit_box(width, height);

    return candidate.box[2] > candidate.box[0] &&
           candidate.box[3] > candidate.box[1];
}

} // namespace human_face_detect
