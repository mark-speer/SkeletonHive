#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

namespace ContentDragTypes
{
inline constexpr const char* sampleInsert = "skeletonHiveContentSample";
inline constexpr const char* clipExport = "skeletonHiveClipExport";
inline constexpr const char* clipPreset = "skeletonHiveClipPreset";
} // namespace ContentDragTypes

struct ContentDragPayload
{
    juce::File file;

    static ContentDragPayload parse (const juce::var& description);
    juce::String encode() const;
    bool isValid() const { return file.existsAsFile(); }
};

struct ClipExportDragPayload
{
    juce::int64 clipItemId = 0;

    static ClipExportDragPayload parse (const juce::var& description);
    juce::String encode() const;
    bool isValid() const { return clipItemId != 0; }
};

struct ClipPresetDragPayload
{
    juce::File presetFile;

    static ClipPresetDragPayload parse (const juce::var& description);
    juce::String encode() const;
    bool isValid() const { return presetFile.existsAsFile(); }
};

} // namespace skeletonhive
