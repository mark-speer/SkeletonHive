#include "PluginHostHelpers.h"
#include "SandboxedPluginInstance.h"

namespace skeletonhive
{

bool PluginHostHelpers::shouldSandboxDescription (const juce::PluginDescription& desc, const AppSettings& settings)
{
    if (! settings.isPluginSandboxEnabled())
        return false;

    if (desc.isInstrument)
        return false;

    // ARA needs Edit document / musical-context binding; the VST3 effect sandbox
    // cannot host that IPC. Always load ARA-capable plugins in-process.
    if (desc.hasARAExtension)
        return false;

    return desc.pluginFormatName == "VST3";
}

juce::String PluginHostHelpers::makeSessionId()
{
    return juce::Uuid().toString();
}

bool PluginHostHelpers::isSandboxedExternalPlugin (const te::Plugin& plugin)
{
    if (auto* external = dynamic_cast<const te::ExternalPlugin*> (&plugin))
        if (auto* instance = external->getAudioPluginInstance())
            return dynamic_cast<const SandboxedPluginInstance*> (instance) != nullptr;

    return false;
}

} // namespace skeletonhive
