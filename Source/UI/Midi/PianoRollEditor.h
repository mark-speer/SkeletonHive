#pragma once

#include "Engine/GroovePoolManager.h"
#include "UI/Arrangement/EditViewState.h"
#include "UI/Midi/MidiLaneEditor.h"
#include "UI/Midi/MidiLaneViewport.h"

namespace skeletonhive
{

/** Ableton-style MIDI editor: multi-select, marquee, velocity lane, ghost notes,
    scale highlighting, fold, quantize/humanize, keyboard shortcuts and audition.

    All note mutations go through the Edit's UndoManager so they are undoable
    alongside the rest of the project.
*/
class PianoRollEditor : public juce::Component,
                        private te::ValueTreeAllEventListener,
                        private te::MidiInputDevice::MidiKeyChangeDispatcher::Listener,
                        private juce::ScrollBar::Listener,
                        private juce::ChangeListener
{
public:
    PianoRollEditor (te::MidiClip& clip, te::Edit& edit, EditViewState& viewState,
                     GroovePoolManager& groovePool);
    ~PianoRollEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool performCommand (int commandID, juce::ModifierKeys mods = {});
    bool keyPressed (const juce::KeyPress& key) override;

    // Editing operations (also reachable via keyboard shortcuts)
    void quantiseNotes();
    void humaniseNotes();
    void deleteSelectedNotes();
    void selectAllNotes();
    void duplicateSelectedNotes();
    void nudgeSelectedNotes (double beatDelta, int pitchDelta);

    void refreshGrooveBox();

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    enum class DragMode { none, marquee, move, resizeStart, resizeEnd, scrollKeyboard };

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

    // Zoom / scroll (juce::ScrollBar::Listener)
    void scrollBarMoved (juce::ScrollBar* bar, double newRangeStart) override;
    void zoomAt (int mouseX, double factor);
    void zoomVerticalAt (int mouseY, double factor);
    void clampScroll();
    void clampVerticalScroll();
    void updateHorizontalScrollBar();
    float minPixelsPerRowForView() const;
    float contentHeightPx() const;
    void fitRowsToView();

    // Step input: advances stepCursorBeat by one note-length per key/click, committing
    // any chord pitches staged (via Shift-click on the keyboard) at the same start beat.
    // allowStaging is true only for keyboard clicks, so a Shift-click on the grid still commits.
    void commitStepNote (int pitch, int velocity = defaultVelocity, bool allowStaging = false);

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
    bool isPitchInScale (int pitch) const;
    void repaintLaneEditor();
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
    GroovePoolManager& groovePool;

    // Toolbar
    juce::TextButton quantiseButton { "Quantize" }, humaniseButton { "Humanize" };
    juce::ToggleButton foldButton { "Fold" }, scaleSnapButton { "Snap Scale" };
    juce::ToggleButton drawButton { "Draw" }, stepButton { "Step" };
    juce::ComboBox rootBox, scaleBox, grooveBox;
    juce::ScrollBar hScrollBar { false };

    // Selection is stored as note state trees so it survives MidiNote reallocation
    juce::Array<juce::ValueTree> selection;
    juce::Array<juce::ValueTree> preMarqueeSelection;
    juce::Array<NoteOrigin> dragOrigins;
    juce::Array<PendingLiveNote> pendingLiveNotes;
    std::unique_ptr<MidiLaneEditor> laneEditor;
    MidiLaneViewport laneViewport;
    juce::SharedResourcePointer<te::MidiInputDevice::MidiKeyChangeDispatcher> midiKeyDispatcher;

    DragMode dragMode = DragMode::none;
    juce::Point<int> dragStartPos;
    juce::Rectangle<int> marqueeRect;
    double dragAnchorBeat = 0.0;
    int dragAnchorPitch = 60;
    int lastAuditionedPitch = -1;
    bool keyboardPendingClick = false;
    int keyboardPendingPitch = 60;
    double dragStartScrollRowOffset = 0.0;

    juce::Array<int> foldedPitches;   // descending, only used when folded

    // Zoom / scroll viewport state
    double pixelsPerBeat = 40.0;
    double scrollBeat = 0.0;
    float pixelsPerRow = 12.0f;
    double scrollRowOffset = 0.0;
    bool zoomInitialised = false;
    bool verticalZoomInitialised = false;

    // Note-length draw tool: remembers the last drawn/resized note's length so the
    // next drawn or step-input note reuses it.
    double currentNoteLengthBeats = 0.25;

    // Step input
    double stepCursorBeat = 0.0;
    juce::Array<int> chordStagingPitches;   // pitches queued via Shift-click, not yet committed

    // Layout regions
    juce::Rectangle<int> toolbarBounds, keyboardBounds, gridBounds, velocityBounds;

    static constexpr int lowestNote = 36;    // C2
    static constexpr int highestNote = 96;   // C7
    static constexpr int keyboardWidth = 52;
    static constexpr int toolbarHeight = 30;
    static constexpr int velocityLaneHeight = 94;
    static constexpr int scrollBarHeight = 14;
    static constexpr int resizeHandlePx = 5;
    static constexpr int defaultVelocity = 96;
    static constexpr double minNoteLengthBeats = 1.0 / 32.0;
    static constexpr double minPixelsPerBeat = 8.0;
    static constexpr double maxPixelsPerBeat = 400.0;
    static constexpr float maxPixelsPerRow = 64.0f;
    static constexpr int keyboardDragScrollThresholdPx = 4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRollEditor)
};

} // namespace skeletonhive
