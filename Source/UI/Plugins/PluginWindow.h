#pragma once

#include "TracktionCommon.h"

namespace arrange
{

class PluginWindow : public juce::DocumentWindow
{
public:
    explicit PluginWindow (te::Plugin& plugin);
    ~PluginWindow() override;

    static std::unique_ptr<juce::Component> create (te::Plugin& plugin);
    void show();
    void recreateEditorAsync();

private:
    void moved() override;
    void closeButtonPressed() override;
    float getDesktopScaleFactor() const override { return 1.0f; }

    void recreateEditor();
    void setEditor (std::unique_ptr<te::Plugin::EditorComponent> newEditor);

    te::Plugin& plugin;
    te::PluginWindowState& windowState;
    std::unique_ptr<te::Plugin::EditorComponent> editor;
    bool updateStoredBounds = false;
};

} // namespace arrange
