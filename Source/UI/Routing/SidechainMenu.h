#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

/** Shared sidechain context-menu helpers (tray + track footer). */
struct SidechainMenu
{
    static constexpr int openMatrixId = 198;
    static constexpr int sidechainBase = 200;

    static void addSidechainMenuItems (juce::PopupMenu& menu, te::Plugin& plugin);
    static bool handleSidechainMenuResult (int result,
                                           te::Plugin& plugin,
                                           int moveToRackBase,
                                           std::function<void()> onChanged);
};

} // namespace skeletonhive
