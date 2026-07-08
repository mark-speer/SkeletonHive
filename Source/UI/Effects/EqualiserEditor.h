#pragma once

#include "EqualiserCurveComponent.h"
#include "EffectControlComponents.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

class EqualiserEditor : public te::Plugin::EditorComponent,
                        private te::ValueTreeAllEventListener
{
public:
    static std::unique_ptr<te::Plugin::EditorComponent> create (te::EqualiserPlugin& eq);

    bool allowWindowResizing() override { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void resized() override;
    ~EqualiserEditor() override;

private:
    explicit EqualiserEditor (te::EqualiserPlugin& eqPlugin);

    void valueTreeChanged() override {}
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    void refreshFromModel();

    te::EqualiserPlugin& eq;
    juce::Label titleLabel;
    EqualiserCurveComponent curve;
    EffectEditorScrollPanel controls;
    bool updatingFromModel = false;
};

} // namespace skeletonhive
