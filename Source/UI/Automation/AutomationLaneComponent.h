#pragma once

#include "TracktionCommon.h"
#include "UI/Arrangement/EditViewState.h"

namespace skeletonhive
{

/** One automation curve for a single parameter, mapped to the currently
    visible timeline range. Points can be added (click), moved (drag) and
    deleted (right-click); all edits go through the Edit's UndoManager.
*/
class AutomationLaneComponent : public juce::Component,
                                private te::AutomatableParameter::Listener
{
public:
    AutomationLaneComponent (te::AutomatableParameter::Ptr param, EditViewState& viewState);
    ~AutomationLaneComponent() override;

    te::AutomatableParameter& getParameter() { return *parameter; }

    std::function<void (AutomationLaneComponent&)> onRemoveLane;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;

private:
    // te::AutomatableParameter::Listener
    void curveHasChanged (te::AutomatableParameter&) override { repaint(); }
    void currentValueChanged (te::AutomatableParameter&) override { repaint(); }

    juce::Rectangle<int> getCurveArea() const;
    int hitTestPoint (juce::Point<int> pos) const;
    float yToNormalisedValue (int y) const;
    int normalisedValueToY (float normalised) const;
    te::TimePosition xToTime (int x) const;
    int timeToX (te::TimePosition time) const;

    te::AutomatableParameter::Ptr parameter;
    EditViewState& editViewState;
    juce::Label nameLabel;
    juce::TextButton removeButton { "x" };

    int draggedPointIndex = -1;
    int hoveredPointIndex = -1;

    static constexpr int labelWidth = 130;
    static constexpr int pointHitRadius = 6;
};

} // namespace skeletonhive
