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

    viewport.setViewedComponent (grid.get(), false);
    viewport.setScrollBarsShown (true, true);
    addAndMakeVisible (viewport);
}

void SessionViewComponent::resized()
{
    viewport.setBounds (getLocalBounds());
}

} // namespace skeletonhive
