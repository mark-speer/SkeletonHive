#pragma once

#include "TracktionCommon.h"

namespace arrange
{

/** Central drag-and-drop payload types for the device chain. */
namespace PluginDragTypes
{
inline constexpr const char* slotReorder   = "arrangePluginSlot";
inline constexpr const char* browserInsert = "arrangePluginBrowser";
inline constexpr const char* crossTrack    = "arrangePluginCrossTrack";
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

} // namespace arrange
