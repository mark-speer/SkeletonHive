#include "MidiLaneEditor.h"

namespace skeletonhive
{

namespace
{
juce::String ccLabel (int cc)
{
    switch (cc)
    {
        case 1:  return "1 Mod Wheel";
        case 7:  return "7 Volume";
        case 10: return "10 Pan";
        case 11: return "11 Expression";
        case 64: return "64 Sustain";
        default: return juce::String (cc) + " CC";
    }
}
} // namespace

MidiLaneEditor::MidiLaneEditor (te::MidiClip& c,
                                MidiLaneViewport& vp,
                                std::function<const juce::Array<juce::ValueTree>&()> sel,
                                std::function<bool (const te::MidiNote&)> noteSelected,
                                std::function<void()> noteVisualsChanged)
    : clip (c),
      viewport (vp),
      getSelection (std::move (sel)),
      isNoteSelected (std::move (noteSelected)),
      onNoteVisualsChanged (std::move (noteVisualsChanged))
{
    laneTypeBox.addItem ("Velocity", (int) LaneType::velocity);
    laneTypeBox.addItem ("CC", (int) LaneType::cc);
    laneTypeBox.addItem ("Pitch Bend", (int) LaneType::pitchBend);
    laneTypeBox.addItem ("Aftertouch", (int) LaneType::aftertouch);
    laneTypeBox.setSelectedId ((int) LaneType::velocity, juce::dontSendNotification);
    laneTypeBox.setTooltip ("Lane type");
    laneTypeBox.onChange = [this]
    {
        currentLane = (LaneType) laneTypeBox.getSelectedId();
        ccBox.setVisible (currentLane == LaneType::cc);
        updateActiveLane();
        repaint();
    };

    ccBox.setTooltip ("MIDI controller number");
    ccBox.onChange = [this]
    {
        selectedCc = ccBox.getSelectedId() - 1;
        if (controllerLane != nullptr)
            controllerLane->setControllerType (selectedCc);
    };

    addAndMakeVisible (laneTypeBox);
    addAndMakeVisible (ccBox);

    rebuildCcBox();
    updateActiveLane();
}

void MidiLaneEditor::rebuildCcBox()
{
    const int previousCc = selectedCc;
    ccBox.clear (juce::dontSendNotification);

    juce::SortedSet<int> ccNumbers;
    static constexpr int commonCcs[] = { 1, 7, 10, 11, 64 };

    for (int cc : commonCcs)
        ccNumbers.add (cc);

    auto& sequence = clip.getSequence();
    for (int cc = 0; cc <= 127; ++cc)
        if (sequence.containsController (cc))
            ccNumbers.add (cc);

    for (int cc : ccNumbers)
        ccBox.addItem (ccLabel (cc), cc + 1);

    if (ccNumbers.contains (previousCc))
        ccBox.setSelectedId (previousCc + 1, juce::dontSendNotification);
    else if (ccBox.getNumItems() > 0)
        ccBox.setSelectedId (ccNumbers[0] + 1, juce::dontSendNotification);

    selectedCc = ccBox.getSelectedId() - 1;
}

int MidiLaneEditor::activeControllerType() const
{
    switch (currentLane)
    {
        case LaneType::pitchBend:  return te::MidiControllerEvent::pitchWheelType;
        case LaneType::aftertouch: return te::MidiControllerEvent::aftertouchType;
        case LaneType::cc:         return selectedCc;
        default:                   return selectedCc;
    }
}

void MidiLaneEditor::updateActiveLane()
{
    if (currentLane == LaneType::velocity)
    {
        controllerLane.reset();
        return;
    }

    if (controllerLane == nullptr)
    {
        controllerLane = std::make_unique<MidiControllerLaneComponent> (clip, viewport, activeControllerType());
        addAndMakeVisible (*controllerLane);
    }
    else
    {
        controllerLane->setControllerType (activeControllerType());
    }

    resized();
}

void MidiLaneEditor::sequenceChanged()
{
    if (currentLane == LaneType::cc)
        rebuildCcBox();

    if (controllerLane != nullptr)
        controllerLane->sequenceChanged();
    else
        repaint();
}

void MidiLaneEditor::viewportChanged()
{
    if (controllerLane != nullptr)
        controllerLane->repaint();
    else
        repaint();
}

juce::Rectangle<int> MidiLaneEditor::getLaneCanvasBounds() const
{
    auto area = getLocalBounds();
    area.removeFromTop (selectorHeight);
    return area;
}

void MidiLaneEditor::resized()
{
    auto selectorRow = getLocalBounds().removeFromTop (selectorHeight).reduced (2, 1);
    ccBox.setVisible (currentLane == LaneType::cc);

    if (currentLane == LaneType::cc)
    {
        ccBox.setBounds (selectorRow.removeFromRight (ccBoxWidth));
        laneTypeBox.setBounds (selectorRow);
    }
    else
    {
        laneTypeBox.setBounds (selectorRow);
    }

    if (controllerLane != nullptr)
        controllerLane->setBounds (getLaneCanvasBounds());
}

void MidiLaneEditor::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff1a1a2e));
    g.fillRect (getLocalBounds().removeFromTop (selectorHeight));

    if (currentLane != LaneType::velocity)
        return;

    paintVelocityLane (g, getLaneCanvasBounds());
}

void MidiLaneEditor::paintVelocityLane (juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.setColour (juce::Colour (0xff141428));
    g.fillRect (area);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawHorizontalLine (area.getY(), (float) area.getX(), (float) area.getRight());

    const double offsetBeats = viewport.getClipOffsetBeats();

    for (auto* n : clip.getSequence().getNotes())
    {
        const double startBeat = n->getStartBeat().inBeats() - offsetBeats;
        const float x = viewport.beatToX (startBeat);
        const float barHeight = (area.getHeight() - 4) * (n->getVelocity() / 127.0f);
        const bool selected = isNoteSelected (*n);

        g.setColour (selected ? juce::Colour (0xffffd166) : juce::Colour (0xff4361ee));
        g.fillRect (x, area.getBottom() - 2 - barHeight, 5.0f, barHeight);
    }
}

int MidiLaneEditor::velocityFromY (int y, juce::Rectangle<int> area) const
{
    return juce::jlimit (1, 127,
        (int) std::round (127.0 * (area.getBottom() - 2 - y)
                          / (double) juce::jmax (1, area.getHeight() - 4)));
}

void MidiLaneEditor::mouseDown (const juce::MouseEvent& e)
{
    if (currentLane != LaneType::velocity)
        return;

    const auto area = getLaneCanvasBounds();
    if (! area.contains (e.getPosition()))
        return;

    dragStartPos = e.getPosition();
    velocityDrag = VelocityDragMode::pencil;
    velocityTargets.clear();

    const double offsetBeats = viewport.getClipOffsetBeats();
    for (auto* n : clip.getSequence().getNotes())
    {
        const float x = viewport.beatToX (n->getStartBeat().inBeats() - offsetBeats);
        if (e.x >= x - 1 && e.x <= x + 6 && isNoteSelected (*n))
        {
            velocityTargets = getSelection();
            velocityDrag = VelocityDragMode::selection;
            break;
        }
    }

    mouseDrag (e);
}

void MidiLaneEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (currentLane != LaneType::velocity || velocityDrag == VelocityDragMode::none)
        return;

    const auto area = getLaneCanvasBounds();
    const int newVelocity = velocityFromY (e.y, area);
    auto& sequence = clip.getSequence();
    auto* um = viewport.getUndoManager();

    if (velocityDrag == VelocityDragMode::selection)
    {
        for (const auto& state : velocityTargets)
            if (auto* n = sequence.getNoteFor (state))
                n->setVelocity (newVelocity, um);
    }
    else
    {
        const double offsetBeats = viewport.getClipOffsetBeats();
        for (auto* n : sequence.getNotes())
        {
            const float x = viewport.beatToX (n->getStartBeat().inBeats() - offsetBeats);
            if (e.x >= x - 1 && e.x <= x + 6)
                n->setVelocity (newVelocity, um);
        }
    }

    repaint (area);
    if (onNoteVisualsChanged)
        onNoteVisualsChanged();
}

void MidiLaneEditor::mouseUp (const juce::MouseEvent&)
{
    velocityDrag = VelocityDragMode::none;
    velocityTargets.clear();
}

} // namespace skeletonhive
