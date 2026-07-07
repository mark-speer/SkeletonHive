#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

/** Fallback editor for TE built-in plugins that do not provide their own UI. */
class NativePluginEditor : public te::Plugin::EditorComponent
{
public:
    static std::unique_ptr<te::Plugin::EditorComponent> create (te::Plugin& plugin);

    bool allowWindowResizing() override { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void resized() override;

private:
    explicit NativePluginEditor (te::Plugin& plugin);

    void rebuildContent();

    te::Plugin& plugin;
    juce::Viewport viewport;
    juce::Component content;
};

} // namespace skeletonhive
