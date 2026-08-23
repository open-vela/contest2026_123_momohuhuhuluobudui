/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "dl_detect_define.hpp"

namespace human_face_detect {

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
