#include "NativePluginCatalog.h"

#include "DrumRackHelpers.h"
#include "Effects/NativeCustomPlugins.h"
#include "Effects/SaturationPlugin.h"
#include "Effects/MultibandDynamicsPlugin.h"
#include "Effects/NamPlugin.h"

namespace skeletonhive
{

namespace
{

const juce::Array<NativePluginEntry>& builtInEntries()
{
    static juce::Array<NativePluginEntry> entries;

    if (entries.isEmpty())
    {
        entries.add ({ "Sampler", te::SamplerPlugin::xmlTypeName, "Instrument", true });
        entries.add ({ "Drum Rack", DrumRackHelpers::drumRackXmlTypeName, "Instrument", true });
        entries.add ({ "4OSC Synth", te::FourOscPlugin::xmlTypeName, "Instrument", true });
        entries.add ({ "EQ", te::EqualiserPlugin::xmlTypeName, "EQ", false });
        entries.add ({ "Compressor", te::CompressorPlugin::xmlTypeName, "Dynamics", false });
        entries.add ({ "Reverb", te::ReverbPlugin::xmlTypeName, "Time", false });
        entries.add ({ "Delay", te::DelayPlugin::xmlTypeName, "Time", false });
        entries.add ({ "Chorus", te::ChorusPlugin::xmlTypeName, "Modulation", false });
        entries.add ({ "Phaser", te::PhaserPlugin::xmlTypeName, "Modulation", false });
        entries.add ({ "Saturation", SaturationPlugin::xmlTypeName, "Distortion", false });
        entries.add ({ "Multiband Dynamics", MultibandDynamicsPlugin::xmlTypeName, "Dynamics", false });
        entries.add ({ "Neural Amp Modeler", NamPlugin::xmlTypeName, "Amp", false });
    }

    return entries;
}

const NativePluginEntry* findEntryByXmlType (const juce::String& xmlTypeName)
{
    for (const auto& entry : builtInEntries())
        if (entry.xmlTypeName != nullptr && xmlTypeName == entry.xmlTypeName)
            return &entry;

    return nullptr;
}

const NativePluginEntry* findEntryByIdentifier (const juce::String& identifierString)
{
    for (const auto& entry : builtInEntries())
    {
        const auto desc = NativePluginCatalog::makeDescription (entry);
        if (desc.createIdentifierString() == identifierString)
            return &entry;
    }

    return nullptr;
}

juce::String pluginXmlTypeName (const te::Plugin& plugin)
{
    if (auto* rack = dynamic_cast<const te::RackInstance*> (&plugin))
        if (rack->type != nullptr && DrumRackHelpers::isDrumRack (*rack->type))
            return DrumRackHelpers::drumRackXmlTypeName;

    if (dynamic_cast<const te::SamplerPlugin*> (&plugin) != nullptr)
        return te::SamplerPlugin::xmlTypeName;
    if (dynamic_cast<const te::FourOscPlugin*> (&plugin) != nullptr)
        return te::FourOscPlugin::xmlTypeName;
    if (dynamic_cast<const te::EqualiserPlugin*> (&plugin) != nullptr)
        return te::EqualiserPlugin::xmlTypeName;
    if (dynamic_cast<const te::CompressorPlugin*> (&plugin) != nullptr)
        return te::CompressorPlugin::xmlTypeName;
    if (dynamic_cast<const te::ReverbPlugin*> (&plugin) != nullptr)
        return te::ReverbPlugin::xmlTypeName;
    if (dynamic_cast<const te::DelayPlugin*> (&plugin) != nullptr)
        return te::DelayPlugin::xmlTypeName;
    if (dynamic_cast<const te::ChorusPlugin*> (&plugin) != nullptr)
        return te::ChorusPlugin::xmlTypeName;
    if (dynamic_cast<const te::PhaserPlugin*> (&plugin) != nullptr)
        return te::PhaserPlugin::xmlTypeName;
    if (isSaturationPlugin (plugin))
        return SaturationPlugin::xmlTypeName;
    if (isMultibandDynamicsPlugin (plugin))
        return MultibandDynamicsPlugin::xmlTypeName;
    if (isNamPlugin (plugin))
        return NamPlugin::xmlTypeName;

    return {};
}

} // namespace

const juce::Array<NativePluginEntry>& NativePluginCatalog::getEntries()
{
    return builtInEntries();
}

juce::PluginDescription NativePluginCatalog::makeDescription (const NativePluginEntry& entry)
{
    juce::PluginDescription desc;
    desc.name = entry.displayName != nullptr ? juce::String (entry.displayName) : juce::String {};
    desc.descriptiveName = desc.name;
    desc.pluginFormatName = "Native";
    desc.manufacturerName = "SkeletonHive";
    desc.category = entry.category != nullptr ? juce::String (entry.category) : juce::String {};
    desc.isInstrument = entry.isInstrument;
    desc.numInputChannels = entry.isInstrument ? 0 : 2;
    desc.numOutputChannels = 2;

    if (entry.xmlTypeName != nullptr)
        desc.fileOrIdentifier = juce::String (identifierPrefix) + entry.xmlTypeName;

    return desc;
}

juce::Array<juce::PluginDescription> NativePluginCatalog::getAllDescriptions()
{
    juce::Array<juce::PluginDescription> result;

    for (const auto& entry : getEntries())
        result.add (makeDescription (entry));

    return result;
}

bool NativePluginCatalog::isNativeDescription (const juce::PluginDescription& desc)
{
    return desc.fileOrIdentifier.startsWith (identifierPrefix)
        || findEntryByIdentifier (desc.createIdentifierString()) != nullptr;
}

juce::String NativePluginCatalog::xmlTypeNameFromDescription (const juce::PluginDescription& desc)
{
    if (desc.fileOrIdentifier.startsWith (identifierPrefix))
        return desc.fileOrIdentifier.fromFirstOccurrenceOf (identifierPrefix, false, false);

    if (const auto* entry = findEntryByIdentifier (desc.createIdentifierString()))
        return entry->xmlTypeName != nullptr ? juce::String (entry->xmlTypeName) : juce::String {};

    return {};
}

juce::PluginDescription NativePluginCatalog::lookupDescription (const juce::String& identifierString)
{
    if (const auto* entry = findEntryByIdentifier (identifierString))
        return makeDescription (*entry);

    return {};
}

juce::PluginDescription NativePluginCatalog::descriptionForPlugin (const te::Plugin& plugin)
{
    if (const auto* entry = findEntryByXmlType (pluginXmlTypeName (plugin)))
        return makeDescription (*entry);

    return {};
}

te::Plugin::Ptr NativePluginCatalog::createPlugin (te::Edit& edit, const juce::PluginDescription& desc)
{
    const auto xmlTypeName = xmlTypeNameFromDescription (desc);

    if (xmlTypeName.isEmpty())
        return {};

    if (xmlTypeName == DrumRackHelpers::drumRackXmlTypeName)
        return DrumRackHelpers::createDrumRack (edit);

    return edit.getPluginCache().createNewPlugin (xmlTypeName, {});
}

bool NativePluginCatalog::isNativeInstrumentPlugin (const te::Plugin& plugin)
{
    if (auto* rack = dynamic_cast<const te::RackInstance*> (&plugin))
        return DrumRackHelpers::isDrumRack (*rack);

    return dynamic_cast<const te::SamplerPlugin*> (&plugin) != nullptr
        || dynamic_cast<const te::FourOscPlugin*> (&plugin) != nullptr;
}

bool NativePluginCatalog::isNativePlugin (const te::Plugin& plugin)
{
    if (auto* rack = dynamic_cast<const te::RackInstance*> (&plugin))
        return DrumRackHelpers::isDrumRack (*rack);

    return findEntryByXmlType (pluginXmlTypeName (plugin)) != nullptr;
}

} // namespace skeletonhive
