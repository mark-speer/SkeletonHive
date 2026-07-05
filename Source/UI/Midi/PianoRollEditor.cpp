#include "PianoRollEditor.h"
#include "UI/Arrangement/TimelineGrid.h"
#include "TracktionCommon.h"

#include <cmath>
#include <limits>

namespace arrange
{

namespace
{
constexpr int scaleMasks[][12] =
{
    { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 },  // Major
    { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0 }   // Natural minor
};

const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

bool isBlackKey (int pitch)
{
    const int pc = pitch % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

/** Maps a grid interval in beats to the closest Tracktion QuantisationType name. */
juce::String quantiseTypeNameForInterval (double intervalBeats)
{
    struct Mapping { double beats; const char* name; };
    static constexpr Mapping mappings[] =
    {
        { 1.0 / 16.0, "1/16 beat" },
        { 1.0 / 8.0,  "1/8 beat" },
        { 1.0 / 4.0,  "1/4 beat" },
        { 1.0 / 2.0,  "1/2 beat" },
        { 1.0,        "1 beat" }
    };

    const Mapping* best = &mappings[0];
    for (const auto& m : mappings)
        if (std::abs (m.beats - intervalBeats) < std::abs (best->beats - intervalBeats))
            best = &m;

    return best->name;
}
} // namespace

PianoRollEditor::PianoRollEditor (te::MidiClip& c, te::Edit& e, EditViewState& evs)
    : clip (&c), edit (e), editViewState (evs)
{
    quantiseButton.setTooltip ("Quantize selected notes to the grid (Q)");
    quantiseButton.onClick = [this] { quantiseNotes(); };

    humaniseButton.setTooltip ("Randomise timing/velocity of selected notes (H)");
    humaniseButton.onClick = [this] { humaniseNotes(); };

    foldButton.setTooltip ("Show only pitches used in this clip (F)");
    foldButton.onClick = [this]
    {
        rebuildFoldedPitches();
        repaint();
    };

    for (int i = 0; i < 12; ++i)
        rootBox.addItem (noteNames[i], i + 1);
    rootBox.setSelectedId (1, juce::dontSendNotification);
    rootBox.onChange = [this] { repaint (gridBounds); repaint (keyboardBounds); };

    scaleBox.addItem ("No Scale", 1);
    scaleBox.addItem ("Major", 2);
    scaleBox.addItem ("Minor", 3);
    scaleBox.setSelectedId (1, juce::dontSendNotification);
    scaleBox.onChange = [this] { repaint (gridBounds); repaint (keyboardBounds); };

    addAndMakeVisible (quantiseButton);
    addAndMakeVisible (humaniseButton);
    addAndMakeVisible (foldButton);
    addAndMakeVisible (rootBox);
    addAndMakeVisible (scaleBox);

    setWantsKeyboardFocus (true);
    clip->state.addListener (this);
}

PianoRollEditor::~PianoRollEditor()
{
    stopAudition();
    clip->state.removeListener (this);
}

juce::UndoManager* PianoRollEditor::getUndoManager() const
{
    return &edit.getUndoManager();
}

//==============================================================================
// Model change notifications (covers undo/redo and external edits)

void PianoRollEditor::valueTreeChanged()
{
    pruneSelection();

    if (foldButton.getToggleState() && dragMode == DragMode::none)
        rebuildFoldedPitches();

    repaint();
}

void PianoRollEditor::pruneSelection()
{
    for (int i = selection.size(); --i >= 0;)
        if (! selection.getReference (i).getParent().isValid())
            selection.remove (i);
}

//==============================================================================
// Geometry

void PianoRollEditor::resized()
{
    auto r = getLocalBounds();
    toolbarBounds = r.removeFromTop (toolbarHeight);
    velocityBounds = r.removeFromBottom (velocityLaneHeight);
    keyboardBounds = r.removeFromLeft (keyboardWidth);
    gridBounds = r;
    velocityBounds.removeFromLeft (keyboardWidth);

    auto tb = toolbarBounds.reduced (3);
    quantiseButton.setBounds (tb.removeFromLeft (76).reduced (1));
    humaniseButton.setBounds (tb.removeFromLeft (80).reduced (1));
    foldButton.setBounds (tb.removeFromLeft (60).reduced (1));
    rootBox.setBounds (tb.removeFromLeft (58).reduced (1));
    scaleBox.setBounds (tb.removeFromLeft (86).reduced (1));
}

void PianoRollEditor::rebuildFoldedPitches()
{
    foldedPitches.clear();

    if (! foldButton.getToggleState())
        return;

    juce::SortedSet<int> pitches;
    for (auto* n : clip->getSequence().getNotes())
        pitches.add (juce::jlimit (lowestNote, highestNote, n->getNoteNumber()));

    for (int i = pitches.size(); --i >= 0;)   // descending for top-down rows
        foldedPitches.add (pitches[i]);
}

int PianoRollEditor::numVisibleRows() const
{
    if (foldButton.getToggleState() && ! foldedPitches.isEmpty())
        return foldedPitches.size();

    return highestNote - lowestNote + 1;
}

int PianoRollEditor::pitchForRow (int row) const
{
    if (foldButton.getToggleState() && ! foldedPitches.isEmpty())
        return foldedPitches[juce::jlimit (0, foldedPitches.size() - 1, row)];

    return juce::jlimit (lowestNote, highestNote, highestNote - row);
}

int PianoRollEditor::rowForPitch (int pitch) const
{
    if (foldButton.getToggleState() && ! foldedPitches.isEmpty())
        return foldedPitches.indexOf (pitch);

    if (pitch < lowestNote || pitch > highestNote)
        return -1;

    return highestNote - pitch;
}

float PianoRollEditor::rowHeight() const
{
    return gridBounds.getHeight() / (float) juce::jmax (1, numVisibleRows());
}

int PianoRollEditor::pitchAtY (int y) const
{
    const int row = (int) ((y - gridBounds.getY()) / rowHeight());
    return pitchForRow (juce::jlimit (0, numVisibleRows() - 1, row));
}

int PianoRollEditor::yForPitchTop (int pitch) const
{
    const int row = rowForPitch (pitch);
    if (row < 0)
        return -1000;
    return gridBounds.getY() + (int) (row * rowHeight());
}

double PianoRollEditor::clipLengthBeats() const
{
    const auto& ts = edit.tempoSequence;
    const auto pos = clip->getPosition();
    const double len = (ts.toBeats (pos.getEnd()) - ts.toBeats (pos.getStart())).inBeats();
    return juce::jmax (0.25, len);
}

double PianoRollEditor::xToBeat (int x) const
{
    const double proportion = (x - gridBounds.getX()) / (double) juce::jmax (1, gridBounds.getWidth());
    return juce::jlimit (0.0, clipLengthBeats(), proportion * clipLengthBeats());
}

float PianoRollEditor::beatToX (double beat) const
{
    return gridBounds.getX() + (float) (beat / clipLengthBeats()) * (float) gridBounds.getWidth();
}

double PianoRollEditor::gridIntervalBeats() const
{
    return TimelineGrid::gridIntervalBeats (edit, editViewState);
}

double PianoRollEditor::snapBeat (double beat) const
{
    if (juce::ModifierKeys::getCurrentModifiers().isAltDown() || ! editViewState.snapToGrid.get())
        return beat;

    const auto interval = gridIntervalBeats();
    return std::round (beat / interval) * interval;
}

double PianoRollEditor::beatsPerBar() const
{
    return TimelineGrid::beatsPerBar (edit, clip->getPosition().getStart());
}

juce::Rectangle<float> PianoRollEditor::rectForNote (const te::MidiNote& note) const
{
    const double offsetBeats = clip->getOffsetInBeats().inBeats();
    const double startBeat = note.getStartBeat().inBeats() - offsetBeats;
    const double lengthBeats = note.getLengthBeats().inBeats();

    const int y = yForPitchTop (note.getNoteNumber());
    const float x1 = beatToX (startBeat);
    const float x2 = beatToX (startBeat + lengthBeats);

    return { x1, (float) y, juce::jmax (3.0f, x2 - x1), juce::jmax (2.0f, rowHeight() - 1.0f) };
}

te::MidiNote* PianoRollEditor::noteAtPosition (juce::Point<int> pos) const
{
    const auto& notes = clip->getSequence().getNotes();

    // Iterate backwards so later (topmost-drawn) notes win
    for (int i = notes.size(); --i >= 0;)
        if (rectForNote (*notes[i]).contains (pos.toFloat()))
            return notes[i];

    return nullptr;
}

bool PianoRollEditor::isSelected (const te::MidiNote& note) const
{
    return selection.contains (note.state);
}

juce::Array<te::MidiNote*> PianoRollEditor::getSelectedNotes() const
{
    juce::Array<te::MidiNote*> result;
    auto& sequence = clip->getSequence();

    for (const auto& state : selection)
        if (auto* n = sequence.getNoteFor (state))
            result.add (n);

    return result;
}

juce::Array<te::MidiNote*> PianoRollEditor::getTargetNotes() const
{
    auto selected = getSelectedNotes();
    if (! selected.isEmpty())
        return selected;

    return clip->getSequence().getNotes();
}

void PianoRollEditor::captureDragOrigins()
{
    dragOrigins.clear();
    const double offsetBeats = clip->getOffsetInBeats().inBeats();

    for (auto* n : getSelectedNotes())
        dragOrigins.add ({ n->state,
                           n->getStartBeat().inBeats() - offsetBeats,
                           n->getLengthBeats().inBeats(),
                           n->getNoteNumber() });
}

//==============================================================================
// Painting

void PianoRollEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0f0f23));

    g.setColour (juce::Colour (0xff1a1a2e));
    g.fillRect (toolbarBounds);

    paintKeyboard (g);
    paintGrid (g);
    paintGhostNotes (g);
    paintNotes (g);
    paintVelocityLane (g);

    if (dragMode == DragMode::marquee)
    {
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRect (marqueeRect);
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.drawRect (marqueeRect);
    }
}

bool PianoRollEditor::isPitchInScale (int pitch) const
{
    const int scaleIdx = scaleBox.getSelectedId() - 2;
    if (scaleIdx < 0)
        return false;

    const int root = rootBox.getSelectedId() - 1;
    return scaleMasks[scaleIdx][((pitch - root) % 12 + 12) % 12] != 0;
}

void PianoRollEditor::paintKeyboard (juce::Graphics& g) const
{
    const float h = rowHeight();

    for (int row = 0; row < numVisibleRows(); ++row)
    {
        const int pitch = pitchForRow (row);
        const float y = gridBounds.getY() + row * h;
        const bool black = isBlackKey (pitch);

        g.setColour (black ? juce::Colour (0xff20203a) : juce::Colour (0xffd8d8e8));
        g.fillRect ((float) keyboardBounds.getX(), y, (float) keyboardBounds.getWidth(), h);
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.drawHorizontalLine ((int) y, (float) keyboardBounds.getX(), (float) keyboardBounds.getRight());

        if (pitch % 12 == 0 && h >= 7.0f)   // label Cs
        {
            g.setColour (black ? juce::Colours::white : juce::Colours::black);
            g.setFont (juce::FontOptions (juce::jmin (11.0f, h - 1.0f)));
            g.drawText (juce::String (noteNames[pitch % 12]) + juce::String (pitch / 12 - 1),
                        keyboardBounds.getX() + 2, (int) y, keyboardBounds.getWidth() - 4, (int) h,
                        juce::Justification::centredLeft, false);
        }
    }
}

void PianoRollEditor::paintGrid (juce::Graphics& g) const
{
    const float h = rowHeight();
    const bool scaleOn = scaleBox.getSelectedId() > 1;

    for (int row = 0; row < numVisibleRows(); ++row)
    {
        const int pitch = pitchForRow (row);
        const float y = gridBounds.getY() + row * h;

        juce::Colour rowColour;
        if (scaleOn)
            rowColour = isPitchInScale (pitch) ? juce::Colour (0xff1c1c38) : juce::Colour (0xff121226);
        else
            rowColour = isBlackKey (pitch) ? juce::Colour (0xff14142a) : juce::Colour (0xff1a1a34);

        if (scaleOn && ((pitch - (rootBox.getSelectedId() - 1)) % 12 + 12) % 12 == 0)
            rowColour = rowColour.brighter (0.12f);

        g.setColour (rowColour);
        g.fillRect ((float) gridBounds.getX(), y, (float) gridBounds.getWidth(), h);

        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine ((int) y, (float) gridBounds.getX(), (float) gridBounds.getRight());
    }

    // Vertical grid lines, aligned with the edit's bar structure
    const double interval = gridIntervalBeats();
    const double barBeats = beatsPerBar();
    const double clipStartBeats = edit.tempoSequence.toBeats (clip->getPosition().getStart()).inBeats();
    const double lengthBeats = clipLengthBeats();

    for (double b = 0.0; b <= lengthBeats + 1.0e-9; b += interval)
    {
        const float x = beatToX (b);
        const double editBeat = clipStartBeats + b;
        const double barRemainder = std::abs (editBeat / barBeats - std::round (editBeat / barBeats));
        const double beatRemainder = std::abs (editBeat - std::round (editBeat));

        if (barRemainder < 1.0e-6)
            g.setColour (juce::Colours::white.withAlpha (0.35f));
        else if (beatRemainder < 1.0e-6)
            g.setColour (juce::Colours::white.withAlpha (0.15f));
        else
            g.setColour (juce::Colours::white.withAlpha (0.06f));

        g.drawVerticalLine ((int) x, (float) gridBounds.getY(), (float) gridBounds.getBottom());
    }
}

void PianoRollEditor::paintGhostNotes (juce::Graphics& g) const
{
    auto* track = dynamic_cast<te::ClipTrack*> (clip->getTrack());
    if (track == nullptr)
        return;

    const auto& ts = edit.tempoSequence;
    const double thisClipStartBeats = ts.toBeats (clip->getPosition().getStart()).inBeats();
    const double lengthBeats = clipLengthBeats();

    g.setColour (juce::Colours::white.withAlpha (0.16f));

    for (auto* c : track->getClips())
    {
        auto* other = dynamic_cast<te::MidiClip*> (c);
        if (other == nullptr || other == clip.get())
            continue;

        for (auto* n : other->getSequence().getNotes())
        {
            const double startBeat = ts.toBeats (n->getEditStartTime (*other)).inBeats() - thisClipStartBeats;
            const double noteLen = n->getLengthBeats().inBeats();

            if (startBeat + noteLen < 0.0 || startBeat > lengthBeats)
                continue;

            const int y = yForPitchTop (n->getNoteNumber());
            if (y < gridBounds.getY() - 1)
                continue;

            const float x1 = beatToX (juce::jmax (0.0, startBeat));
            const float x2 = beatToX (juce::jmin (lengthBeats, startBeat + noteLen));
            g.fillRect (x1, (float) y, juce::jmax (2.0f, x2 - x1), juce::jmax (2.0f, rowHeight() - 1.0f));
        }
    }
}

void PianoRollEditor::paintNotes (juce::Graphics& g) const
{
    for (auto* n : clip->getSequence().getNotes())
    {
        const auto rect = rectForNote (*n);
        if (! rect.intersects (gridBounds.toFloat()))
            continue;

        const bool selected = isSelected (*n);
        const float velocityIntensity = 0.5f + 0.5f * (n->getVelocity() / 127.0f);

        g.setColour (selected ? juce::Colour (0xffffd166)
                              : juce::Colour (0xff4361ee).withMultipliedBrightness (velocityIntensity));
        g.fillRoundedRectangle (rect, 2.0f);
        g.setColour (juce::Colours::white.withAlpha (selected ? 0.9f : 0.4f));
        g.drawRoundedRectangle (rect, 2.0f, 1.0f);
    }
}

void PianoRollEditor::paintVelocityLane (juce::Graphics& g) const
{
    g.setColour (juce::Colour (0xff141428));
    g.fillRect (velocityBounds);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawHorizontalLine (velocityBounds.getY(), (float) velocityBounds.getX(), (float) velocityBounds.getRight());

    const double offsetBeats = clip->getOffsetInBeats().inBeats();

    for (auto* n : clip->getSequence().getNotes())
    {
        const double startBeat = n->getStartBeat().inBeats() - offsetBeats;
        const float x = beatToX (startBeat);
        const float barHeight = (velocityBounds.getHeight() - 4) * (n->getVelocity() / 127.0f);
        const bool selected = isSelected (*n);

        g.setColour (selected ? juce::Colour (0xffffd166) : juce::Colour (0xff4361ee));
        g.fillRect (x, velocityBounds.getBottom() - 2 - barHeight, 5.0f, barHeight);
    }
}

//==============================================================================
// Mouse interaction

void PianoRollEditor::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    dragStartPos = e.getPosition();
    dragMode = DragMode::none;

    if (keyboardBounds.contains (e.getPosition()))
    {
        auditionPitch (pitchAtY (e.y), defaultVelocity);
        return;
    }

    if (velocityBounds.contains (e.getPosition()))
    {
        dragMode = DragMode::velocity;
        velocityTargets.clear();
        velocityPaintMode = true;

        // If dragging a selected note's bar, edit the whole selection; otherwise paint per-note
        for (auto* n : clip->getSequence().getNotes())
        {
            const double startBeat = n->getStartBeat().inBeats() - clip->getOffsetInBeats().inBeats();
            const float x = beatToX (startBeat);
            if (e.x >= x - 1 && e.x <= x + 6 && isSelected (*n))
            {
                velocityTargets = selection;
                velocityPaintMode = false;
                break;
            }
        }

        mouseDrag (e);
        return;
    }

    if (! gridBounds.contains (e.getPosition()))
        return;

    auto* hitNote = noteAtPosition (e.getPosition());

    if (e.mods.isRightButtonDown())
    {
        if (hitNote != nullptr)
        {
            if (isSelected (*hitNote))
            {
                deleteSelectedNotes();
            }
            else
            {
                clip->getSequence().removeNote (*hitNote, getUndoManager());
                repaint (gridBounds);
                repaint (velocityBounds);
            }
        }
        return;
    }

    if (hitNote != nullptr)
    {
        if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        {
            // Toggle selection membership, no drag
            if (isSelected (*hitNote))
                selection.removeAllInstancesOf (hitNote->state);
            else
                selection.add (hitNote->state);
            repaint (gridBounds);
            repaint (velocityBounds);
            return;
        }

        if (e.mods.isShiftDown())
        {
            if (! isSelected (*hitNote))
                selection.add (hitNote->state);
        }
        else if (! isSelected (*hitNote))
        {
            selection.clearQuick();
            selection.add (hitNote->state);
        }

        const auto rect = rectForNote (*hitNote);
        if (rect.getWidth() > resizeHandlePx * 2 + 2 && e.x >= rect.getRight() - resizeHandlePx)
            dragMode = DragMode::resizeEnd;
        else if (rect.getWidth() > resizeHandlePx * 2 + 2 && e.x <= rect.getX() + resizeHandlePx)
            dragMode = DragMode::resizeStart;
        else
            dragMode = DragMode::move;

        dragAnchorBeat = hitNote->getStartBeat().inBeats() - clip->getOffsetInBeats().inBeats();
        dragAnchorPitch = hitNote->getNoteNumber();
        captureDragOrigins();
        auditionPitch (hitNote->getNoteNumber(), hitNote->getVelocity());
        repaint (gridBounds);
        repaint (velocityBounds);
        return;
    }

    // Empty area: start marquee selection
    if (! e.mods.isShiftDown())
        selection.clearQuick();

    preMarqueeSelection = selection;
    dragMode = DragMode::marquee;
    marqueeRect = { e.x, e.y, 0, 0 };
    repaint (gridBounds);
    repaint (velocityBounds);
}

void PianoRollEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! gridBounds.contains (e.getPosition()))
        return;

    if (noteAtPosition (e.getPosition()) != nullptr)
        return;

    // Create a note one grid interval long
    const double interval = gridIntervalBeats();
    const double startBeat = snapBeat (xToBeat (e.x));
    const int pitch = pitchAtY (e.y);
    const double offsetBeats = clip->getOffsetInBeats().inBeats();

    auto* note = clip->getSequence().addNote (pitch,
                                              te::BeatPosition::fromBeats (startBeat + offsetBeats),
                                              te::BeatDuration::fromBeats (interval),
                                              defaultVelocity, 0, getUndoManager());
    if (note != nullptr)
    {
        selection.clearQuick();
        selection.add (note->state);
        auditionPitch (pitch, defaultVelocity);

        // Allow immediate length adjustment by continuing to drag
        dragMode = DragMode::resizeEnd;
        dragAnchorBeat = startBeat;
        dragAnchorPitch = pitch;
        captureDragOrigins();
    }

    repaint (gridBounds);
    repaint (velocityBounds);
}

void PianoRollEditor::mouseDrag (const juce::MouseEvent& e)
{
    switch (dragMode)
    {
        case DragMode::none:
            return;

        case DragMode::marquee:
        {
            marqueeRect = juce::Rectangle<int>::leftTopRightBottom (
                juce::jmin (dragStartPos.x, e.x), juce::jmin (dragStartPos.y, e.y),
                juce::jmax (dragStartPos.x, e.x), juce::jmax (dragStartPos.y, e.y));

            selection = preMarqueeSelection;
            for (auto* n : clip->getSequence().getNotes())
                if (rectForNote (*n).intersects (marqueeRect.toFloat()) && ! selection.contains (n->state))
                    selection.add (n->state);

            repaint (gridBounds);
            repaint (velocityBounds);
            return;
        }

        case DragMode::velocity:
        {
            const int newVelocity = juce::jlimit (1, 127,
                (int) std::round (127.0 * (velocityBounds.getBottom() - 2 - e.y)
                                  / (double) juce::jmax (1, velocityBounds.getHeight() - 4)));

            auto& sequence = clip->getSequence();

            if (! velocityPaintMode)
            {
                for (const auto& state : velocityTargets)
                    if (auto* n = sequence.getNoteFor (state))
                        n->setVelocity (newVelocity, getUndoManager());
            }
            else
            {
                // Pencil mode: set velocity of any note whose bar is under the cursor
                const double offsetBeats = clip->getOffsetInBeats().inBeats();
                for (auto* n : sequence.getNotes())
                {
                    const float x = beatToX (n->getStartBeat().inBeats() - offsetBeats);
                    if (e.x >= x - 1 && e.x <= x + 6)
                        n->setVelocity (newVelocity, getUndoManager());
                }
            }

            repaint (velocityBounds);
            repaint (gridBounds);
            return;
        }

        case DragMode::move:
        {
            const double rawDelta = xToBeat (e.x) - xToBeat (dragStartPos.x);
            const double snappedStart = snapBeat (dragAnchorBeat + rawDelta);
            const double beatDelta = snappedStart - dragAnchorBeat;

            const int anchorRow = rowForPitch (dragAnchorPitch);
            const int currentRow = juce::jlimit (0, numVisibleRows() - 1,
                                                 (int) ((e.y - gridBounds.getY()) / rowHeight()));
            const int rowDelta = anchorRow >= 0 ? currentRow - anchorRow : 0;

            const double offsetBeats = clip->getOffsetInBeats().inBeats();
            const double lengthBeats = clipLengthBeats();
            auto& sequence = clip->getSequence();

            int auditionedPitch = -1;

            for (const auto& origin : dragOrigins)
            {
                auto* n = sequence.getNoteFor (origin.state);
                if (n == nullptr)
                    continue;

                const double newStart = juce::jlimit (0.0, juce::jmax (0.0, lengthBeats - origin.lengthBeats),
                                                      origin.startBeat + beatDelta);

                int newPitch = origin.pitch;
                if (rowDelta != 0)
                {
                    const int originRow = rowForPitch (origin.pitch);
                    if (originRow >= 0)
                        newPitch = pitchForRow (juce::jlimit (0, numVisibleRows() - 1, originRow + rowDelta));
                }

                n->setStartAndLength (te::BeatPosition::fromBeats (newStart + offsetBeats),
                                      te::BeatDuration::fromBeats (origin.lengthBeats),
                                      getUndoManager());
                n->setNoteNumber (newPitch, getUndoManager());

                if (origin.pitch == dragAnchorPitch)
                    auditionedPitch = newPitch;
            }

            if (auditionedPitch >= 0 && auditionedPitch != lastAuditionedPitch)
                auditionPitch (auditionedPitch, defaultVelocity);

            repaint (gridBounds);
            repaint (velocityBounds);
            return;
        }

        case DragMode::resizeEnd:
        {
            const double snappedEnd = snapBeat (xToBeat (e.x));
            const double offsetBeats = clip->getOffsetInBeats().inBeats();
            auto& sequence = clip->getSequence();

            for (const auto& origin : dragOrigins)
            {
                if (auto* n = sequence.getNoteFor (origin.state))
                {
                    const double newLength = juce::jmax (minNoteLengthBeats, snappedEnd - origin.startBeat);
                    n->setStartAndLength (te::BeatPosition::fromBeats (origin.startBeat + offsetBeats),
                                          te::BeatDuration::fromBeats (newLength),
                                          getUndoManager());
                }
            }

            repaint (gridBounds);
            return;
        }

        case DragMode::resizeStart:
        {
            const double snappedStart = snapBeat (xToBeat (e.x));
            const double offsetBeats = clip->getOffsetInBeats().inBeats();
            auto& sequence = clip->getSequence();

            for (const auto& origin : dragOrigins)
            {
                if (auto* n = sequence.getNoteFor (origin.state))
                {
                    const double origEnd = origin.startBeat + origin.lengthBeats;
                    const double newStart = juce::jlimit (0.0, origEnd - minNoteLengthBeats, snappedStart);
                    n->setStartAndLength (te::BeatPosition::fromBeats (newStart + offsetBeats),
                                          te::BeatDuration::fromBeats (origEnd - newStart),
                                          getUndoManager());
                }
            }

            repaint (gridBounds);
            return;
        }
    }
}

void PianoRollEditor::mouseUp (const juce::MouseEvent&)
{
    dragMode = DragMode::none;
    velocityTargets.clear();
    stopAudition();
    repaint();
}

void PianoRollEditor::mouseMove (const juce::MouseEvent& e)
{
    if (dragMode != DragMode::none)
        return;

    if (auto* n = noteAtPosition (e.getPosition()))
    {
        const auto rect = rectForNote (*n);
        if (rect.getWidth() > resizeHandlePx * 2 + 2
            && (e.x >= rect.getRight() - resizeHandlePx || e.x <= rect.getX() + resizeHandlePx))
        {
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            return;
        }
    }

    setMouseCursor (juce::MouseCursor::NormalCursor);
}

//==============================================================================
// Keyboard shortcuts

bool PianoRollEditor::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();

    if (key.getKeyCode() == juce::KeyPress::deleteKey || key.getKeyCode() == juce::KeyPress::backspaceKey)
    {
        deleteSelectedNotes();
        return true;
    }
    if (key == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0)
        || key == juce::KeyPress ('a', juce::ModifierKeys::ctrlModifier, 0))
    {
        selectAllNotes();
        return true;
    }
    if (key == juce::KeyPress ('d', juce::ModifierKeys::commandModifier, 0)
        || key == juce::KeyPress ('d', juce::ModifierKeys::ctrlModifier, 0))
    {
        duplicateSelectedNotes();
        return true;
    }
    if (key.getKeyCode() == 'Q' || key.getKeyCode() == 'q')
    {
        quantiseNotes();
        return true;
    }
    if (key.getKeyCode() == 'H' || key.getKeyCode() == 'h')
    {
        humaniseNotes();
        return true;
    }
    if (key.getKeyCode() == 'F' || key.getKeyCode() == 'f')
    {
        foldButton.setToggleState (! foldButton.getToggleState(), juce::dontSendNotification);
        rebuildFoldedPitches();
        repaint();
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        selection.clearQuick();
        repaint();
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::leftKey)
    {
        nudgeSelectedNotes (-gridIntervalBeats(), 0);
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::rightKey)
    {
        nudgeSelectedNotes (gridIntervalBeats(), 0);
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::upKey)
    {
        nudgeSelectedNotes (0.0, mods.isShiftDown() ? 12 : 1);
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::downKey)
    {
        nudgeSelectedNotes (0.0, mods.isShiftDown() ? -12 : -1);
        return true;
    }

    return false;
}

//==============================================================================
// Operations

void PianoRollEditor::quantiseNotes()
{
    te::QuantisationType quantiser;
    quantiser.setType (quantiseTypeNameForInterval (gridIntervalBeats()));

    for (auto* n : getTargetNotes())
    {
        const auto newStart = quantiser.roundBeatToNearest (n->getStartBeat());
        n->setStartAndLength (newStart, n->getLengthBeats(), getUndoManager());
    }

    repaint (gridBounds);
    repaint (velocityBounds);
}

void PianoRollEditor::humaniseNotes()
{
    auto& rng = juce::Random::getSystemRandom();
    const double maxTimeJitter = gridIntervalBeats() * 0.1;

    for (auto* n : getTargetNotes())
    {
        const double jitter = (rng.nextDouble() * 2.0 - 1.0) * maxTimeJitter;
        const double newStart = juce::jmax (0.0, n->getStartBeat().inBeats() + jitter);
        n->setStartAndLength (te::BeatPosition::fromBeats (newStart), n->getLengthBeats(), getUndoManager());

        const int newVelocity = juce::jlimit (1, 127, n->getVelocity() + rng.nextInt ({ -10, 11 }));
        n->setVelocity (newVelocity, getUndoManager());
    }

    repaint (gridBounds);
    repaint (velocityBounds);
}

void PianoRollEditor::deleteSelectedNotes()
{
    auto notes = getSelectedNotes();
    selection.clearQuick();

    for (auto* n : notes)
        clip->getSequence().removeNote (*n, getUndoManager());

    repaint (gridBounds);
    repaint (velocityBounds);
}

void PianoRollEditor::selectAllNotes()
{
    selection.clearQuick();
    for (auto* n : clip->getSequence().getNotes())
        selection.add (n->state);

    repaint (gridBounds);
    repaint (velocityBounds);
}

void PianoRollEditor::duplicateSelectedNotes()
{
    auto notes = getSelectedNotes();
    if (notes.isEmpty())
        return;

    double spanStart = std::numeric_limits<double>::max();
    double spanEnd = std::numeric_limits<double>::lowest();

    for (auto* n : notes)
    {
        spanStart = juce::jmin (spanStart, n->getStartBeat().inBeats());
        spanEnd = juce::jmax (spanEnd, n->getEndBeat().inBeats());
    }

    const double offset = juce::jmax (gridIntervalBeats(), spanEnd - spanStart);

    juce::Array<juce::ValueTree> newSelection;
    for (auto* n : notes)
    {
        if (auto* copy = clip->getSequence().addNote (n->getNoteNumber(),
                                                      te::BeatPosition::fromBeats (n->getStartBeat().inBeats() + offset),
                                                      n->getLengthBeats(),
                                                      n->getVelocity(), n->getColour(), getUndoManager()))
            newSelection.add (copy->state);
    }

    selection = newSelection;
    repaint (gridBounds);
    repaint (velocityBounds);
}

void PianoRollEditor::nudgeSelectedNotes (double beatDelta, int pitchDelta)
{
    for (auto* n : getSelectedNotes())
    {
        if (beatDelta != 0.0)
        {
            const double newStart = juce::jmax (0.0, n->getStartBeat().inBeats() + beatDelta);
            n->setStartAndLength (te::BeatPosition::fromBeats (newStart), n->getLengthBeats(), getUndoManager());
        }
        if (pitchDelta != 0)
            n->setNoteNumber (juce::jlimit (0, 127, n->getNoteNumber() + pitchDelta), getUndoManager());
    }

    repaint (gridBounds);
    repaint (velocityBounds);
}

//==============================================================================
// Audition

void PianoRollEditor::auditionPitch (int pitch, int velocity)
{
    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (clip->getTrack()))
    {
        audioTrack->playGuideNote (pitch, clip->getMidiChannel(), velocity, true);
        lastAuditionedPitch = pitch;
    }
}

void PianoRollEditor::stopAudition()
{
    if (lastAuditionedPitch < 0)
        return;

    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (clip->getTrack()))
        audioTrack->turnOffGuideNotes();

    lastAuditionedPitch = -1;
}

} // namespace arrange
