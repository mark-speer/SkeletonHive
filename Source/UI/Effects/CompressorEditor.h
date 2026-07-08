#pragma once

#include "EffectControlComponents.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

class CompressorEditor : public te::Plugin::EditorComponent,
                         private te::ValueTreeAllEventListener
{
public:
    static std::unique_ptr<te::Plugin::EditorComponent> create (te::CompressorPlugin& comp);

    bool allowWindowResizing() override { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void resized() override;
    ~CompressorEditor() override;

private:
    explicit CompressorEditor (te::CompressorPlugin& compPlugin);

    void valueTreeChanged() override {}
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    te::CompressorPlugin& comp;
    juce::Label titleLabel;
    EffectEditorScrollPanel controls;
    std::unique_ptr<BoolToggleRow> sidechainRow;
    bool updatingFromModel = false;
};

} // namespace skeletonhive
