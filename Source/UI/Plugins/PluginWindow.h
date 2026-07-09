#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

class PluginWindow : public juce::DocumentWindow
{
public:
    explicit PluginWindow (te::Plugin& plugin);
    ~PluginWindow() override;

    static std::unique_ptr<juce::Component> create (te::Plugin& plugin, juce::Component* alertParent = nullptr);
    void show();
    void recreateEditorAsync();
    void setSandboxPlaceholder (te::ExternalPlugin& externalPlugin);

private:
    void moved() override;
    void resized() override;
    void childBoundsChanged (juce::Component* child) override;
    void closeButtonPressed() override;
    float getDesktopScaleFactor() const override { return 1.0f; }

    void recreateEditor();
    void setEditor (std::unique_ptr<te::Plugin::EditorComponent> newEditor);
    void resizeToFitEditorContent();

    te::Plugin& plugin;
    te::PluginWindowState& windowState;
    std::unique_ptr<te::Plugin::EditorComponent> editor;
    bool updateStoredBounds = false;
};

} // namespace skeletonhive
