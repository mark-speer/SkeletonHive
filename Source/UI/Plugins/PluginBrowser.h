#pragma once

#include "Engine/PluginScanner.h"

namespace arrange
{

class PluginBrowser : public juce::Component,
                      private juce::Timer
{
public:
    PluginBrowser (PluginScanner& scanner, te::Edit& edit);

    te::Track* selectedTrack = nullptr;

    void refreshList();
    void scanPlugins();
    void resized() override;

private:
    class PluginTreeItem : public juce::TreeViewItem
    {
    public:
        PluginTreeItem (PluginBrowser& owner, juce::PluginDescription desc);
        juce::String getUniqueName() const override { return desc.createIdentifierString(); }
        bool mightContainSubItems() override { return false; }
        int getItemHeight() const override { return 22; }
        void paintItem (juce::Graphics& g, int width, int height) override;
        void itemClicked (const juce::MouseEvent& e) override;

    private:
        PluginBrowser& browser;
        juce::PluginDescription desc;
    };

    class RootItem : public juce::TreeViewItem
    {
    public:
        explicit RootItem (PluginBrowser& owner);
        juce::String getUniqueName() const override { return "root"; }
        bool mightContainSubItems() override { return true; }
        void itemOpennessChanged (bool) override;

    private:
        PluginBrowser& browser;
    };

    void timerCallback() override;
    void finishScan (int numFound);

    PluginScanner& pluginScanner;
    te::Edit& edit;
    juce::TreeView tree;
    juce::TextButton scanButton { "Scan Plugins" }, insertButton { "Insert on Track" };
    juce::Label statusLabel;
    std::unique_ptr<RootItem> rootItem;
    juce::PluginDescription selectedPlugin;
};

} // namespace arrange
