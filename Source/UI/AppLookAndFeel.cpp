#include "AppLookAndFeel.h"
#include "Engine/AppSettings.h"

namespace skeletonhive
{

namespace
{
juce::Colour darkOrLight (ThemeChoice theme, juce::Colour dark, juce::Colour light)
{
    return theme == ThemeChoice::light ? light : dark;
}
} // namespace

juce::Colour AppColours::panelBackground (ThemeChoice theme)
{
    return darkOrLight (theme, juce::Colour (0xff1b263b), juce::Colour (0xfff0f2f5));
}

juce::Colour AppColours::surfaceBackground (ThemeChoice theme)
{
    return darkOrLight (theme, juce::Colour (0xff0d1b2a), juce::Colour (0xffffffff));
}

juce::Colour AppColours::accentLoop (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xffffd166);
}

juce::Colour AppColours::accentTempo (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xffef8354);
}

juce::Colour AppColours::accentTimeSig (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xff4cc9f0);
}

juce::Colour AppColours::accentValidDrop (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xff06d6a0);
}

juce::Colour AppColours::accentInvalidDrop (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xffef476f);
}

juce::Colour AppColours::accentFrozen (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xff4cc9f0);
}

juce::Colour AppColours::pluginTrayBackground (ThemeChoice theme)
{
    return darkOrLight (theme, juce::Colour (0xff0d1b2a), juce::Colour (0xffe8edf5));
}

juce::Colour AppColours::automationPanelBackground (ThemeChoice theme)
{
    return darkOrLight (theme, juce::Colour (0xff101a33), juce::Colour (0xfff5f7fb));
}

juce::Colour AppColours::pluginSlotInstrument (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xff5a189a);
}

juce::Colour AppColours::pluginSlotEffect (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xff1d3557);
}

juce::Colour AppColours::pluginSlotFailed (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xff9b2226);
}

juce::Colour AppColours::pluginSlotLoading (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xffca6702);
}

juce::Colour AppColours::pluginSlotBypassed (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xff4a4e69);
}

juce::Colour AppColours::pluginOutputNode (ThemeChoice theme)
{
    return darkOrLight (theme, juce::Colour (0xff2b2d42), juce::Colour (0xffd8dee9));
}

juce::Colour AppColours::clipGroupPalette (int index)
{
    static const juce::Colour palette[] {
        juce::Colour (0xffffd166), juce::Colour (0xff06d6a0), juce::Colour (0xff118ab2),
        juce::Colour (0xffef476f), juce::Colour (0xffc77dff), juce::Colour (0xfff4a261),
    };
    return palette[index % (int) std::size (palette)];
}

juce::Colour AppColours::arrangementBackground (ThemeChoice theme)
{
    return darkOrLight (theme, juce::Colour (0xff12121f), juce::Colour (0xffe8eaef));
}

juce::Colour AppColours::laneBackground (ThemeChoice theme)
{
    return darkOrLight (theme, juce::Colour (0xff0f0f23), juce::Colour (0xfff5f6fa));
}

juce::Colour AppColours::laneBackgroundAlt (ThemeChoice theme)
{
    return darkOrLight (theme, juce::Colour (0xff121228), juce::Colour (0xffeceef4));
}

juce::Colour AppColours::headerBackground (ThemeChoice theme)
{
    return darkOrLight (theme, juce::Colour (0xff1a1a2e), juce::Colour (0xffe2e6ee));
}

juce::Colour AppColours::footerBackground (ThemeChoice theme)
{
    return darkOrLight (theme, juce::Colour (0xff16213e), juce::Colour (0xffd8dee9));
}

juce::Colour AppColours::folderLaneBackground (ThemeChoice theme)
{
    return darkOrLight (theme, juce::Colour (0xff2a1a3e), juce::Colour (0xffddd0e8));
}

juce::Colour AppColours::gridBarLine (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colours::white.withAlpha (0.38f);
}

juce::Colour AppColours::gridBeatLine (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colours::white.withAlpha (0.16f);
}

juce::Colour AppColours::gridSubdivisionLine (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colours::white.withAlpha (0.07f);
}

juce::Colour AppColours::clipSelectedBorder (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xffffd166);
}

juce::Colour AppColours::clipHoverBorder (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colours::white.withAlpha (0.45f);
}

juce::Colour AppColours::clipMutedOverlay (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colours::black.withAlpha (0.42f);
}

juce::Colour AppColours::playheadLine (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xffff4d6d);
}

juce::Colour AppColours::playheadGlow (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xffff4d6d);
}

juce::Colour AppColours::snapGuideLine (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xff4cc9f0);
}

juce::Colour AppColours::trackSeparator (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colours::white.withAlpha (0.10f);
}

juce::Colour AppColours::rangeSelectionFill (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xff4361ee).withAlpha (0.28f);
}

juce::Colour AppColours::rangeSelectionBorder (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xff4361ee).withAlpha (0.85f);
}

juce::Colour AppColours::clipAudioDefault (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xff357a5c);
}

juce::Colour AppColours::clipMidiDefault (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xff4a5fd4);
}

juce::Colour AppColours::trackAccentAudio (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xff2d6a4f);
}

juce::Colour AppColours::trackAccentMidi (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xff4361ee);
}

juce::Colour AppColours::trackAccentFolder (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xff7209b7);
}

juce::Colour AppColours::trackAccentReturn (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xffe63946);
}

juce::Colour AppColours::monitorOff (ThemeChoice theme)
{
    return darkOrLight (theme, juce::Colour (0xff3a3a4a), juce::Colour (0xffb0b5c0));
}

juce::Colour AppColours::monitorAuto (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xffca6702);
}

juce::Colour AppColours::monitorIn (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xff06d6a0);
}

juce::Colour AppColours::armActive (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xffe63946);
}

juce::Colour AppColours::marqueeFill (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xffffd166).withAlpha (0.12f);
}

juce::Colour AppColours::marqueeBorder (ThemeChoice theme)
{
    juce::ignoreUnused (theme);
    return juce::Colour (0xffffd166).withAlpha (0.75f);
}

AppLookAndFeel::AppLookAndFeel (ThemeChoice theme)
    : currentTheme (theme)
{
    applyPalette();
}

void AppLookAndFeel::setTheme (ThemeChoice theme)
{
    if (theme == currentTheme)
        return;

    currentTheme = theme;
    applyPalette();
    applyThemeToDesktop();
}

void AppLookAndFeel::applyThemeToDesktop()
{
    juce::Desktop::getInstance().setDefaultLookAndFeel (this);
}

ThemeChoice AppLookAndFeel::getCurrentTheme()
{
    if (auto* laf = dynamic_cast<AppLookAndFeel*> (&juce::LookAndFeel::getDefaultLookAndFeel()))
        return laf->getTheme();

    return ThemeChoice::dark;
}

void AppLookAndFeel::applyPalette()
{
    if (currentTheme == ThemeChoice::light)
    {
        setColour (juce::ResizableWindow::backgroundColourId, AppColours::panelBackground (currentTheme));
        setColour (juce::DocumentWindow::backgroundColourId, AppColours::panelBackground (currentTheme));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xffd8dee9));
        setColour (juce::TextButton::textColourOffId, juce::Colour (0xff1b263b));
        setColour (juce::Label::textColourId, juce::Colour (0xff1b263b));
        setColour (juce::ComboBox::backgroundColourId, juce::Colours::white);
        setColour (juce::Slider::backgroundColourId, juce::Colour (0xffe8edf5));
        setColour (juce::Slider::trackColourId, juce::Colour (0xff118ab2));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colours::white);
        setColour (juce::PopupMenu::textColourId, juce::Colour (0xff1b263b));
        setColour (juce::TabbedComponent::backgroundColourId, AppColours::panelBackground (currentTheme));
        setColour (juce::TabbedComponent::outlineColourId, juce::Colour (0xffc0c8d4));
    }
    else
    {
        setColour (juce::ResizableWindow::backgroundColourId, AppColours::panelBackground (currentTheme));
        setColour (juce::DocumentWindow::backgroundColourId, AppColours::panelBackground (currentTheme));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b2d42));
        setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        setColour (juce::Label::textColourId, juce::Colours::white);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1b263b));
        setColour (juce::Slider::backgroundColourId, juce::Colour (0xff1b263b));
        setColour (juce::Slider::trackColourId, juce::Colour (0xff118ab2));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1b263b));
        setColour (juce::PopupMenu::textColourId, juce::Colours::white);
        setColour (juce::TabbedComponent::backgroundColourId, AppColours::panelBackground (currentTheme));
        setColour (juce::TabbedComponent::outlineColourId, juce::Colour (0xff415a77));
    }
}

} // namespace skeletonhive
