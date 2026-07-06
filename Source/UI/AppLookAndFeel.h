#pragma once

#include "Engine/AppSettings.h"
#include <JuceHeader.h>

namespace skeletonhive
{

/** Shared colour tokens for SkeletonHive UI. */
struct AppColours
{
    static juce::Colour panelBackground (ThemeChoice theme);
    static juce::Colour surfaceBackground (ThemeChoice theme);
    static juce::Colour accentLoop (ThemeChoice theme);
    static juce::Colour accentTempo (ThemeChoice theme);
    static juce::Colour accentTimeSig (ThemeChoice theme);
    static juce::Colour accentValidDrop (ThemeChoice theme);
    static juce::Colour accentInvalidDrop (ThemeChoice theme);
    static juce::Colour accentFrozen (ThemeChoice theme);
    static juce::Colour pluginTrayBackground (ThemeChoice theme);
    static juce::Colour automationPanelBackground (ThemeChoice theme);
    static juce::Colour pluginSlotInstrument (ThemeChoice theme);
    static juce::Colour pluginSlotEffect (ThemeChoice theme);
    static juce::Colour pluginSlotFailed (ThemeChoice theme);
    static juce::Colour pluginSlotLoading (ThemeChoice theme);
    static juce::Colour pluginSlotBypassed (ThemeChoice theme);
    static juce::Colour pluginOutputNode (ThemeChoice theme);
    static juce::Colour clipGroupPalette (int index);
};

/** Custom LookAndFeel with dark/light palettes. */
class AppLookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit AppLookAndFeel (ThemeChoice theme = ThemeChoice::dark);

    void setTheme (ThemeChoice theme);
    ThemeChoice getTheme() const { return currentTheme; }

    void applyThemeToDesktop();

    static ThemeChoice getCurrentTheme();

private:
    void applyPalette();

    ThemeChoice currentTheme;
};

} // namespace skeletonhive
