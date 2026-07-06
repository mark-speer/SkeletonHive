#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

/** Central drag-and-drop payload types for the device chain. */
namespace PluginDragTypes
{
inline constexpr const char* slotReorder   = "skeletonHivePluginSlot";
inline constexpr const char* browserInsert = "skeletonHivePluginBrowser";
inline constexpr const char* crossTrack    = "skeletonHivePluginCrossTrack";
} // namespace PluginDragTypes

struct PluginDragPayload
{
    enum class Kind { slotReorder, browserInsert, crossTrack, unknown };

    Kind kind = Kind::unknown;
    te::EditItemID pluginId;
    te::EditItemID sourceTrackId;
    te::EditItemID rackInstanceId;
    juce::String pluginIdentifier;

    static PluginDragPayload parse (const juce::var& description);
    juce::String encode() const;
};

} // namespace skeletonhive
