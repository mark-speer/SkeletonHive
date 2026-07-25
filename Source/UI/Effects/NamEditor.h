#pragma once

#include "EffectControlComponents.h"
#include "Engine/Effects/NamPlugin.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

class NamEditor : public te::Plugin::EditorComponent,
                  private te::ValueTreeAllEventListener
{
public:
    static std::unique_ptr<te::Plugin::EditorComponent> create (NamPlugin& plugin);

    bool allowWindowResizing() override { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void resized() override;
    ~NamEditor() override;

private:
    explicit NamEditor (NamPlugin& nam);

    void valueTreeChanged() override {}
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    void refreshStatus();
    void browseForModel();

    NamPlugin& nam;
    juce::Label titleLabel;
    juce::Label pathLabel;
    juce::Label statusLabel;
    juce::TextButton browseButton { "Browse..." };
    juce::TextButton reloadButton { "Reload" };
    EffectEditorScrollPanel controls;
    bool updatingFromModel = false;
};

} // namespace skeletonhive
