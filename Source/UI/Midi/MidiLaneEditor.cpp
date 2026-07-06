#include "MidiLaneEditor.h"
#include "Engine/EngineHelpers.h"

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
    laneTypeBox.addItem ("Probability", (int) LaneType::probability);
    laneTypeBox.addItem ("Iteration", (int) LaneType::iteration);
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
    if (currentLane == LaneType::velocity
        || currentLane == LaneType::probability
        || currentLane == LaneType::iteration)
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

    if (currentLane == LaneType::velocity)
        paintVelocityLane (g, getLaneCanvasBounds());
    else if (currentLane == LaneType::probability)
        paintProbabilityLane (g, getLaneCanvasBounds());
    else if (currentLane == LaneType::iteration)
        paintIterationLane (g, getLaneCanvasBounds());
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

int MidiLaneEditor::probabilityFromY (int y, juce::Rectangle<int> area) const
{
    return juce::jlimit (0, 100,
        (int) std::round (100.0 * (area.getBottom() - 2 - y)
                          / (double) juce::jmax (1, area.getHeight() - 4)));
}

int MidiLaneEditor::iterationFromY (int y, juce::Rectangle<int> area) const
{
    return juce::jlimit (0, 16,
        (int) std::round (16.0 * (area.getBottom() - 2 - y)
                          / (double) juce::jmax (1, area.getHeight() - 4)));
}

void MidiLaneEditor::paintProbabilityLane (juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.setColour (juce::Colour (0xff141428));
    g.fillRect (area);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawHorizontalLine (area.getY(), (float) area.getX(), (float) area.getRight());

    const double offsetBeats = viewport.getClipOffsetBeats();

    for (auto* n : clip.getSequence().getNotes())
    {
        const int probability = EngineHelpers::getNoteProbability (n->state);
        const double startBeat = n->getStartBeat().inBeats() - offsetBeats;
        const float x = viewport.beatToX (startBeat);
        const float barHeight = (area.getHeight() - 4) * (probability / 100.0f);
        const bool selected = isNoteSelected (*n);

        g.setColour (selected ? juce::Colour (0xffffd166) : juce::Colour (0xff2a9d8f));
        g.fillRect (x, area.getBottom() - 2 - barHeight, 5.0f, barHeight);
    }
}

void MidiLaneEditor::paintIterationLane (juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.setColour (juce::Colour (0xff141428));
    g.fillRect (area);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawHorizontalLine (area.getY(), (float) area.getX(), (float) area.getRight());

    const double offsetBeats = viewport.getClipOffsetBeats();

    for (auto* n : clip.getSequence().getNotes())
    {
        const int iteration = EngineHelpers::getNoteIteration (n->state);
        const double startBeat = n->getStartBeat().inBeats() - offsetBeats;
        const float x = viewport.beatToX (startBeat);
        const float barHeight = iteration > 0 ? (area.getHeight() - 4) * (iteration / 16.0f) : 0.0f;
        const bool selected = isNoteSelected (*n);

        g.setColour (selected ? juce::Colour (0xffffd166) : juce::Colour (0xff9b5de5));
        g.fillRect (x, area.getBottom() - 2 - barHeight, 5.0f, juce::jmax (2.0f, barHeight));

        if (iteration > 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.setFont (juce::FontOptions (9.0f));
            g.drawText (juce::String (iteration), (int) x, area.getY(), 12, 12, juce::Justification::centred);
        }
    }
}

void MidiLaneEditor::applyLaneValueAt (const juce::MouseEvent& e, juce::Rectangle<int> area,
                                       std::function<void (te::MidiNote&, juce::UndoManager*)> applyValue)
{
    if (applyValue == nullptr)
        return;

    auto& sequence = clip.getSequence();
    auto* um = viewport.getUndoManager();
    const double offsetBeats = viewport.getClipOffsetBeats();

    juce::Array<juce::ValueTree> targets;

    for (auto* n : sequence.getNotes())
    {
        const float x = viewport.beatToX (n->getStartBeat().inBeats() - offsetBeats);
        if (e.x >= x - 1 && e.x <= x + 6 && isNoteSelected (*n))
        {
            targets = getSelection();
            break;
        }
    }

    if (targets.isEmpty())
    {
        for (auto* n : sequence.getNotes())
        {
            const float x = viewport.beatToX (n->getStartBeat().inBeats() - offsetBeats);
            if (e.x >= x - 1 && e.x <= x + 6)
            {
                applyValue (*n, um);
                repaint (area);
                if (onNoteVisualsChanged)
                    onNoteVisualsChanged();
                return;
            }
        }

        return;
    }

    for (const auto& state : targets)
        if (auto* n = sequence.getNoteFor (state))
            applyValue (*n, um);

    repaint (area);
    if (onNoteVisualsChanged)
        onNoteVisualsChanged();
}

void MidiLaneEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto area = getLaneCanvasBounds();
    if (! area.contains (e.getPosition()))
        return;

    if (currentLane == LaneType::velocity)
    {
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
        return;
    }

    if (currentLane == LaneType::probability)
    {
        dragStartPos = e.getPosition();
        applyLaneValueAt (e, area, [this, area, y = e.y] (te::MidiNote& note, juce::UndoManager* um)
        {
            EngineHelpers::setNoteProbability (note.state, probabilityFromY (y, area), um);
        });
        return;
    }

    if (currentLane == LaneType::iteration)
    {
        dragStartPos = e.getPosition();
        applyLaneValueAt (e, area, [this, area, y = e.y] (te::MidiNote& note, juce::UndoManager* um)
        {
            EngineHelpers::setNoteIteration (note.state, iterationFromY (y, area), um);
        });
    }
}

void MidiLaneEditor::mouseDrag (const juce::MouseEvent& e)
{
    const auto area = getLaneCanvasBounds();

    if (currentLane == LaneType::probability)
    {
        applyLaneValueAt (e, area, [this, area, y = e.y] (te::MidiNote& note, juce::UndoManager* um)
        {
            EngineHelpers::setNoteProbability (note.state, probabilityFromY (y, area), um);
        });
        return;
    }

    if (currentLane == LaneType::iteration)
    {
        applyLaneValueAt (e, area, [this, area, y = e.y] (te::MidiNote& note, juce::UndoManager* um)
        {
            EngineHelpers::setNoteIteration (note.state, iterationFromY (y, area), um);
        });
        return;
    }

    if (currentLane != LaneType::velocity || velocityDrag == VelocityDragMode::none)
        return;

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
