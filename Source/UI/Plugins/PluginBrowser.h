#pragma once

#include "Engine/PluginScanner.h"
#include "Engine/PluginStateManager.h"

namespace skeletonhive
{

enum class PluginBrowserFilter
{
    All,
    Instruments,
    Effects,
    Native,
    Favorites,
    Recent
};

class PluginBrowser : public juce::Component,
                      private juce::Timer,
                      private juce::TextEditor::Listener,
                      private juce::ComboBox::Listener
{
public:
    PluginBrowser (PluginScanner& scanner, te::Edit& edit, PluginStateManager& stateManager);
    ~PluginBrowser() override;

    te::Track* selectedTrack = nullptr;

    void refreshList();
    void scanPlugins();
    void showFailedPluginsDialog();
    void resized() override;

private:
    class PluginListModel;

    void timerCallback() override;
    void finishScan (const PluginScanReport& report);
    void textEditorTextChanged (juce::TextEditor&) override;
    void comboBoxChanged (juce::ComboBox*) override;
    void updateFailedButton();
    void updateNativeFilterUi();

    juce::Array<juce::PluginDescription> getFilteredPlugins() const;
    void rebuildListBox();

    PluginScanner& pluginScanner;
    te::Edit& edit;
    PluginStateManager& pluginStateManager;

    juce::TextEditor searchBox;
    juce::ComboBox categoryFilter;
    juce::ComboBox vendorFilter;
    juce::ListBox pluginList;
    juce::TextButton scanButton { "Scan" }, failedButton { "Failed" }, insertButton { "Insert" };
    juce::Label statusLabel;
    std::unique_ptr<PluginListModel> listModel;
    juce::PluginDescription selectedPlugin;
};

} // namespace skeletonhive
