#pragma once

#include "SamplerWaveformComponent.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

/** Dedicated editor for standalone te::SamplerPlugin (Simpler/Sampler-equivalent UI). */
class SamplerEditor : public te::Plugin::EditorComponent,
                      public juce::DragAndDropTarget,
                      private te::ValueTreeAllEventListener,
                      private juce::Timer
{
public:
    static std::unique_ptr<te::Plugin::EditorComponent> create (te::SamplerPlugin& sampler);
    ~SamplerEditor() override;

    bool allowWindowResizing() override { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void resized() override;

    bool isInterestedInDragSource (const SourceDetails& dragSourceDetails) override;
    void itemDragEnter (const SourceDetails& dragSourceDetails) override;
    void itemDragExit (const SourceDetails& dragSourceDetails) override;
    void itemDropped (const SourceDetails& dragSourceDetails) override;

    void setSelectedSound (int index);

private:
    class SoundListModel;

    explicit SamplerEditor (te::SamplerPlugin& samplerPlugin);

    void valueTreeChanged() override {}
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop) override;
    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child) override;
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int index) override;

    void timerCallback() override;
    void refreshFromModel();
    void refreshSoundList();
    void browseForSample();
    void removeSelectedSound();
    void assignSampleFile (const juce::File& file);
    void applyPendingGains();
    void updateDetailVisibility();

    te::SamplerPlugin& sampler;
    SamplerWaveformComponent waveform;
    juce::Label titleLabel;
    juce::ListBox soundList;
    juce::TextButton addButton { "Add Sample..." };
    juce::TextButton removeButton { "Remove" };
    juce::Label emptyLabel;
    juce::Label gainLabel;
    juce::Label panLabel;
    juce::Label rootLabel;
    juce::Slider gainSlider;
    juce::Slider panSlider;
    juce::Slider rootSlider;
    juce::ToggleButton oneShotButton { "One-shot" };
    juce::Label oneShotHintLabel;
    std::unique_ptr<SoundListModel> listModel;
    int selectedSound = -1;
    bool updatingFromModel = false;
    bool dragHover = false;
    bool pendingGainUpdate = false;
    float pendingGainDb = 0.0f;
    float pendingPan = 0.0f;
};

} // namespace skeletonhive
