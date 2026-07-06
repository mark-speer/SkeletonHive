#include "SidechainMatrixPanel.h"

namespace skeletonhive
{

namespace
{
juce::String abbreviateTrackName (const juce::String& name, int maxLen = 10)
{
    if (name.length() <= maxLen)
        return name;

    return name.substring (0, maxLen - 1) + "...";
}
} // namespace

SidechainMatrixPanel::SidechainMatrixPanel (te::Edit& e)
    : edit (e)
{
    titleLabel.setText ("Sidechain Routing", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);

    helpLabel.setText ("Select one source track per plugin. Sidechain sources remain audible when muted.",
                       juce::dontSendNotification);
    helpLabel.setFont (juce::FontOptions (11.0f));
    helpLabel.setColour (juce::Label::textColourId, juce::Colours::grey);

    applyButton.onClick = [this] { applyChanges(); };
    cancelButton.onClick = [this] { cancelChanges(); };

    gridViewport.setViewedComponent (&gridContent, false);
    gridViewport.setScrollBarsShown (true, true);

    addAndMakeVisible (titleLabel);
    addAndMakeVisible (helpLabel);
    addAndMakeVisible (applyButton);
    addAndMakeVisible (cancelButton);
    addAndMakeVisible (gridViewport);

    edit.state.addListener (this);
    rebuildFromEdit();
}

SidechainMatrixPanel::~SidechainMatrixPanel()
{
    edit.state.removeListener (this);
}

void SidechainMatrixPanel::focusPlugin (te::EditItemID pluginId)
{
    focusPluginId = pluginId;
    rebuildGrid();

    for (int i = 0; i < draftRows.size(); ++i)
    {
        if (draftRows.getReference (i).pluginId == pluginId)
        {
            const int y = headerHeight + i * rowHeight;
            gridViewport.setViewPosition (gridViewport.getViewPositionX(),
                                          juce::jmax (0, y - rowHeight));
            break;
        }
    }
}

void SidechainMatrixPanel::resized()
{
    auto area = getLocalBounds().reduced (8);
    titleLabel.setBounds (area.removeFromTop (22));
    helpLabel.setBounds (area.removeFromTop (18));
    area.removeFromTop (6);

    auto footer = area.removeFromBottom (28);
    cancelButton.setBounds (footer.removeFromRight (72));
    footer.removeFromRight (6);
    applyButton.setBounds (footer.removeFromRight (72));

    gridViewport.setBounds (area);
}

void SidechainMatrixPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2e));
}

void SidechainMatrixPanel::handleAsyncUpdate()
{
    if (dirty)
    {
        dirty = false;
        rebuildFromEdit();
    }
}

void SidechainMatrixPanel::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&)
{
    markDirty();
}

void SidechainMatrixPanel::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int)
{
    markDirty();
}

void SidechainMatrixPanel::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    markDirty();
}

void SidechainMatrixPanel::markDirty()
{
    dirty = true;
    triggerAsyncUpdate();
}

void SidechainMatrixPanel::rebuildFromEdit()
{
    draftRows = SidechainRouting::buildMatrix (edit);
    sourceTracks = SidechainRouting::getAllSourceTracks (edit);
    rebuildGrid();
}

void SidechainMatrixPanel::rebuildGrid()
{
    rowControls.clear();
    headerLabels.clear();
    gridContent.removeAllChildren();

    const int numCols = 1 + sourceTracks.size();
    const int contentWidth = labelColumnWidth + numCols * columnWidth;
    const int contentHeight = juce::jmax (headerHeight + rowHeight,
                                          headerHeight + draftRows.size() * rowHeight);

    gridContent.setSize (contentWidth, contentHeight);

    auto* noneHeader = new juce::Label ({}, "None");
    noneHeader->setJustificationType (juce::Justification::centred);
    noneHeader->setFont (juce::FontOptions (10.0f, juce::Font::bold));
    noneHeader->setBounds (labelColumnWidth, 0, columnWidth, headerHeight);
    gridContent.addAndMakeVisible (noneHeader);
    headerLabels.add (noneHeader);

    for (int col = 0; col < sourceTracks.size(); ++col)
    {
        auto* track = sourceTracks[col];
        const bool isActiveSource = SidechainRouting::isTrackUsedAsSidechainSource (edit, *track);
        auto* header = new juce::Label ({}, abbreviateTrackName (track->getName()));
        header->setJustificationType (juce::Justification::centred);
        header->setFont (juce::FontOptions (10.0f, juce::Font::bold));
        header->setTooltip (track->getName());

        if (isActiveSource)
            header->setColour (juce::Label::textColourId, juce::Colours::cyan);

        header->setBounds (labelColumnWidth + (col + 1) * columnWidth, 0, columnWidth, headerHeight);
        gridContent.addAndMakeVisible (header);
        headerLabels.add (header);
    }

    if (draftRows.isEmpty())
    {
        auto* empty = new juce::Label ({}, "No sidechain-capable plugins in this project.");
        empty->setJustificationType (juce::Justification::centred);
        empty->setBounds (0, headerHeight, contentWidth, rowHeight * 2);
        gridContent.addAndMakeVisible (empty);
        applyButton.setEnabled (false);
        return;
    }

    applyButton.setEnabled (true);

    for (int row = 0; row < draftRows.size(); ++row)
    {
        const auto& data = draftRows.getReference (row);
        auto* rowLabel = new juce::Label ({}, data.hostTrackName + " \u203a " + data.pluginName);
        rowLabel->setJustificationType (juce::Justification::centredLeft);
        rowLabel->setFont (juce::FontOptions (11.0f));
        rowLabel->setBounds (4, headerHeight + row * rowHeight, labelColumnWidth - 8, rowHeight);

        if (data.pluginId == focusPluginId)
            rowLabel->setColour (juce::Label::backgroundColourId, juce::Colours::white.withAlpha (0.08f));

        gridContent.addAndMakeVisible (rowLabel);

        auto* controls = new RowControls();
        controls->pluginId = data.pluginId;
        const int radioGroupId = 1000 + row;

        auto* noneRadio = new juce::ToggleButton ("");
        noneRadio->setRadioGroupId (radioGroupId);
        noneRadio->setToggleState (! data.currentSourceTrackId.isValid(), juce::dontSendNotification);
        noneRadio->setBounds (labelColumnWidth + (columnWidth - 18) / 2,
                              headerHeight + row * rowHeight + 5,
                              18, 18);
        gridContent.addAndMakeVisible (noneRadio);
        controls->radios.add (noneRadio);

        for (int col = 0; col < sourceTracks.size(); ++col)
        {
            auto* track = sourceTracks[col];
            const bool isHost = track->itemID == data.hostTrackId;
            auto* radio = new juce::ToggleButton ("");
            radio->setRadioGroupId (radioGroupId);
            radio->setEnabled (! isHost);
            radio->setToggleState (! isHost && data.currentSourceTrackId == track->itemID,
                                   juce::dontSendNotification);
            radio->setBounds (labelColumnWidth + (col + 1) * columnWidth + (columnWidth - 18) / 2,
                              headerHeight + row * rowHeight + 5,
                              18, 18);
            gridContent.addAndMakeVisible (radio);
            controls->radios.add (radio);
        }

        rowControls.add (controls);
    }
}

void SidechainMatrixPanel::syncDraftFromControls()
{
    for (int row = 0; row < rowControls.size() && row < draftRows.size(); ++row)
    {
        auto& data = draftRows.getReference (row);
        auto* controls = rowControls[row];
        data.currentSourceTrackId = {};

        for (int col = 0; col < controls->radios.size(); ++col)
        {
            if (! controls->radios[col]->getToggleState())
                continue;

            if (col == 0)
                break;

            if (juce::isPositiveAndBelow (col - 1, sourceTracks.size()))
                data.currentSourceTrackId = sourceTracks[col - 1]->itemID;

            break;
        }
    }
}

void SidechainMatrixPanel::applyChanges()
{
    syncDraftFromControls();
    SidechainRouting::applyMatrix (edit, draftRows);
    focusPluginId = {};
    rebuildFromEdit();
}

void SidechainMatrixPanel::cancelChanges()
{
    focusPluginId = {};
    rebuildFromEdit();
}

} // namespace skeletonhive
