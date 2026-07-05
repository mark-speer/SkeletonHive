#pragma once

#include "Engine/PluginScanner.h"
#include "Engine/PluginStateManager.h"

namespace arrange
{

enum class PluginBrowserFilter
{
    All,
    Instruments,
    Effects,
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

    te::Track* selectedTrack = nullptr;

    void refreshList();
    void scanPlugins();
    void resized() override;

private:
    class PluginListModel;

    void timerCallback() override;
    void finishScan (int numFound);
    void textEditorTextChanged (juce::TextEditor&) override;
    void comboBoxChanged (juce::ComboBox*) override;
    void mouseDrag (const juce::MouseEvent& e) override;

    juce::Array<juce::PluginDescription> getFilteredPlugins() const;
    void rebuildListBox();

    PluginScanner& pluginScanner;
    te::Edit& edit;
    PluginStateManager& pluginStateManager;

    juce::TextEditor searchBox;
    juce::ComboBox categoryFilter;
    juce::ComboBox vendorFilter;
    juce::ListBox pluginList;
    juce::TextButton scanButton { "Scan" }, insertButton { "Insert" };
    juce::Label statusLabel;
    std::unique_ptr<PluginListModel> listModel;
    juce::PluginDescription selectedPlugin;
};

} // namespace arrange
