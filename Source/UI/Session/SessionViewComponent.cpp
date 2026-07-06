#include "SessionViewComponent.h"

namespace skeletonhive
{

SessionViewComponent::SessionViewComponent (SessionManager& session, SessionMidiMapper& midiMapper,
                                            EditViewState& viewState, ClipLibraryManager* clipLibrary)
{
    grid = std::make_unique<SessionGridComponent> (session, midiMapper, viewState, clipLibrary);
    grid->onTrackSelected = [this] (te::EditItemID trackId)
    {
        if (onTrackSelected)
            onTrackSelected (trackId);
    };
    grid->onSlotFocused = [this] (te::EditItemID trackId, int sceneIndex)
    {
        if (onSlotFocused)
            onSlotFocused (trackId, sceneIndex);
    };
    grid->onCommitLoopToArrangement = [this] (te::EditItemID trackId, int sceneIndex)
    {
        if (onCommitLoopToArrangement)
            onCommitLoopToArrangement (trackId, sceneIndex);
    };

    viewport.setViewedComponent (grid.get(), false);
    viewport.setScrollBarsShown (true, true);
    viewport.getVerticalScrollBar().addListener (this);
    viewport.getHorizontalScrollBar().addListener (this);
    addAndMakeVisible (viewport);
}

void SessionViewComponent::resized()
{
    viewport.setBounds (getLocalBounds());
    syncViewportToGrid();
}

void SessionViewComponent::scrollBarMoved (juce::ScrollBar*, double)
{
    syncViewportToGrid();
}

void SessionViewComponent::syncViewportToGrid()
{
    if (grid != nullptr)
        grid->setViewportRange (viewport.getViewPositionY(), viewport.getViewHeight());
}

} // namespace skeletonhive
