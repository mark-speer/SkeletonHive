#include "ContentDragManager.h"

namespace skeletonhive
{

ContentDragPayload ContentDragPayload::parse (const juce::var& description)
{
    ContentDragPayload payload;
    const auto text = description.toString();

    if (! text.startsWith (ContentDragTypes::sampleInsert))
        return payload;

    payload.file = juce::File (text.fromFirstOccurrenceOf (":", false, false));
    return payload;
}

juce::String ContentDragPayload::encode() const
{
    return juce::String (ContentDragTypes::sampleInsert) + ":" + file.getFullPathName();
}

ClipExportDragPayload ClipExportDragPayload::parse (const juce::var& description)
{
    ClipExportDragPayload payload;
    const auto text = description.toString();

    if (! text.startsWith (ContentDragTypes::clipExport))
        return payload;

    payload.clipItemId = text.fromFirstOccurrenceOf (":", false, false).getLargeIntValue();
    return payload;
}

juce::String ClipExportDragPayload::encode() const
{
    return juce::String (ContentDragTypes::clipExport) + ":" + juce::String (clipItemId);
}

ClipPresetDragPayload ClipPresetDragPayload::parse (const juce::var& description)
{
    ClipPresetDragPayload payload;
    const auto text = description.toString();

    if (! text.startsWith (ContentDragTypes::clipPreset))
        return payload;

    payload.presetFile = juce::File (text.fromFirstOccurrenceOf (":", false, false));
    return payload;
}

juce::String ClipPresetDragPayload::encode() const
{
    return juce::String (ContentDragTypes::clipPreset) + ":" + presetFile.getFullPathName();
}

} // namespace skeletonhive
