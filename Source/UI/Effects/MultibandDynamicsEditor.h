#pragma once

#include "EffectControlComponents.h"
#include "Engine/Effects/MultibandDynamicsPlugin.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

class MultibandDynamicsEditor : public te::Plugin::EditorComponent,
                                private te::ValueTreeAllEventListener
{
public:
    static std::unique_ptr<te::Plugin::EditorComponent> create (MultibandDynamicsPlugin& plugin);

    bool allowWindowResizing() override { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void resized() override;
    ~MultibandDynamicsEditor() override;

private:
    explicit MultibandDynamicsEditor (MultibandDynamicsPlugin& mb);

    void valueTreeChanged() override {}
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override {}

    MultibandDynamicsPlugin& multiband;
    juce::Label titleLabel;
    EffectEditorScrollPanel controls;
    bool updatingFromModel = false;
};

} // namespace skeletonhive
