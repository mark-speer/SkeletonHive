#pragma once

#include "TracktionCommon.h"

namespace arrange
{

struct TrackRowInfo
{
    te::Track::Ptr track;
    int y = 0;
    int height = 0;
};

} // namespace arrange
