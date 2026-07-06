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
