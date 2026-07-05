#pragma once

#include "UI/Arrangement/EditViewState.h"

namespace arrange
{

/** Ableton-style MIDI editor: multi-select, marquee, velocity lane, ghost notes,
    scale highlighting, fold, quantize/humanize, keyboard shortcuts and audition.

    All note mutations go through the Edit's UndoManager so they are undoable
    alongside the rest of the project.
*/
class PianoRollEditor : public juce::Component,
                        private te::ValueTreeAllEventListener,
                        private te::MidiInputDevice::MidiKeyChangeDispatcher::Listener
{
public:
    PianoRollEditor (te::MidiClip& clip, te::Edit& edit, EditViewState& viewState);
    ~PianoRollEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& key) override;

    // Editing operations (also reachable via keyboard shortcuts)
    void quantiseNotes();
    void humaniseNotes();
    void deleteSelectedNotes();
    void selectAllNotes();
    void duplicateSelectedNotes();
    void nudgeSelectedNotes (double beatDelta, int pitchDelta);

private:
    enum class DragMode { none, marquee, move, resizeStart, resizeEnd, velocity };

    struct NoteOrigin
    {
        juce::ValueTree state;
        double startBeat = 0.0;
        double lengthBeats = 0.0;
        int pitch = 60;
    };

    struct PendingLiveNote
    {
        int pitch = 0;
        int velocity = 0;
        double startBeat = 0.0;   // clip-visible-relative, same frame as xToBeat()
    };

    // ValueTree listener (external edits, undo/redo)
    void valueTreeChanged() override;

    // Live MIDI input (te::MidiInputDevice::MidiKeyChangeDispatcher::Listener)
    void midiKeyStateChanged (te::AudioTrack*, const juce::Array<int>& notesOn,
                              const juce::Array<int>& vels, const juce::Array<int>& notesOff) override;
    void focusGained (FocusChangeType) override;
    void focusLost (FocusChangeType) override;
    double liveInputBeatPosition() const;

    // Geometry / mapping
    void rebuildFoldedPitches();
    int numVisibleRows() const;
    int pitchForRow (int row) const;
    int rowForPitch (int pitch) const;
    float rowHeight() const;
    int pitchAtY (int y) const;
    int yForPitchTop (int pitch) const;

    double clipLengthBeats() const;
    double xToBeat (int x) const;
    float beatToX (double beat) const;
    double gridIntervalBeats() const;
    double snapBeat (double beat) const;
    double beatsPerBar() const;

    juce::Rectangle<float> rectForNote (const te::MidiNote& note) const;
    te::MidiNote* noteAtPosition (juce::Point<int> pos) const;
    juce::Array<te::MidiNote*> getTargetNotes() const;   // selection, or all notes when nothing selected
    juce::Array<te::MidiNote*> getSelectedNotes() const;
    bool isSelected (const te::MidiNote& note) const;
    void pruneSelection();
    void captureDragOrigins();

    // Painting helpers
    void paintKeyboard (juce::Graphics& g) const;
    void paintGrid (juce::Graphics& g) const;
    void paintGhostNotes (juce::Graphics& g) const;
    void paintNotes (juce::Graphics& g) const;
    void paintVelocityLane (juce::Graphics& g) const;
    bool isPitchInScale (int pitch) const;
    /** Nearest pitch that's in the current scale (returns pitch unchanged if no scale is selected). */
    int nearestInScalePitch (int pitch) const;

    // Audition
    void auditionPitch (int pitch, int velocity);
    void stopAudition();

    juce::UndoManager* getUndoManager() const;

    //==============================================================================
    te::MidiClip::Ptr clip;
    te::Edit& edit;
    EditViewState& editViewState;

    // Toolbar
    juce::TextButton quantiseButton { "Quantize" }, humaniseButton { "Humanize" };
    juce::ToggleButton foldButton { "Fold" }, scaleSnapButton { "Snap Scale" };
    juce::ComboBox rootBox, scaleBox, grooveBox;

    // Selection is stored as note state trees so it survives MidiNote reallocation
    juce::Array<juce::ValueTree> selection;
    juce::Array<juce::ValueTree> preMarqueeSelection;
    juce::Array<NoteOrigin> dragOrigins;
    juce::Array<juce::ValueTree> velocityTargets;
    juce::Array<PendingLiveNote> pendingLiveNotes;
    juce::SharedResourcePointer<te::MidiInputDevice::MidiKeyChangeDispatcher> midiKeyDispatcher;

    DragMode dragMode = DragMode::none;
    juce::Point<int> dragStartPos;
    juce::Rectangle<int> marqueeRect;
    double dragAnchorBeat = 0.0;
    int dragAnchorPitch = 60;
    int lastAuditionedPitch = -1;
    bool velocityPaintMode = false;

    juce::Array<int> foldedPitches;   // descending, only used when folded

    // Layout regions
    juce::Rectangle<int> toolbarBounds, keyboardBounds, gridBounds, velocityBounds;

    static constexpr int lowestNote = 36;    // C2
    static constexpr int highestNote = 96;   // C7
    static constexpr int keyboardWidth = 52;
    static constexpr int toolbarHeight = 30;
    static constexpr int velocityLaneHeight = 72;
    static constexpr int resizeHandlePx = 5;
    static constexpr int defaultVelocity = 96;
    static constexpr double minNoteLengthBeats = 1.0 / 32.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRollEditor)
};

} // namespace arrange
