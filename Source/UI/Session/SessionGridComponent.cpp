#include "SessionGridComponent.h"

namespace skeletonhive
{

SessionGridComponent::SessionGridComponent (SessionManager& session, SessionMidiMapper& midiMapper,
                                            EditViewState& viewState, ClipLibraryManager* clipLibrary)
    : sessionManager (session),
      sessionMidiMapper (midiMapper),
      editViewState (viewState),
      clipLibraryManager (clipLibrary)
{
    sceneLaunchColumn = std::make_unique<SceneLaunchColumn> (sessionManager);
    addAndMakeVisible (*sceneLaunchColumn);

    sessionManager.addChangeListener (this);
    editViewState.edit.state.addListener (this);
    rebuild();
}

void SessionGridComponent::scheduleLayoutRebuild()
{
    layoutDirty = true;
    triggerAsyncUpdate();
}

void SessionGridComponent::handleAsyncUpdate()
{
    if (layoutDirty)
        rebuild();
}

void SessionGridComponent::rebuild()
{
    layoutDirty = false;
    destroyVisibleUI();
    sceneLabels.clear();

    const int sceneCount = sessionManager.getSceneCount();

    for (int s = 0; s < sceneCount; ++s)
    {
        auto* label = sceneLabels.add (new juce::Label ({}, juce::String (s + 1)));
        label->setJustificationType (juce::Justification::centred);
        label->setFont (juce::FontOptions (11.0f));
        addAndMakeVisible (*label);
    }

    buildSlotLayout();
    sceneLaunchColumn->setSceneCount (sceneCount);
    refreshVisibleSlots();
}

void SessionGridComponent::buildSlotLayout()
{
    trackRows.clear();

    int y = 0;

    for (auto track : te::getAllTracks (editViewState.edit))
    {
        if (dynamic_cast<te::FolderTrack*> (track) != nullptr)
            continue;

        if (dynamic_cast<te::ClipTrack*> (track) == nullptr)
            continue;

        SessionSlotRowInfo row;
        row.track = track;
        row.trackId = track->itemID;
        row.y = y;
        row.height = juce::jmax (slotSize, TrackHeaderComponent::getPreferredHeight (*track));
        trackRows.add (row);
        y += slotSize;
    }

    const int sceneCount = sessionManager.getSceneCount();
    const int launchColumnWidth = 56;
    const int contentHeight = juce::jmax (slotSize, trackRows.size() * slotSize);
    setSize (getTrackHeaderWidth() + sceneCount * slotSize + launchColumnWidth, 22 + contentHeight);
}

void SessionGridComponent::destroyVisibleUI()
{
    visibleHeaders.clear();
    visibleSlots.clear();
}

void SessionGridComponent::setViewportRange (int viewY, int viewHeight)
{
    viewportY = viewY;
    viewportHeight = juce::jmax (1, viewHeight);
    refreshVisibleSlots();
}

void SessionGridComponent::refreshVisibleSlots()
{
    if (trackRows.isEmpty())
    {
        destroyVisibleUI();
        resized();
        return;
    }

    const int margin = juce::jmax (verticalVirtualizationMargin, viewportHeight / 2);
    const int visibleStartY = viewportY - margin;
    const int visibleEndY = viewportY + viewportHeight + margin;
    const int sceneCount = sessionManager.getSceneCount();

    juce::Array<te::EditItemID> desiredTrackIds;

    for (const auto& row : trackRows)
    {
        const int rowTop = 22 + row.y;
        const int rowBottom = rowTop + row.height;

        if (rowBottom >= visibleStartY && rowTop <= visibleEndY)
            desiredTrackIds.addIfNotAlreadyThere (row.trackId);
    }

    for (int i = visibleHeaders.size(); --i >= 0;)
    {
        if (! desiredTrackIds.contains (visibleHeaders[i]->getTrackId()))
            visibleHeaders.remove (i);
    }

    for (int i = visibleSlots.size(); --i >= 0;)
    {
        if (! desiredTrackIds.contains (visibleSlots[i]->getTrackId()))
            visibleSlots.remove (i);
    }

    for (const auto& row : trackRows)
    {
        if (! desiredTrackIds.contains (row.trackId))
            continue;

        bool hasHeader = false;
        for (auto* header : visibleHeaders)
        {
            if (header->getTrackId() == row.trackId)
            {
                hasHeader = true;
                break;
            }
        }

        if (! hasHeader)
            createVisibleHeader (row);

        for (int scene = 0; scene < sceneCount; ++scene)
        {
            if (findVisibleSlot (row.trackId, scene) != nullptr)
                continue;

            createVisibleSlot (row, scene);
        }
    }

    layoutVisibleUI();
}

void SessionGridComponent::createVisibleHeader (const SessionSlotRowInfo& row)
{
    auto header = std::make_unique<TrackHeaderComponent> (editViewState, row.track.get());
    header->onTrackSelected = [this] (te::Track& t)
    {
        selectedTrackId = t.itemID;
        if (onTrackSelected)
            onTrackSelected (selectedTrackId);

        for (auto* h : visibleHeaders)
            h->repaint();

        for (auto* slot : visibleSlots)
            slot->refresh();
    };
    addAndMakeVisible (*header);
    visibleHeaders.add (header.release());
}

void SessionGridComponent::createVisibleSlot (const SessionSlotRowInfo& row, int sceneIndex)
{
    auto* slot = visibleSlots.add (new ClipSlotComponent (sessionManager, sessionMidiMapper, editViewState,
                                                          clipLibraryManager, row.trackId, sceneIndex));
    slot->onTrackFocus = [this] (te::EditItemID trackId, int sceneIndex)
    {
        selectedTrackId = trackId;
        if (onTrackSelected)
            onTrackSelected (trackId);

        if (onSlotFocused)
            onSlotFocused (trackId, sceneIndex);

        for (auto* s : visibleSlots)
            s->setSelected (s->getTrackId() == trackId && s->getSceneIndex() == sceneIndex);
    };
    slot->onCommitLoopToArrangement = [this] (te::EditItemID trackId, int sceneIndex)
    {
        if (onCommitLoopToArrangement)
            onCommitLoopToArrangement (trackId, sceneIndex);
    };
    addAndMakeVisible (*slot);
}

ClipSlotComponent* SessionGridComponent::findVisibleSlot (te::EditItemID trackId, int sceneIndex) const
{
    for (auto* slot : visibleSlots)
    {
        if (slot->getTrackId() == trackId && slot->getSceneIndex() == sceneIndex)
            return slot;
    }

    return nullptr;
}

void SessionGridComponent::layoutVisibleUI()
{
    auto bounds = getLocalBounds();
    const int sceneCount = sessionManager.getSceneCount();
    const int launchColumnWidth = 56;
    auto launchArea = bounds.removeFromRight (launchColumnWidth);
    auto headerRow = bounds.removeFromTop (22);

    for (int s = 0; s < sceneLabels.size() && s < sceneCount; ++s)
    {
        if (auto* label = sceneLabels[s])
            label->setBounds (headerRow.removeFromLeft (slotSize));
    }

    headerRow.removeFromLeft (getTrackHeaderWidth());

    for (const auto& row : trackRows)
    {
        auto rowBounds = juce::Rectangle<int> (bounds.getX(), row.y + 22, bounds.getWidth(), row.height);
        auto headerArea = rowBounds.removeFromLeft (getTrackHeaderWidth());

        for (auto* header : visibleHeaders)
        {
            if (header->getTrackId() == row.trackId)
            {
                header->setBounds (headerArea.withHeight (TrackHeaderComponent::getPreferredHeight (*row.track)));
                break;
            }
        }

        for (int s = 0; s < sceneCount; ++s)
        {
            auto cellBounds = rowBounds.removeFromLeft (slotSize).reduced (1);
            if (auto* slot = findVisibleSlot (row.trackId, s))
                slot->setBounds (cellBounds);
        }
    }

    sceneLaunchColumn->setBounds (launchArea);
    sceneLaunchColumn->layoutButtons (slotSize);
}

void SessionGridComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    for (auto* slot : visibleSlots)
        slot->refresh();
}

void SessionGridComponent::paintEmptyCells (juce::Graphics& g, juce::Rectangle<int> area) const
{
    const int sceneCount = sessionManager.getSceneCount();

    for (const auto& row : trackRows)
    {
        auto rowArea = juce::Rectangle<int> (area.getX() + getTrackHeaderWidth(), row.y + 22,
                                             sceneCount * slotSize, row.height);

        if (! rowArea.intersects (area))
            continue;

        for (int s = 0; s < sceneCount; ++s)
        {
            auto cell = rowArea.removeFromLeft (slotSize).reduced (1);

            if (findVisibleSlot (row.trackId, s) != nullptr)
                continue;

            g.setColour (juce::Colour (0xff2a3344));
            g.fillRect (cell);
            g.setColour (juce::Colour (0xff415a77));
            g.drawRect (cell, 1);
        }
    }
}

void SessionGridComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b263b));

    const int sceneCount = sessionManager.getSceneCount();
    auto headerRow = getLocalBounds().removeFromTop (22);
    headerRow.removeFromLeft (getTrackHeaderWidth());
    const int sceneWidth = slotSize;

    for (int s = 0; s < sceneCount; ++s)
    {
        auto cell = headerRow.removeFromLeft (sceneWidth);
        g.setColour (juce::Colour (0xff415a77));
        g.drawRect (cell, 1);
    }

    paintEmptyCells (g, getLocalBounds());
}

void SessionGridComponent::resized()
{
    layoutVisibleUI();
}

} // namespace skeletonhive
