#pragma once

#include "EffectControlComponents.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

class DelayEditor : public te::Plugin::EditorComponent,
                    private te::ValueTreeAllEventListener
{
public:
    static std::unique_ptr<te::Plugin::EditorComponent> create (te::DelayPlugin& delay);

    bool allowWindowResizing() override { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void resized() override;
    ~DelayEditor() override;

private:
    explicit DelayEditor (te::DelayPlugin& delayPlugin);

    void valueTreeChanged() override {}
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override {}

    te::DelayPlugin& delay;
    juce::Label titleLabel;
    EffectEditorScrollPanel controls;
    bool updatingFromModel = false;
};

class ReverbEditor : public te::Plugin::EditorComponent,
                     private te::ValueTreeAllEventListener
{
public:
    static std::unique_ptr<te::Plugin::EditorComponent> create (te::ReverbPlugin& reverb);

    bool allowWindowResizing() override { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void resized() override;
    ~ReverbEditor() override;

private:
    explicit ReverbEditor (te::ReverbPlugin& reverbPlugin);

    void valueTreeChanged() override {}
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override {}

    te::ReverbPlugin& reverb;
    juce::Label titleLabel;
    EffectEditorScrollPanel controls;
    bool updatingFromModel = false;
};

} // namespace skeletonhive
