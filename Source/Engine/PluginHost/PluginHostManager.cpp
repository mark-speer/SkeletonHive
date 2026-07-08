#include "PluginHostManager.h"
#include "PluginHostHelpers.h"
#include "SandboxedPluginInstance.h"

namespace skeletonhive
{

void PluginHostManager::installCreatePluginInstanceHook (te::Engine& engine, AppSettings& settings)
{
    auto& pluginManager = engine.getPluginManager();

    pluginManager.createPluginInstance =
        [&engine, &settings, &pluginManager] (const juce::PluginDescription& desc,
                                                double sampleRate,
                                                int blockSize,
                                                juce::String& errorMessage) -> std::unique_ptr<juce::AudioPluginInstance>
    {
        if (PluginHostHelpers::shouldSandboxDescription (desc, settings))
            return SandboxedPluginInstance::create (engine, desc, sampleRate, blockSize, errorMessage);

        return pluginManager.pluginFormatManager.createPluginInstance (desc, sampleRate, blockSize, errorMessage);
    };
}

} // namespace skeletonhive
