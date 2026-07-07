#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

class DrumPadComponent : public juce::Component,
                         public juce::DragAndDropTarget
{
public:
    DrumPadComponent (int padIndexIn, int midiNoteIn);

    void setSelected (bool shouldBeSelected);
    bool isSelected() const { return selected; }

    void setSampleName (const juce::String& name);
    void setEmpty (bool shouldBeEmpty);

    std::function<void (int padIndex)> onSelected;
    std::function<void (int padIndex)> onBrowseRequested;
    std::function<void (int padIndex, const juce::File& file)> onSampleDropped;
    std::function<void (int padIndex)> onClearRequested;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    bool isInterestedInDragSource (const SourceDetails& dragSourceDetails) override;
    void itemDragEnter (const SourceDetails& dragSourceDetails) override;
    void itemDragExit (const SourceDetails& dragSourceDetails) override;
    void itemDropped (const SourceDetails& dragSourceDetails) override;

private:
    int padIndex;
    int midiNote;
    bool selected = false;
    bool empty = true;
    bool dragHover = false;
    juce::String sampleName;
};

} // namespace skeletonhive
