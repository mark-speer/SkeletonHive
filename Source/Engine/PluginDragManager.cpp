#include "PluginDragManager.h"

namespace skeletonhive
{

PluginDragPayload PluginDragPayload::parse (const juce::var& description)
{
    PluginDragPayload payload;
    const auto text = description.toString();

    if (text.startsWith (PluginDragTypes::crossTrack))
    {
        payload.kind = Kind::crossTrack;
        const auto body = text.fromFirstOccurrenceOf (":", false, false);
        const auto parts = juce::StringArray::fromTokens (body, ":", {});
        if (parts.size() >= 2)
        {
            payload.sourceTrackId = te::EditItemID::fromVar (parts[0]);
            payload.pluginId = te::EditItemID::fromVar (parts[1]);
        }
        return payload;
    }

    if (text.startsWith (PluginDragTypes::slotReorder))
    {
        payload.kind = Kind::slotReorder;
        const auto body = text.fromFirstOccurrenceOf (":", false, false);
        const auto parts = juce::StringArray::fromTokens (body, ":", {});
        if (parts.size() >= 1)
            payload.pluginId = te::EditItemID::fromVar (parts[0]);
        if (parts.size() >= 2)
            payload.rackInstanceId = te::EditItemID::fromVar (parts[1]);
        return payload;
    }

    if (text.startsWith (PluginDragTypes::browserInsert))
    {
        payload.kind = Kind::browserInsert;
        payload.pluginIdentifier = text.fromFirstOccurrenceOf (":", false, false);
        return payload;
    }

    return payload;
}

juce::String PluginDragPayload::encode() const
{
    switch (kind)
    {
        case Kind::crossTrack:
            return juce::String (PluginDragTypes::crossTrack) + ":"
                 + sourceTrackId.toVar().toString() + ":" + pluginId.toVar().toString();
        case Kind::slotReorder:
        {
            auto encoded = juce::String (PluginDragTypes::slotReorder) + ":" + pluginId.toVar().toString();
            if (rackInstanceId.isValid())
                encoded += ":" + rackInstanceId.toVar().toString();
            return encoded;
        }
        case Kind::browserInsert:
            return juce::String (PluginDragTypes::browserInsert) + ":" + pluginIdentifier;
        default:
            return {};
    }
}

} // namespace skeletonhive
