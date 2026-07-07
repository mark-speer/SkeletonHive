#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

class DrumRackEditor : public te::Plugin::EditorComponent,
                       private juce::Timer
{
public:
    static std::unique_ptr<te::Plugin::EditorComponent> create (te::RackInstance& rack);

    bool allowWindowResizing() override { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void resized() override;

private:
    explicit DrumRackEditor (te::RackInstance& rack);

    void refreshPads();
    void setSelectedPad (int padIndex);
    void browseForPadSample (int padIndex);
    void assignSample (int padIndex, const juce::File& file);
    void clearSelectedPad();
    void timerCallback() override;

    te::RackInstance::Ptr rack;
    juce::OwnedArray<class DrumPadComponent> pads;
    juce::Label titleLabel;
    juce::Label detailLabel;
    juce::TextButton browseButton { "Browse..." };
    juce::TextButton clearButton { "Clear" };
    int selectedPad = 0;
    int lastKnownSoundRevision = -1;
};

} // namespace skeletonhive
