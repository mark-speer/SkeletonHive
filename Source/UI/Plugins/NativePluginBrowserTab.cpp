#include "NativePluginBrowserTab.h"

namespace skeletonhive
{

void NativePluginBrowserTab::paintListRowBadge (const juce::PluginDescription& desc,
                                                juce::Graphics& g,
                                                int width,
                                                int height)
{
    if (! NativePluginCatalog::isNativeDescription (desc))
        return;

    g.setColour (juce::Colour (0xff1b4332).withAlpha (0.85f));
    g.fillRoundedRectangle ((float) width - (float) badgeWidth(), 4.0f,
                            (float) badgeWidth() - 8.0f, (float) height - 8.0f, 3.0f);
    g.setColour (juce::Colours::white);
    g.drawText ("NATIVE", width - badgeWidth(), 0, badgeWidth() - 8, height,
                juce::Justification::centred, false);
}

} // namespace skeletonhive
