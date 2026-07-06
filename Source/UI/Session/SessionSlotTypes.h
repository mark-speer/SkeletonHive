#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

struct SessionSlotRowInfo
{
    te::Track::Ptr track;
    te::EditItemID trackId;
    int y = 0;
    int height = 0;
};

struct SessionSlotCellKey
{
    te::EditItemID trackId;
    int sceneIndex = 0;

    bool operator== (const SessionSlotCellKey& other) const noexcept
    {
        return trackId == other.trackId && sceneIndex == other.sceneIndex;
    }
};

} // namespace skeletonhive
