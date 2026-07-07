#pragma once

#include "SynthModMatrixPanel.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

/** Dedicated editor for te::FourOscPlugin (subtractive synth UI). */
class SynthEditor : public te::Plugin::EditorComponent,
                    private te::ValueTreeAllEventListener
{
public:
    static std::unique_ptr<te::Plugin::EditorComponent> create (te::FourOscPlugin& synth);

    bool allowWindowResizing() override { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void resized() override;

    ~SynthEditor() override;

private:
    explicit SynthEditor (te::FourOscPlugin& synthPlugin);

    void valueTreeChanged() override {}
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop) override;

    void refreshFromModel();

    te::FourOscPlugin& synth;
    juce::Label titleLabel;
    juce::TabbedComponent tabs;
    bool updatingFromModel = false;
};

} // namespace skeletonhive
