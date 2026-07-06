#include "AutomationLaneComponent.h"
#include "UI/Arrangement/TimelineGrid.h"

namespace skeletonhive
{

AutomationLaneComponent::AutomationLaneComponent (te::AutomatableParameter::Ptr param, EditViewState& vs)
    : parameter (std::move (param)), editViewState (vs)
{
    jassert (parameter != nullptr);

    nameLabel.setText (parameter->getFullName(), juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centredLeft);
    nameLabel.setFont (juce::FontOptions (12.0f));
    nameLabel.setInterceptsMouseClicks (false, false);

    removeButton.setTooltip ("Hide this automation lane");
    removeButton.onClick = [this]
    {
        if (onRemoveLane)
            onRemoveLane (*this);
    };

    addAndMakeVisible (nameLabel);
    addAndMakeVisible (removeButton);

    parameter->addListener (this);
}

AutomationLaneComponent::~AutomationLaneComponent()
{
    parameter->removeListener (this);
}

juce::Rectangle<int> AutomationLaneComponent::getCurveArea() const
{
    return getLocalBounds().withTrimmedLeft (labelWidth).reduced (0, 3);
}

void AutomationLaneComponent::resized()
{
    auto label = getLocalBounds().removeFromLeft (labelWidth).reduced (2);
    removeButton.setBounds (label.removeFromRight (18).withSizeKeepingCentre (16, 16));
    nameLabel.setBounds (label);
}

void AutomationLaneComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff16213e));

    g.setColour (juce::Colour (0xff0f1830));
    g.fillRect (getLocalBounds().removeFromLeft (labelWidth));

    const auto area = getCurveArea();
    auto& curve = parameter->getCurve();
    const auto& range = parameter->valueRange;
    const float defaultValue = parameter->getCurrentBaseValue();

    // Curve line, sampled per pixel over the visible range
    {
        juce::Path path;
        bool started = false;

        for (int x = area.getX(); x < area.getRight(); ++x)
        {
            const float value = curve.getValueAt (xToTime (x), defaultValue);
            const float y = (float) normalisedValueToY (range.convertTo0to1 (value));

            if (! started)
            {
                path.startNewSubPath ((float) x, y);
                started = true;
            }
            else
            {
                path.lineTo ((float) x, y);
            }
        }

        g.setColour (juce::Colour (0xffe9c46a));
        g.strokePath (path, juce::PathStrokeType (1.5f));
    }

    // Points
    for (int i = 0; i < curve.getNumPoints(); ++i)
    {
        const int x = timeToX (curve.getPointTime (i));
        if (x < area.getX() - pointHitRadius || x > area.getRight() + pointHitRadius)
            continue;

        const int y = normalisedValueToY (range.convertTo0to1 (curve.getPointValue (i)));
        const bool highlighted = (i == hoveredPointIndex || i == draggedPointIndex);

        g.setColour (highlighted ? juce::Colours::white : juce::Colour (0xffe9c46a));
        g.fillEllipse ((float) x - 3.5f, (float) y - 3.5f, 7.0f, 7.0f);
    }

    // Live value marker at the left edge of the curve area
    {
        const int y = normalisedValueToY (parameter->getCurrentNormalisedValue());
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.fillRect (area.getX(), y - 1, 4, 2);
    }

    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

int AutomationLaneComponent::hitTestPoint (juce::Point<int> pos) const
{
    auto& curve = parameter->getCurve();
    const auto& range = parameter->valueRange;

    for (int i = 0; i < curve.getNumPoints(); ++i)
    {
        const int x = timeToX (curve.getPointTime (i));
        const int y = normalisedValueToY (range.convertTo0to1 (curve.getPointValue (i)));

        if (pos.getDistanceFrom ({ x, y }) <= pointHitRadius)
            return i;
    }

    return -1;
}

void AutomationLaneComponent::mouseDown (const juce::MouseEvent& e)
{
    if (! getCurveArea().contains (e.getPosition()))
        return;

    auto& curve = parameter->getCurve();
    auto* um = &editViewState.edit.getUndoManager();
    const int hit = hitTestPoint (e.getPosition());

    if (e.mods.isPopupMenu())
    {
        if (hit >= 0)
        {
            curve.removePoint (hit, um);
            repaint();
        }
        return;
    }

    if (hit >= 0)
    {
        draggedPointIndex = hit;
    }
    else
    {
        auto time = xToTime (e.x);
        if (editViewState.snapToGrid.get() && ! e.mods.isAltDown())
            time = TimelineGrid::snapTime (editViewState.edit, editViewState, time);

        const float value = parameter->valueRange.convertFrom0to1 (yToNormalisedValue (e.y));
        draggedPointIndex = curve.addPoint (time, value, 0.0f, um);
    }

    repaint();
}

void AutomationLaneComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (draggedPointIndex < 0)
        return;

    auto& curve = parameter->getCurve();
    auto* um = &editViewState.edit.getUndoManager();

    auto time = xToTime (juce::jlimit (getCurveArea().getX(), getCurveArea().getRight(), e.x));
    if (editViewState.snapToGrid.get() && ! e.mods.isAltDown())
        time = TimelineGrid::snapTime (editViewState.edit, editViewState, time);

    const float value = parameter->valueRange.convertFrom0to1 (yToNormalisedValue (e.y));

    draggedPointIndex = curve.movePoint (*parameter, draggedPointIndex,
                                         juce::jmax (te::TimePosition(), time), value, false, um);
    repaint();
}

void AutomationLaneComponent::mouseUp (const juce::MouseEvent&)
{
    draggedPointIndex = -1;
    repaint();
}

void AutomationLaneComponent::mouseMove (const juce::MouseEvent& e)
{
    const int hit = hitTestPoint (e.getPosition());
    if (hit != hoveredPointIndex)
    {
        hoveredPointIndex = hit;
        repaint();
    }

    setMouseCursor (hit >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

float AutomationLaneComponent::yToNormalisedValue (int y) const
{
    const auto area = getCurveArea();
    return 1.0f - juce::jlimit (0.0f, 1.0f,
                                (float) (y - area.getY()) / (float) juce::jmax (1, area.getHeight()));
}

int AutomationLaneComponent::normalisedValueToY (float normalised) const
{
    const auto area = getCurveArea();
    return area.getY() + juce::roundToInt ((1.0f - juce::jlimit (0.0f, 1.0f, normalised))
                                           * (float) juce::jmax (1, area.getHeight()));
}

te::TimePosition AutomationLaneComponent::xToTime (int x) const
{
    const auto area = getCurveArea();
    return editViewState.xToTimeInView (x - area.getX(), juce::jmax (1, area.getWidth()));
}

int AutomationLaneComponent::timeToX (te::TimePosition time) const
{
    const auto area = getCurveArea();
    return area.getX() + editViewState.timeToXInView (time, juce::jmax (1, area.getWidth()));
}

} // namespace skeletonhive
