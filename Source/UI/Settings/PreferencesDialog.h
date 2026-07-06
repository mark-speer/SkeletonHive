#pragma once

#include "Engine/AppSettings.h"
#include "Engine/AppCommands.h"
#include "Engine/PluginScanner.h"
#include "Engine/PluginStateManager.h"
#include "UI/AppLookAndFeel.h"

namespace skeletonhive
{

class PreferencesDialog : public juce::Component
{
public:
    PreferencesDialog (AppSettings& settings,
                       AppLookAndFeel& lookAndFeel,
                       te::Engine& engine,
                       PluginScanner& pluginScanner,
                       PluginStateManager& pluginStateManager,
                       juce::ApplicationCommandManager& commandManager,
                       std::function<void (int autosaveSeconds)> onAutosaveChanged,
                       std::function<void()> onLibraryPathsChanged = {});

    void paint (juce::Graphics& g) override;
    void resized() override;

    static void show (juce::Component* centreAround,
                      AppSettings& settings,
                      AppLookAndFeel& lookAndFeel,
                      te::Engine& engine,
                      PluginScanner& pluginScanner,
                      PluginStateManager& pluginStateManager,
                      juce::ApplicationCommandManager& commandManager,
                      std::function<void (int autosaveSeconds)> onAutosaveChanged,
                      std::function<void()> onLibraryPathsChanged = {});

private:
    class GeneralPage;
    class AppearancePage;
    class KeyboardPage;
    class AudioPage;
    class LibraryPage;
    class DevicesPage;

    AppSettings& appSettings;
    AppLookAndFeel& appLookAndFeel;
    te::Engine& engineRef;
    PluginScanner& pluginScannerRef;
    PluginStateManager& pluginStateManagerRef;
    juce::ApplicationCommandManager& commandManagerRef;
    std::function<void (int autosaveSeconds)> autosaveCallback;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    std::unique_ptr<GeneralPage> generalPage;
    std::unique_ptr<AppearancePage> appearancePage;
    std::unique_ptr<KeyboardPage> keyboardPage;
    std::unique_ptr<AudioPage> audioPage;
    std::unique_ptr<LibraryPage> libraryPage;
    std::unique_ptr<DevicesPage> devicesPage;
};

} // namespace skeletonhive
