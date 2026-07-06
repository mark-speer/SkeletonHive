#pragma once

#include "MidiControllerLaneComponent.h"
#include "MidiLaneViewport.h"

namespace skeletonhive
{

/** Bottom strip of the piano roll: lane selector plus velocity or controller editing. */
class MidiLaneEditor : public juce::Component
{
public:
    enum class LaneType
    {
        velocity = 1,
        probability,
        iteration,
        cc,
        pitchBend,
        aftertouch
    };

    MidiLaneEditor (te::MidiClip& clip,
                    MidiLaneViewport& viewport,
                    std::function<const juce::Array<juce::ValueTree>&()> getSelection,
                    std::function<bool (const te::MidiNote&)> isNoteSelected,
                    std::function<void()> onNoteVisualsChanged);

    void sequenceChanged();
    void viewportChanged();

    LaneType getLaneType() const { return currentLane; }

private:
    enum class VelocityDragMode { none, selection, pencil };

    void rebuildCcBox();
    void updateActiveLane();
    int activeControllerType() const;

    juce::Rectangle<int> getLaneCanvasBounds() const;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    void paintVelocityLane (juce::Graphics& g, juce::Rectangle<int> area) const;
    void paintProbabilityLane (juce::Graphics& g, juce::Rectangle<int> area) const;
    void paintIterationLane (juce::Graphics& g, juce::Rectangle<int> area) const;
    int velocityFromY (int y, juce::Rectangle<int> area) const;
    int probabilityFromY (int y, juce::Rectangle<int> area) const;
    int iterationFromY (int y, juce::Rectangle<int> area) const;
    void applyLaneValueAt (const juce::MouseEvent& e, juce::Rectangle<int> area,
                           std::function<void (te::MidiNote&, juce::UndoManager*)> applyValue);

    te::MidiClip& clip;
    MidiLaneViewport& viewport;
    std::function<const juce::Array<juce::ValueTree>&()> getSelection;
    std::function<bool (const te::MidiNote&)> isNoteSelected;
    std::function<void()> onNoteVisualsChanged;

    juce::ComboBox laneTypeBox, ccBox;
    std::unique_ptr<MidiControllerLaneComponent> controllerLane;

    LaneType currentLane = LaneType::velocity;
    int selectedCc = 1;

    VelocityDragMode velocityDrag = VelocityDragMode::none;
    juce::Array<juce::ValueTree> velocityTargets;
    juce::Point<int> dragStartPos;

    static constexpr int selectorHeight = 22;
    static constexpr int ccBoxWidth = 110;
};

} // namespace skeletonhive
