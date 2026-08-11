#pragma once

#include "Engine/PluginHost/SandboxedPluginInstance.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

class PluginWindow : public juce::DocumentWindow
{
public:
    explicit PluginWindow (te::Plugin& plugin, bool createEditorNow = true);
    ~PluginWindow() override;

    static std::unique_ptr<juce::Component> create (te::Plugin& plugin, juce::Component* alertParent = nullptr);
    void show();
    void recreateEditorAsync();
    void setSandboxPlaceholder (te::ExternalPlugin& externalPlugin);
    void setSandboxEmbeddedEditor (te::ExternalPlugin& externalPlugin);
    void replaceSandboxContentWithFallback (te::ExternalPlugin& externalPlugin,
                                            const SandboxEditorResult& result = {});

private:
    void clearBridgeEditorInvalidationHandler();
    void moved() override;
    void resized() override;
    void childBoundsChanged (juce::Component* child) override;
    void closeButtonPressed() override;
    float getDesktopScaleFactor() const override { return 1.0f; }

    void recreateEditor();
    void setEditor (std::unique_ptr<te::Plugin::EditorComponent> newEditor);
    void resizeToFitEditorContent();
    void resizeToFitSandboxContent (int contentW, int contentH);

    te::Plugin& plugin;
    te::PluginWindowState& windowState;
    std::unique_ptr<te::Plugin::EditorComponent> editor;
    bool updateStoredBounds = false;
};

} // namespace skeletonhive
