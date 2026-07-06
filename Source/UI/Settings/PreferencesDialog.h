#pragma once

#include "Engine/AppSettings.h"
#include "Engine/AppCommands.h"
#include "UI/AppLookAndFeel.h"

namespace skeletonhive
{

class PreferencesDialog : public juce::Component
{
public:
    PreferencesDialog (AppSettings& settings,
                       AppLookAndFeel& lookAndFeel,
                       te::Engine& engine,
                       juce::ApplicationCommandManager& commandManager,
                       std::function<void (int autosaveSeconds)> onAutosaveChanged);

    void paint (juce::Graphics& g) override;
    void resized() override;

    static void show (juce::Component* centreAround,
                      AppSettings& settings,
                      AppLookAndFeel& lookAndFeel,
                      te::Engine& engine,
                      juce::ApplicationCommandManager& commandManager,
                      std::function<void (int autosaveSeconds)> onAutosaveChanged);

private:
    class GeneralPage;
    class AppearancePage;
    class KeyboardPage;
    class AudioPage;

    AppSettings& appSettings;
    AppLookAndFeel& appLookAndFeel;
    te::Engine& engineRef;
    juce::ApplicationCommandManager& commandManagerRef;
    std::function<void (int autosaveSeconds)> autosaveCallback;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    std::unique_ptr<GeneralPage> generalPage;
    std::unique_ptr<AppearancePage> appearancePage;
    std::unique_ptr<KeyboardPage> keyboardPage;
    std::unique_ptr<AudioPage> audioPage;
};

} // namespace skeletonhive
