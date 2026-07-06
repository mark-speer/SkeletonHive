#include "SessionViewComponent.h"

namespace skeletonhive
{

SessionViewComponent::SessionViewComponent (SessionManager& session, EditViewState& viewState,
                                            ClipLibraryManager* clipLibrary)
{
    grid = std::make_unique<SessionGridComponent> (session, viewState, clipLibrary);
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
    addAndMakeVisible (viewport);
}

void SessionViewComponent::resized()
{
    viewport.setBounds (getLocalBounds());
}

} // namespace skeletonhive
