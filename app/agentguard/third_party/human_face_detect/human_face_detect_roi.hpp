/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "agentguard/face_diagnostics.h"
#include "dl_detect_define.hpp"

#include <algorithm>
#include <cstring>
#include <list>

namespace human_face_detect {

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
