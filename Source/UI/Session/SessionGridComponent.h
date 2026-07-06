#pragma once

#include "ClipSlotComponent.h"
#include "SceneLaunchColumn.h"
#include "SessionSlotTypes.h"
#include "UI/Arrangement/TrackComponents.h"

namespace skeletonhive
{

class SessionGridComponent : public juce::Component,
                             private juce::ChangeListener,
                             private te::ValueTreeAllEventListener,
                             private FlaggedAsyncUpdater
{
public:
    SessionGridComponent (SessionManager& session, SessionMidiMapper& midiMapper, EditViewState& viewState,
                          ClipLibraryManager* clipLibrary);

    static constexpr int slotSize = 80;
    static constexpr int trackHeaderWidth = 190;
    static constexpr int verticalVirtualizationMargin = 200;

    std::function<void (te::EditItemID trackId)> onTrackSelected;
    std::function<void (te::EditItemID trackId, int sceneIndex)> onSlotFocused;
    std::function<void (te::EditItemID trackId, int sceneIndex)> onCommitLoopToArrangement;

    void rebuild();
    void setViewportRange (int viewY, int viewHeight);
    void refreshVisibleSlots();
    int getLiveSlotComponentCount() const { return visibleSlots.size(); }

private:
    void valueTreeChanged() override {}
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { scheduleLayoutRebuild(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { scheduleLayoutRebuild(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override { scheduleLayoutRebuild(); }
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void handleAsyncUpdate() override;

    void scheduleLayoutRebuild();
    void buildSlotLayout();
    void destroyVisibleUI();
    void createVisibleSlot (const SessionSlotRowInfo& row, int sceneIndex);
    void createVisibleHeader (const SessionSlotRowInfo& row);
    void layoutVisibleUI();
    void paintEmptyCells (juce::Graphics& g, juce::Rectangle<int> area) const;
    ClipSlotComponent* findVisibleSlot (te::EditItemID trackId, int sceneIndex) const;

    void paint (juce::Graphics& g) override;
    void resized() override;

    SessionManager& sessionManager;
    SessionMidiMapper& sessionMidiMapper;
    EditViewState& editViewState;
    ClipLibraryManager* clipLibraryManager = nullptr;

    juce::Array<SessionSlotRowInfo> trackRows;
    juce::OwnedArray<TrackHeaderComponent> visibleHeaders;
    juce::OwnedArray<ClipSlotComponent> visibleSlots;
    juce::OwnedArray<juce::Label> sceneLabels;
    std::unique_ptr<SceneLaunchColumn> sceneLaunchColumn;
    te::EditItemID selectedTrackId;
    int viewportY = 0;
    int viewportHeight = 600;
    bool layoutDirty = true;
};

} // namespace skeletonhive
