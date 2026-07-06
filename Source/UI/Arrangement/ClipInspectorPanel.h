#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

/** Bottom inspector for selected clip name/colour, audio-clip properties, and MIDI scale lock. */
class ClipInspectorPanel : public juce::Component,
                           private te::ValueTreeAllEventListener
{
public:
    ClipInspectorPanel (te::Edit& edit, te::SelectionManager& selectionManager);
    ~ClipInspectorPanel() override;

    void setClips (const juce::Array<te::Clip*>& newClips);
    bool hasAudioSelection() const { return ! audioClips.isEmpty(); }
    bool hasMidiSelection() const { return ! midiClips.isEmpty(); }
    bool hasClipSelection() const { return hasAudioSelection() || hasMidiSelection(); }
    int getPreferredHeight() const;

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    void valueTreeChanged() override {}
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop) override;

    void attachClipListener (te::Clip* clip);
    void detachClipListener();
    void refreshFromModel();
    void applyToAudioClips (std::function<void (te::AudioClipBase&)> fn);
    void applyToMidiClips (std::function<void (te::MidiClip&)> fn);
    void updateControlVisibility();
    juce::Array<te::Clip*> clipsIncludingLinkedPeers() const;

    juce::Array<te::Clip*> clips;
    juce::Array<te::AudioClipBase*> audioClips;
    juce::Array<te::MidiClip*> midiClips;
    te::Clip::Ptr listenedClip;

    bool updatingFromModel = false;

    juce::Label titleLabel, countLabel;
    juce::TextEditor nameEditor;
    juce::OwnedArray<juce::TextButton> colourButtons;

    juce::Label gainLabel, transposeLabel, speedLabel, stretchLabel, loopLengthLabel;
    juce::Label scaleRootLabel, scaleModeLabel;
    juce::Label takeLabel;
    juce::Slider gainSlider, transposeSlider, speedSlider, loopLengthSlider;
    juce::ToggleButton reverseButton { "Reverse" }, loopButton { "Loop" };
    juce::ToggleButton scaleLockButton { "Scale Lock" };
    juce::ComboBox stretchModeBox, takeBox, scaleRootBox, scaleModeBox;

    te::Edit& edit;
    te::SelectionManager& selectionManager;

    static constexpr int panelHeight = 110;
    static constexpr int midiPanelHeight = 84;
    static constexpr int takeRowHeight = 24;
    static constexpr int headerHeight = 28;
};

} // namespace skeletonhive
