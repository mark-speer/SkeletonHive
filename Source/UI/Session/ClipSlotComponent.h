#pragma once

#include "Engine/SessionManager.h"
#include "Engine/SessionMidiMapper.h"
#include "Engine/ClipLibraryManager.h"
#include "UI/Arrangement/EditViewState.h"

namespace skeletonhive
{

enum class ClipSlotState
{
    empty,
    loaded,
    playing,
    recording
};

class ClipSlotComponent : public juce::Component,
                          public juce::DragAndDropTarget
{
public:
    ClipSlotComponent (SessionManager& session, SessionMidiMapper& midiMapper, EditViewState& viewState,
                       ClipLibraryManager* clipLibrary, te::EditItemID trackId, int sceneIndex);

    void refresh();
    void setSelected (bool shouldBeSelected);
    bool isSelected() const { return selected; }
    te::EditItemID getTrackId() const { return trackId; }
    int getSceneIndex() const { return sceneIndex; }

    std::function<void (te::EditItemID trackId, int sceneIndex)> onTrackFocus;
    std::function<void (te::EditItemID trackId, int sceneIndex)> onCommitLoopToArrangement;

    bool isInterestedInDragSource (const SourceDetails& dragSourceDetails) override;
    void itemDropped (const SourceDetails& dragSourceDetails) override;

private:
    ClipSlotState getState() const;
    te::Clip* getClip() const;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    void itemDragEnter (const SourceDetails&) override {}
    void itemDragMove (const SourceDetails&) override {}
    void itemDragExit (const SourceDetails&) override {}

    void showContextMenu (juce::Point<int> screenPos);
    void promptDuplicateToScene();

    te::ClipTrack* getTrack() const;

    SessionManager& sessionManager;
    SessionMidiMapper& sessionMidiMapper;
    EditViewState& editViewState;
    ClipLibraryManager* clipLibraryManager = nullptr;
    te::EditItemID trackId;
    int sceneIndex = 0;
    bool selected = false;
};

} // namespace skeletonhive
