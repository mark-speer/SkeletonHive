#include "PreferencesDialog.h"

namespace skeletonhive
{

class PreferencesDialog::GeneralPage : public juce::Component
{
public:
    GeneralPage (AppSettings& settings, std::function<void (int)> onAutosaveChanged)
        : appSettings (settings), autosaveChanged (std::move (onAutosaveChanged))
    {
        folderLabel.setText ("Default project folder:", juce::dontSendNotification);
        autosaveLabel.setText ("Autosave interval (seconds):", juce::dontSendNotification);

        folderEditor.setText (appSettings.getDefaultProjectFolder().getFullPathName(), juce::dontSendNotification);
        browseButton.onClick = [this] { browseForFolder(); };

        autosaveEditor.setInputRestrictions (4, "0123456789");
        autosaveEditor.setText (juce::String (appSettings.getAutosaveIntervalSeconds()), juce::dontSendNotification);
        autosaveEditor.onReturnKey = [this] { applyAutosave(); };
        autosaveEditor.onFocusLost = [this] { applyAutosave(); };

        addAndMakeVisible (folderLabel);
        addAndMakeVisible (folderEditor);
        addAndMakeVisible (browseButton);
        addAndMakeVisible (autosaveLabel);
        addAndMakeVisible (autosaveEditor);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        folderLabel.setBounds (r.removeFromTop (20));
        auto folderRow = r.removeFromTop (28);
        browseButton.setBounds (folderRow.removeFromRight (80));
        folderEditor.setBounds (folderRow);
        r.removeFromTop (12);
        autosaveLabel.setBounds (r.removeFromTop (20));
        autosaveEditor.setBounds (r.removeFromTop (28).removeFromLeft (120));
    }

private:
    void browseForFolder()
    {
        auto fc = std::make_shared<juce::FileChooser> ("Choose default project folder",
                                                         appSettings.getDefaultProjectFolder());
        fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                         [this, fc] (const juce::FileChooser&)
                         {
                             const auto f = fc->getResult();
                             if (f.isDirectory())
                             {
                                 appSettings.setDefaultProjectFolder (f);
                                 folderEditor.setText (f.getFullPathName(), juce::dontSendNotification);
                             }
                         });
    }

    void applyAutosave()
    {
        const int seconds = autosaveEditor.getText().getIntValue();
        appSettings.setAutosaveIntervalSeconds (seconds);

        if (autosaveChanged)
            autosaveChanged (appSettings.getAutosaveIntervalSeconds());
    }

    AppSettings& appSettings;
    std::function<void (int)> autosaveChanged;
    juce::Label folderLabel, autosaveLabel;
    juce::TextEditor folderEditor;
    juce::TextButton browseButton { "Browse..." };
    juce::TextEditor autosaveEditor;
};

class PreferencesDialog::AppearancePage : public juce::Component
{
public:
    AppearancePage (AppSettings& settings, AppLookAndFeel& lookAndFeel)
        : appSettings (settings), appLookAndFeel (lookAndFeel)
    {
        themeLabel.setText ("Theme:", juce::dontSendNotification);
        themeBox.addItem ("Dark", 1);
        themeBox.addItem ("Light", 2);
        themeBox.setSelectedId (appSettings.getTheme() == ThemeChoice::light ? 2 : 1, juce::dontSendNotification);
        themeBox.onChange = [this]
        {
            appSettings.setTheme (themeBox.getSelectedId() == 2 ? ThemeChoice::light : ThemeChoice::dark);
            appLookAndFeel.setTheme (appSettings.getTheme());
        };

        addAndMakeVisible (themeLabel);
        addAndMakeVisible (themeBox);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        themeLabel.setBounds (r.removeFromTop (20));
        themeBox.setBounds (r.removeFromTop (28).removeFromLeft (160));
    }

private:
    AppSettings& appSettings;
    AppLookAndFeel& appLookAndFeel;
    juce::Label themeLabel;
    juce::ComboBox themeBox;
};

class PreferencesDialog::KeyboardPage : public juce::Component
{
public:
    KeyboardPage (AppSettings& settings, juce::ApplicationCommandManager& commandManager)
        : appSettings (settings), commandManagerRef (commandManager)
    {
        if (auto* mappings = commandManagerRef.getKeyMappings())
        {
            editor = std::make_unique<juce::KeyMappingEditorComponent> (*mappings, true);
            addAndMakeVisible (*editor);
        }
    }

    ~KeyboardPage() override
    {
        appSettings.saveKeyMappings (commandManagerRef);
    }

    void resized() override
    {
        if (editor != nullptr)
            editor->setBounds (getLocalBounds().reduced (8));
    }

private:
    AppSettings& appSettings;
    juce::ApplicationCommandManager& commandManagerRef;
    std::unique_ptr<juce::KeyMappingEditorComponent> editor;
};

class PreferencesDialog::AudioPage : public juce::Component
{
public:
    explicit AudioPage (te::Engine& engine)
        : selector (engine.getDeviceManager().deviceManager, 0, 512, 1, 512, false, false, true, true)
    {
        addAndMakeVisible (selector);
    }

    void resized() override
    {
        selector.setBounds (getLocalBounds().reduced (8));
    }

private:
    juce::AudioDeviceSelectorComponent selector;
};

PreferencesDialog::PreferencesDialog (AppSettings& settings,
                                      AppLookAndFeel& lookAndFeel,
                                      te::Engine& engine,
                                      juce::ApplicationCommandManager& commandManager,
                                      std::function<void (int autosaveSeconds)> onAutosaveChanged)
    : appSettings (settings),
      appLookAndFeel (lookAndFeel),
      engineRef (engine),
      commandManagerRef (commandManager),
      autosaveCallback (std::move (onAutosaveChanged))
{
    generalPage = std::make_unique<GeneralPage> (appSettings, autosaveCallback);
    appearancePage = std::make_unique<AppearancePage> (appSettings, appLookAndFeel);
    keyboardPage = std::make_unique<KeyboardPage> (appSettings, commandManagerRef);
    audioPage = std::make_unique<AudioPage> (engineRef);

    tabs.addTab ("General", AppColours::panelBackground (appSettings.getTheme()), generalPage.get(), false);
    tabs.addTab ("Appearance", AppColours::panelBackground (appSettings.getTheme()), appearancePage.get(), false);
    tabs.addTab ("Keyboard", AppColours::panelBackground (appSettings.getTheme()), keyboardPage.get(), false);
    tabs.addTab ("Audio", AppColours::panelBackground (appSettings.getTheme()), audioPage.get(), false);

    addAndMakeVisible (tabs);
    setSize (560, 460);
}

void PreferencesDialog::paint (juce::Graphics& g)
{
    g.fillAll (AppColours::panelBackground (appSettings.getTheme()));
}

void PreferencesDialog::resized()
{
    tabs.setBounds (getLocalBounds());
}

void PreferencesDialog::show (juce::Component* centreAround,
                              AppSettings& settings,
                              AppLookAndFeel& lookAndFeel,
                              te::Engine& engine,
                              juce::ApplicationCommandManager& commandManager,
                              std::function<void (int autosaveSeconds)> onAutosaveChanged)
{
    auto content = std::make_unique<PreferencesDialog> (settings, lookAndFeel, engine, commandManager,
                                                        std::move (onAutosaveChanged));

    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle = "Preferences";
    o.dialogBackgroundColour = AppColours::panelBackground (settings.getTheme());
    o.content.setOwned (content.release());
    o.componentToCentreAround = centreAround;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

} // namespace skeletonhive
