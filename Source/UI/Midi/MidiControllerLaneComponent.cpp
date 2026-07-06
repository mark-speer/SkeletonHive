#include "MidiControllerLaneComponent.h"

namespace skeletonhive
{

MidiControllerLaneComponent::MidiControllerLaneComponent (te::MidiClip& c, MidiLaneViewport& vp, int type)
    : clip (c), viewport (vp), controllerType (type)
{
    setInterceptsMouseClicks (true, false);
}

void MidiControllerLaneComponent::setControllerType (int newType)
{
    if (controllerType == newType)
        return;

    controllerType = newType;
    draggedEvent = nullptr;
    hoveredEvent = nullptr;
    repaint();
}

MidiControllerLaneComponent::ValueRange MidiControllerLaneComponent::getValueRange() const
{
    if (controllerType == te::MidiControllerEvent::pitchWheelType)
        return { 0, pitchWheelMax, pitchWheelCentre, true };

    return { 0, 127, te::MidiControllerEvent::getControllerDefautValue (controllerType), false };
}

juce::Rectangle<int> MidiControllerLaneComponent::getCurveArea() const
{
    return getLocalBounds().reduced (2, 2);
}

double MidiControllerLaneComponent::clipRelativeBeat (const te::MidiControllerEvent& event) const
{
    return event.getBeatPosition().inBeats() - viewport.getClipOffsetBeats();
}

te::BeatPosition MidiControllerLaneComponent::sequenceBeat (double clipRelativeBeat) const
{
    return te::BeatPosition::fromBeats (clipRelativeBeat + viewport.getClipOffsetBeats());
}

juce::Array<te::MidiControllerEvent*> MidiControllerLaneComponent::getEventsForType() const
{
    juce::Array<te::MidiControllerEvent*> result;

    for (auto* event : clip.getSequence().getControllerEvents())
        if (event->getType() == controllerType)
            result.add (event);

    return result;
}

te::MidiControllerEvent* MidiControllerLaneComponent::eventAtPosition (juce::Point<int> pos) const
{
    const int hit = hitTestPoint (pos);
    if (hit < 0)
        return nullptr;

    return getEventsForType()[hit];
}

int MidiControllerLaneComponent::hitTestPoint (juce::Point<int> pos) const
{
    const auto events = getEventsForType();

    for (int i = 0; i < events.size(); ++i)
    {
        const int x = (int) viewport.beatToX (clipRelativeBeat (*events[i]));
        const int y = valueToY (events[i]->getControllerValue());

        if (pos.getDistanceFrom ({ x, y }) <= pointHitRadius)
            return i;
    }

    return -1;
}

int MidiControllerLaneComponent::yToValue (int y) const
{
    const auto area = getCurveArea();
    const auto range = getValueRange();
    const float normalised = 1.0f - (float) (y - area.getY()) / (float) juce::jmax (1, area.getHeight());
    const int raw = range.min + (int) std::round (normalised * (float) (range.max - range.min));
    return juce::jlimit (range.min, range.max, raw);
}

int MidiControllerLaneComponent::valueToY (int value) const
{
    const auto area = getCurveArea();
    const auto range = getValueRange();
    const float normalised = (float) (value - range.min) / (float) juce::jmax (1, range.max - range.min);
    return area.getY() + (int) std::round ((1.0f - normalised) * (float) area.getHeight());
}

void MidiControllerLaneComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141428));

    const auto area = getCurveArea();
    const auto range = getValueRange();

    if (range.bipolar)
    {
        const int centreY = valueToY (range.centre);
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.drawHorizontalLine (centreY, (float) area.getX(), (float) area.getRight());
    }

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine (area.getBottom(), (float) area.getX(), (float) area.getRight());

    const auto events = getEventsForType();

    if (events.size() > 1)
    {
        juce::Path path;
        bool started = false;

        for (auto* event : events)
        {
            const float x = viewport.beatToX (clipRelativeBeat (*event));
            const float y = (float) valueToY (event->getControllerValue());

            if (x < (float) area.getX() - pointHitRadius && started)
                continue;

            if (! started)
            {
                path.startNewSubPath (x, y);
                started = true;
            }
            else
            {
                path.lineTo (x, y);
            }
        }

        g.setColour (juce::Colour (0xff4361ee).withAlpha (0.85f));
        g.strokePath (path, juce::PathStrokeType (1.5f));
    }

    for (auto* event : events)
    {
        const int x = (int) viewport.beatToX (clipRelativeBeat (*event));
        if (x < area.getX() - pointHitRadius || x > area.getRight() + pointHitRadius)
            continue;

        const int y = valueToY (event->getControllerValue());
        const bool highlighted = event == hoveredEvent || event == draggedEvent;

        g.setColour (highlighted ? juce::Colour (0xffffd166) : juce::Colour (0xff4361ee));
        g.fillEllipse ((float) x - 3.5f, (float) y - 3.5f, 7.0f, 7.0f);
    }
}

void MidiControllerLaneComponent::mouseDown (const juce::MouseEvent& e)
{
    if (! getCurveArea().contains (e.getPosition()))
        return;

    auto* um = viewport.getUndoManager();
    auto& sequence = clip.getSequence();

    if (e.mods.isPopupMenu())
    {
        if (auto* hit = eventAtPosition (e.getPosition()))
        {
            sequence.removeControllerEvent (*hit, um);
            repaint();
        }
        return;
    }

    if (auto* hit = eventAtPosition (e.getPosition()))
    {
        draggedEvent = hit;
        pencilMode = false;
    }
    else
    {
        draggedEvent = nullptr;
        pencilMode = true;

        const double beat = viewport.snapBeat (viewport.xToBeat (e.x), e.mods.isAltDown());
        const int value = yToValue (e.y);
        sequence.setControllerValueAt (controllerType, sequenceBeat (beat), value, um);
    }

    mouseDrag (e);
}

void MidiControllerLaneComponent::mouseDrag (const juce::MouseEvent& e)
{
    auto* um = viewport.getUndoManager();
    auto& sequence = clip.getSequence();
    const double beat = viewport.snapBeat (viewport.xToBeat (e.x), e.mods.isAltDown());
    const int value = yToValue (e.y);

    if (draggedEvent != nullptr)
    {
        draggedEvent->setBeatPosition (sequenceBeat (beat), um);
        draggedEvent->setControllerValue (value, um);
    }
    else if (pencilMode)
    {
        sequence.setControllerValueAt (controllerType, sequenceBeat (beat), value, um);
    }

    repaint();
}

void MidiControllerLaneComponent::mouseUp (const juce::MouseEvent&)
{
    draggedEvent = nullptr;
    pencilMode = false;
    repaint();
}

void MidiControllerLaneComponent::mouseMove (const juce::MouseEvent& e)
{
    te::MidiControllerEvent* hit = getCurveArea().contains (e.getPosition())
                                     ? eventAtPosition (e.getPosition())
                                     : nullptr;

    if (hit != hoveredEvent)
    {
        hoveredEvent = hit;
        repaint();
    }

    setMouseCursor (hit != nullptr ? juce::MouseCursor::PointingHandCursor
                                   : juce::MouseCursor::NormalCursor);
}

} // namespace skeletonhive
