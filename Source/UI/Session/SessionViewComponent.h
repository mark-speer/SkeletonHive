#pragma once

#include "SessionGridComponent.h"
#include "Engine/SessionManager.h"

namespace skeletonhive
{

class SessionViewComponent : public juce::Component
{
public:
    SessionViewComponent (SessionManager& session, EditViewState& viewState,
                          ClipLibraryManager* clipLibrary);

    std::function<void (te::EditItemID trackId)> onTrackSelected;

private:
    void resized() override;

    std::unique_ptr<SessionGridComponent> grid;
    juce::Viewport viewport;
};

} // namespace skeletonhive
