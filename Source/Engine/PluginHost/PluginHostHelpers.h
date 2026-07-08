#pragma once

#include "Engine/AppSettings.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

struct PluginHostHelpers
{
    static bool shouldSandboxDescription (const juce::PluginDescription& desc, const AppSettings& settings);
    static juce::String makeSessionId();
    static bool isSandboxedExternalPlugin (const te::Plugin& plugin);
};

} // namespace skeletonhive
