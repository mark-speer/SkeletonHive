#pragma once

#include "Engine/NativePluginCatalog.h"

namespace skeletonhive
{

/** Row paint helpers for native plugins in PluginBrowser / PluginPickerDialog. */
class NativePluginBrowserTab
{
public:
    static void paintListRowBadge (const juce::PluginDescription& desc,
                                   juce::Graphics& g,
                                   int width,
                                   int height);

    static int badgeWidth() { return 56; }
};

} // namespace skeletonhive
