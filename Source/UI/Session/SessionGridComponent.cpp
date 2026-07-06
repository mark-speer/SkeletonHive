#include "SessionGridComponent.h"

namespace skeletonhive
{

SessionGridComponent::SessionGridComponent (SessionManager& session, EditViewState& viewState,
                                            ClipLibraryManager* clipLibrary)
    : sessionManager (session),
      editViewState (viewState),
      clipLibraryManager (clipLibrary)
{
    sceneLaunchColumn = std::make_unique<SceneLaunchColumn> (sessionManager);
    addAndMakeVisible (*sceneLaunchColumn);

    sessionManager.addChangeListener (this);
    editViewState.edit.state.addListener (this);
    rebuild();
}

void SessionGridComponent::rebuild()
{
    trackHeaders.clear();
    slots.clear();
    sceneLabels.clear();

    const int sceneCount = sessionManager.getSceneCount();

    for (int s = 0; s < sceneCount; ++s)
    {
        auto* label = sceneLabels.add (new juce::Label ({}, juce::String (s + 1)));
        label->setJustificationType (juce::Justification::centred);
        label->setFont (juce::FontOptions (11.0f));
        addAndMakeVisible (*label);
    }

    for (auto track : te::getAllTracks (editViewState.edit))
    {
        if (dynamic_cast<te::FolderTrack*> (track) != nullptr)
            continue;

        if (dynamic_cast<te::ClipTrack*> (track) == nullptr)
            continue;

        auto header = std::make_unique<TrackHeaderComponent> (editViewState, track);
        header->onTrackSelected = [this] (te::Track& t)
        {
            selectedTrackId = t.itemID;
            if (onTrackSelected)
                onTrackSelected (selectedTrackId);

            for (auto* h : trackHeaders)
                h->repaint();

            for (auto* slot : slots)
                slot->setSelected (slot->isSelected()); // refresh via track focus below
        };
        addAndMakeVisible (*header);
        trackHeaders.add (header.release());

        for (int scene = 0; scene < sceneCount; ++scene)
        {
            auto* slot = slots.add (new ClipSlotComponent (sessionManager, editViewState, clipLibraryManager,
                                                          track->itemID, scene));
            slot->onTrackFocus = [this] (te::EditItemID trackId, int sceneIndex)
            {
                selectedTrackId = trackId;
                if (onTrackSelected)
                    onTrackSelected (trackId);

                if (onSlotFocused)
                    onSlotFocused (trackId, sceneIndex);

                for (auto* s : slots)
                    s->setSelected (false);
            };
            slot->onCommitLoopToArrangement = [this] (te::EditItemID trackId, int sceneIndex)
            {
                if (onCommitLoopToArrangement)
                    onCommitLoopToArrangement (trackId, sceneIndex);
            };
            addAndMakeVisible (*slot);
        }
    }

    sceneLaunchColumn->setSceneCount (sceneCount);
    resized();
}

void SessionGridComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b263b));

    const int sceneCount = sessionManager.getSceneCount();
    auto headerRow = getLocalBounds().removeFromTop (22);
    headerRow.removeFromLeft (trackHeaderWidth);
    const int sceneWidth = slotSize;

    for (int s = 0; s < sceneCount; ++s)
    {
        auto cell = headerRow.removeFromLeft (sceneWidth);
        g.setColour (juce::Colour (0xff415a77));
        g.drawRect (cell, 1);
    }
}

void SessionGridComponent::resized()
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

    headerRow.removeFromLeft (trackHeaderWidth);

    int y = bounds.getY();
    const int trackCount = trackHeaders.size();

    for (int t = 0; t < trackCount; ++t)
    {
        auto row = bounds.removeFromTop (slotSize);
        auto headerArea = row.removeFromLeft (trackHeaderWidth);

        if (auto* header = trackHeaders[t])
            header->setBounds (headerArea);

        for (int s = 0; s < sceneCount; ++s)
        {
            const int slotIndex = t * sceneCount + s;
            if (auto* slot = slots[slotIndex])
                slot->setBounds (row.removeFromLeft (slotSize).reduced (1));
        }

        y += slotSize;
    }

    sceneLaunchColumn->setBounds (launchArea);
    sceneLaunchColumn->layoutButtons (slotSize);

    const int contentHeight = juce::jmax (slotSize, trackCount * slotSize);
    setSize (trackHeaderWidth + sceneCount * slotSize + launchColumnWidth, 22 + contentHeight);
}

} // namespace skeletonhive
