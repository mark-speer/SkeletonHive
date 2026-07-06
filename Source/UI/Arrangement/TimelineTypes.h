#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

struct TrackRowInfo
{
    te::Track::Ptr track;
    int y = 0;
    int height = 0;
};

} // namespace skeletonhive
