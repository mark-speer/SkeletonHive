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

    // Arrangement timeline tokens
    static juce::Colour arrangementBackground (ThemeChoice theme);
    static juce::Colour laneBackground (ThemeChoice theme);
    static juce::Colour laneBackgroundAlt (ThemeChoice theme);
    static juce::Colour headerBackground (ThemeChoice theme);
    static juce::Colour footerBackground (ThemeChoice theme);
    static juce::Colour folderLaneBackground (ThemeChoice theme);
    static juce::Colour gridBarLine (ThemeChoice theme);
    static juce::Colour gridBeatLine (ThemeChoice theme);
    static juce::Colour gridSubdivisionLine (ThemeChoice theme);
    static juce::Colour clipSelectedBorder (ThemeChoice theme);
    static juce::Colour clipHoverBorder (ThemeChoice theme);
    static juce::Colour clipMutedOverlay (ThemeChoice theme);
    static juce::Colour playheadLine (ThemeChoice theme);
    static juce::Colour playheadGlow (ThemeChoice theme);
    static juce::Colour snapGuideLine (ThemeChoice theme);
    static juce::Colour trackSeparator (ThemeChoice theme);
    static juce::Colour rangeSelectionFill (ThemeChoice theme);
    static juce::Colour rangeSelectionBorder (ThemeChoice theme);
    static juce::Colour clipAudioDefault (ThemeChoice theme);
    static juce::Colour clipMidiDefault (ThemeChoice theme);
    static juce::Colour trackAccentAudio (ThemeChoice theme);
    static juce::Colour trackAccentMidi (ThemeChoice theme);
    static juce::Colour trackAccentFolder (ThemeChoice theme);
    static juce::Colour trackAccentReturn (ThemeChoice theme);
    static juce::Colour marqueeFill (ThemeChoice theme);
    static juce::Colour marqueeBorder (ThemeChoice theme);
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
