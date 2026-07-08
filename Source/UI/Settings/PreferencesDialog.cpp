#include "PreferencesDialog.h"
#include "UI/Plugins/PluginPickerDialog.h"
#include "Engine/EngineHelpers.h"

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

class PreferencesDialog::LibraryPage : public juce::Component
{
public:
    LibraryPage (AppSettings& settings, std::function<void()> onPathsChanged)
        : appSettings (settings), pathsChanged (std::move (onPathsChanged))
    {
        infoLabel.setText ("Sample library folders (WAV, AIFF, FLAC, etc.):", juce::dontSendNotification);
        addButton.onClick = [this] { addFolder(); };
        removeButton.onClick = [this] { removeSelected(); };

        listModel = std::make_unique<FolderListModel> (*this);
        folderList.setModel (listModel.get());
        folderList.setRowHeight (22);
        refreshList();

        addAndMakeVisible (infoLabel);
        addAndMakeVisible (folderList);
        addAndMakeVisible (addButton);
        addAndMakeVisible (removeButton);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        infoLabel.setBounds (r.removeFromTop (20));
        r.removeFromTop (8);
        auto buttons = r.removeFromBottom (28);
        addButton.setBounds (buttons.removeFromLeft (80));
        buttons.removeFromLeft (8);
        removeButton.setBounds (buttons.removeFromLeft (80));
        r.removeFromBottom (8);
        folderList.setBounds (r);
    }

    juce::Array<juce::File> getPaths() const { return paths; }

private:
    class FolderListModel : public juce::ListBoxModel
    {
    public:
        explicit FolderListModel (LibraryPage& owner) : page (owner) {}

        int getNumRows() override { return page.paths.size(); }

        void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
        {
            if (! juce::isPositiveAndBelow (row, page.paths.size()))
                return;

            if (selected)
                g.fillAll (juce::Colours::white.withAlpha (0.12f));

            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.setFont (juce::FontOptions ((float) height * 0.65f));
            g.drawText (page.paths.getReference (row).getFullPathName(),
                        4, 0, width - 8, height, juce::Justification::centredLeft, true);
        }

        LibraryPage& page;
    };

    void refreshList()
    {
        paths = appSettings.getSampleLibraryPaths();
        folderList.updateContent();
    }

    void persist()
    {
        appSettings.setSampleLibraryPaths (paths);
        if (pathsChanged)
            pathsChanged();
    }

    void addFolder()
    {
        juce::File start = paths.isEmpty() ? appSettings.getDefaultProjectFolder()
                                           : paths.getLast();

        auto fc = std::make_shared<juce::FileChooser> ("Add sample library folder", start);
        fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                         [this, fc] (const juce::FileChooser&)
                         {
                             const auto f = fc->getResult();
                             if (! f.isDirectory())
                                 return;

                             for (const auto& existing : paths)
                             {
                                 if (existing == f)
                                     return;
                             }

                             paths.add (f);
                             folderList.updateContent();
                             persist();
                         });
    }

    void removeSelected()
    {
        const int row = folderList.getSelectedRow();
        if (! juce::isPositiveAndBelow (row, paths.size()))
            return;

        paths.remove (row);
        folderList.updateContent();
        persist();
    }

    AppSettings& appSettings;
    std::function<void()> pathsChanged;
    juce::Label infoLabel;
    juce::ListBox folderList;
    juce::TextButton addButton { "Add..." };
    juce::TextButton removeButton { "Remove" };
    juce::Array<juce::File> paths;
    std::unique_ptr<FolderListModel> listModel;
};

class PreferencesDialog::DevicesPage : public juce::Component
{
public:
    DevicesPage (AppSettings& settings,
                 te::Engine& engine,
                 PluginScanner& scanner,
                 PluginStateManager& stateManager)
        : appSettings (settings),
          engineRef (engine),
          pluginScanner (scanner),
          pluginStateManager (stateManager)
    {
        audioLabel.setText ("New audio track chain (effects):", juce::dontSendNotification);
        midiLabel.setText ("New MIDI track chain (instrument + effects):", juce::dontSendNotification);

        audioChain = appSettings.getDefaultDeviceChain (DefaultChainKind::audioTrack);
        midiChain = appSettings.getDefaultDeviceChain (DefaultChainKind::midiTrack);

        audioListModel = std::make_unique<ChainListModel> (*this, true);
        midiListModel = std::make_unique<ChainListModel> (*this, false);
        audioList.setModel (audioListModel.get());
        midiList.setModel (midiListModel.get());
        audioList.setRowHeight (22);
        midiList.setRowHeight (22);

        wireButtons (audioAdd, audioRemove, audioUp, audioDown, audioList, true);
        wireButtons (midiAdd, midiRemove, midiUp, midiDown, midiList, false);

        sandboxToggle.setButtonText ("Run VST3 effects in separate process (crash isolation)");
        sandboxToggle.setToggleState (appSettings.isPluginSandboxEnabled(), juce::dontSendNotification);
        sandboxToggle.onClick = [this]
        {
            appSettings.setPluginSandboxEnabled (sandboxToggle.getToggleState());
        };

        addAndMakeVisible (sandboxToggle);
        addAndMakeVisible (audioLabel);
        addAndMakeVisible (audioList);
        addAndMakeVisible (audioAdd);
        addAndMakeVisible (audioRemove);
        addAndMakeVisible (audioUp);
        addAndMakeVisible (audioDown);
        addAndMakeVisible (midiLabel);
        addAndMakeVisible (midiList);
        addAndMakeVisible (midiAdd);
        addAndMakeVisible (midiRemove);
        addAndMakeVisible (midiUp);
        addAndMakeVisible (midiDown);
    }

    ~DevicesPage() override
    {
        appSettings.setDefaultDeviceChain (DefaultChainKind::audioTrack, audioChain);
        appSettings.setDefaultDeviceChain (DefaultChainKind::midiTrack, midiChain);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        sandboxToggle.setBounds (r.removeFromTop (24));
        r.removeFromTop (12);
        layoutChainSection (r, audioLabel, audioList, audioAdd, audioRemove, audioUp, audioDown);
        r.removeFromTop (12);
        layoutChainSection (r, midiLabel, midiList, midiAdd, midiRemove, midiUp, midiDown);
    }

private:
    class ChainListModel : public juce::ListBoxModel
    {
    public:
        ChainListModel (DevicesPage& owner, bool audio) : page (owner), isAudio (audio) {}

        int getNumRows() override
        {
            return isAudio ? page.audioChain.size() : page.midiChain.size();
        }

        void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
        {
            const auto& chain = isAudio ? page.audioChain : page.midiChain;
            if (! juce::isPositiveAndBelow (row, chain.size()))
                return;

            if (selected)
                g.fillAll (juce::Colours::white.withAlpha (0.12f));

            const auto desc = EngineHelpers::lookupKnownPlugin (page.engineRef, chain[row]);
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawText (desc.name.isNotEmpty() ? desc.name : chain[row],
                        6, 0, width - 12, height, juce::Justification::centredLeft, true);
        }

        DevicesPage& page;
        bool isAudio;
    };

    static void layoutChainSection (juce::Rectangle<int>& area,
                                    juce::Label& label,
                                    juce::ListBox& list,
                                    juce::TextButton& add,
                                    juce::TextButton& remove,
                                    juce::TextButton& up,
                                    juce::TextButton& down)
    {
        label.setBounds (area.removeFromTop (20));
        area.removeFromTop (4);
        auto section = area.removeFromTop (juce::jmin (110, area.getHeight()));
        auto buttons = section.removeFromRight (72);
        down.setBounds (buttons.removeFromBottom (24));
        buttons.removeFromBottom (4);
        up.setBounds (buttons.removeFromBottom (24));
        buttons.removeFromBottom (4);
        remove.setBounds (buttons.removeFromBottom (24));
        buttons.removeFromBottom (4);
        add.setBounds (buttons.removeFromBottom (24));
        list.setBounds (section);
    }

    void wireButtons (juce::TextButton& add,
                      juce::TextButton& remove,
                      juce::TextButton& up,
                      juce::TextButton& down,
                      juce::ListBox& list,
                      bool audio)
    {
        add.setButtonText ("Add");
        remove.setButtonText ("Remove");
        up.setButtonText ("Up");
        down.setButtonText ("Down");

        add.onClick = [this, audio] { addPlugin (audio); };
        remove.onClick = [this, audio, &list] { removeSelected (audio, list); };
        up.onClick = [this, audio, &list] { moveSelected (audio, list, -1); };
        down.onClick = [this, audio, &list] { moveSelected (audio, list, 1); };
    }

    juce::StringArray& chainFor (bool audio) { return audio ? audioChain : midiChain; }

    void addPlugin (bool audio)
    {
        const auto filter = audio ? PluginPickerFilter::effectsOnly : PluginPickerFilter::all;

        PluginPickerDialog::show (this,
                                  pluginScanner,
                                  engineRef,
                                  pluginStateManager,
                                  filter,
                                  audio ? "Add Audio Track Effect" : "Add MIDI Track Device",
                                  [this, audio] (const juce::PluginDescription& desc)
        {
            chainFor (audio).add (desc.createIdentifierString());
            refreshLists();
        });
    }

    void removeSelected (bool audio, juce::ListBox& list)
    {
        const int row = list.getSelectedRow();
        auto& chain = chainFor (audio);

        if (juce::isPositiveAndBelow (row, chain.size()))
        {
            chain.remove (row);
            refreshLists();
        }
    }

    void moveSelected (bool audio, juce::ListBox& list, int delta)
    {
        const int row = list.getSelectedRow();
        auto& chain = chainFor (audio);
        const int newRow = row + delta;

        if (juce::isPositiveAndBelow (row, chain.size())
            && juce::isPositiveAndBelow (newRow, chain.size()))
        {
            chain.move (row, newRow);
            refreshLists();
            list.selectRow (newRow);
        }
    }

    void refreshLists()
    {
        audioList.updateContent();
        midiList.updateContent();
        audioList.repaint();
        midiList.repaint();
    }

    AppSettings& appSettings;
    te::Engine& engineRef;
    PluginScanner& pluginScanner;
    PluginStateManager& pluginStateManager;

    juce::StringArray audioChain;
    juce::StringArray midiChain;

    juce::ToggleButton sandboxToggle;
    juce::Label audioLabel, midiLabel;
    juce::ListBox audioList, midiList;
    juce::TextButton audioAdd, audioRemove, audioUp, audioDown;
    juce::TextButton midiAdd, midiRemove, midiUp, midiDown;
    std::unique_ptr<ChainListModel> audioListModel, midiListModel;
};

PreferencesDialog::PreferencesDialog (AppSettings& settings,
                                      AppLookAndFeel& lookAndFeel,
                                      te::Engine& engine,
                                      PluginScanner& pluginScanner,
                                      PluginStateManager& pluginStateManager,
                                      juce::ApplicationCommandManager& commandManager,
                                      std::function<void (int autosaveSeconds)> onAutosaveChanged,
                                      std::function<void()> onLibraryPathsChanged)
    : appSettings (settings),
      appLookAndFeel (lookAndFeel),
      engineRef (engine),
      pluginScannerRef (pluginScanner),
      pluginStateManagerRef (pluginStateManager),
      commandManagerRef (commandManager),
      autosaveCallback (std::move (onAutosaveChanged))
{
    generalPage = std::make_unique<GeneralPage> (appSettings, autosaveCallback);
    appearancePage = std::make_unique<AppearancePage> (appSettings, appLookAndFeel);
    keyboardPage = std::make_unique<KeyboardPage> (appSettings, commandManagerRef);
    audioPage = std::make_unique<AudioPage> (engineRef);
    libraryPage = std::make_unique<LibraryPage> (appSettings, std::move (onLibraryPathsChanged));
    devicesPage = std::make_unique<DevicesPage> (appSettings, engineRef, pluginScannerRef, pluginStateManagerRef);

    tabs.addTab ("General", AppColours::panelBackground (appSettings.getTheme()), generalPage.get(), false);
    tabs.addTab ("Appearance", AppColours::panelBackground (appSettings.getTheme()), appearancePage.get(), false);
    tabs.addTab ("Keyboard", AppColours::panelBackground (appSettings.getTheme()), keyboardPage.get(), false);
    tabs.addTab ("Audio", AppColours::panelBackground (appSettings.getTheme()), audioPage.get(), false);
    tabs.addTab ("Library", AppColours::panelBackground (appSettings.getTheme()), libraryPage.get(), false);
    tabs.addTab ("Devices", AppColours::panelBackground (appSettings.getTheme()), devicesPage.get(), false);

    addAndMakeVisible (tabs);
    setSize (560, 520);
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
                              PluginScanner& pluginScanner,
                              PluginStateManager& pluginStateManager,
                              juce::ApplicationCommandManager& commandManager,
                              std::function<void (int autosaveSeconds)> onAutosaveChanged,
                              std::function<void()> onLibraryPathsChanged)
{
    auto content = std::make_unique<PreferencesDialog> (settings, lookAndFeel, engine, pluginScanner,
                                                        pluginStateManager, commandManager,
                                                        std::move (onAutosaveChanged),
                                                        std::move (onLibraryPathsChanged));

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
