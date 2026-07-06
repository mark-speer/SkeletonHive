#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

struct TrackRowInfo
{
    te::Track::Ptr track;
    int y = 0;
    int height = 0;
    int takeLaneExtraHeight = 0;
};

static constexpr int compLaneStripHeight = 26;
static constexpr int takeLaneStripHeight = 22;

} // namespace skeletonhive
