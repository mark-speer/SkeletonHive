#pragma once

#include "EffectControlComponents.h"
#include "Engine/Effects/SaturationPlugin.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

class SaturationEditor : public te::Plugin::EditorComponent,
                         private te::ValueTreeAllEventListener
{
public:
    static std::unique_ptr<te::Plugin::EditorComponent> create (SaturationPlugin& plugin);

    bool allowWindowResizing() override { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void resized() override;
    ~SaturationEditor() override;

private:
    explicit SaturationEditor (SaturationPlugin& saturation);

    void valueTreeChanged() override {}
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override {}

    SaturationPlugin& saturation;
    juce::Label titleLabel;
    EffectEditorScrollPanel controls;
    bool updatingFromModel = false;
};

} // namespace skeletonhive
