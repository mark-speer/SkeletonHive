#pragma once

#include "ClipSlotComponent.h"
#include "SceneLaunchColumn.h"
#include "UI/Arrangement/TrackComponents.h"

namespace skeletonhive
{

class SessionGridComponent : public juce::Component,
                             private juce::ChangeListener,
                             private te::ValueTreeAllEventListener
{
public:
    SessionGridComponent (SessionManager& session, EditViewState& viewState,
                          ClipLibraryManager* clipLibrary);

    static constexpr int slotSize = 80;
    static constexpr int trackHeaderWidth = 190;

    std::function<void (te::EditItemID trackId)> onTrackSelected;
    std::function<void (te::EditItemID trackId, int sceneIndex)> onSlotFocused;
    std::function<void (te::EditItemID trackId, int sceneIndex)> onCommitLoopToArrangement;

    void rebuild();

private:
    void valueTreeChanged() override {}
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { rebuild(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { rebuild(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override { rebuild(); }
    void changeListenerCallback (juce::ChangeBroadcaster*) override { rebuild(); }

    void paint (juce::Graphics& g) override;
    void resized() override;

    SessionManager& sessionManager;
    EditViewState& editViewState;
    ClipLibraryManager* clipLibraryManager = nullptr;

    juce::OwnedArray<TrackHeaderComponent> trackHeaders;
    juce::OwnedArray<ClipSlotComponent> slots;
    juce::OwnedArray<juce::Label> sceneLabels;
    std::unique_ptr<SceneLaunchColumn> sceneLaunchColumn;
    te::EditItemID selectedTrackId;
};

} // namespace skeletonhive
