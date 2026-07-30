#include "SampleBrowserTab.h"
#include "Engine/ContentDragManager.h"

namespace skeletonhive
{

class SampleBrowserTab::SampleListModel : public juce::ListBoxModel
{
public:
    explicit SampleListModel (SampleBrowserTab& owner) : tab (owner) {}

    int getNumRows() override { return tab.displayedEntries.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, tab.displayedEntries.size()))
            return;

        const auto& entry = tab.displayedEntries.getReference (row);

        if (selected)
            g.fillAll (juce::Colours::white.withAlpha (0.18f));

        if (tab.previewPlayer.isPlaying() && tab.previewPlayer.getCurrentFile() == entry.file)
        {
            const auto progress = (float) tab.previewPlayer.getPlaybackProgress();
            g.setColour (juce::Colour (0xff5a189a).withAlpha (0.35f));
            g.fillRect (0, 0, (int) (progress * (float) width), height);
        }

        const int waveformWidth = juce::jmin (72, width / 3);
        auto textArea = juce::Rectangle<int> (4, 0, width - waveformWidth - 8, height);

        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.setFont (juce::FontOptions ((float) height * 0.42f));
        g.drawText (entry.displayName, textArea.removeFromTop (height / 2),
                    juce::Justification::centredLeft, true);

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (juce::FontOptions ((float) height * 0.34f));
        g.drawText (tab.formatDuration (entry.lengthSeconds), textArea,
                    juce::Justification::centredLeft, true);

        if (tab.contentLibrary.isFavorite (entry.file))
        {
            g.setColour (juce::Colours::gold.withAlpha (0.9f));
            g.fillEllipse ((float) width - 14.0f, 4.0f, 8.0f, 8.0f);
        }

        const auto waveformArea = juce::Rectangle<int> (width - waveformWidth, 4, waveformWidth - 4, height - 8);
        te::AudioFile audioFile (tab.engineRef, entry.file);

        if (audioFile.isValid())
        {
            if (auto thumb = tab.waveformCache.acquire (tab.engineRef, audioFile, tab, nullptr))
            {
                g.setColour (juce::Colours::white.withAlpha (0.35f));
                const te::TimeRange viewRange { 0s, te::TimeDuration::fromSeconds (audioFile.getLength()) };
                thumb->drawChannels (g, waveformArea, viewRange, 1.0f);
            }
        }
    }

    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        if (! juce::isPositiveAndBelow (row, tab.displayedEntries.size()))
            return;

        tab.sampleList.selectRow (row);
        tab.selectedEntry = tab.displayedEntries.getReference (row);
        tab.previewRow (row);

        if (e.mods.isPopupMenu())
            tab.showContextMenu (row, e.getScreenPosition());
    }

    SampleBrowserTab& tab;
};

SampleBrowserTab::SampleBrowserTab (ContentLibraryManager& library,
                                    PreviewPlayer& preview,
                                    WaveformCache& waveforms,
                                    te::Engine& engine)
    : contentLibrary (library),
      previewPlayer (preview),
      waveformCache (waveforms),
      engineRef (engine)
{
    searchBox.setTextToShowWhenEmpty ("Search samples...", juce::Colours::grey);
    searchBox.addListener (this);

    sortBox.addItem ("Name", 1);
    sortBox.addItem ("Date modified", 2);
    sortBox.addItem ("Duration", 3);
    sortBox.setSelectedId (1, juce::dontSendNotification);
    sortBox.addListener (this);

    filterBox.addItem ("All", 1);
    filterBox.addItem ("Favorites", 2);
    filterBox.addItem ("Recent", 3);
    filterBox.setSelectedId (1, juce::dontSendNotification);
    filterBox.addListener (this);

    listModel = std::make_unique<SampleListModel> (*this);
    sampleList.setModel (listModel.get());
    sampleList.setRowHeight (36);
    sampleList.onStartDrag = [this] (const juce::MouseEvent& e, int row)
    {
        startSampleDrag (e, row);
    };

    rescanButton.onClick = [this] { contentLibrary.rescanAll(); };

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setFont (juce::FontOptions (11.0f));

    contentLibrary.addChangeListener (this);

    addAndMakeVisible (searchBox);
    addAndMakeVisible (sortBox);
    addAndMakeVisible (filterBox);
    addAndMakeVisible (sampleList);
    addAndMakeVisible (rescanButton);
    addAndMakeVisible (statusLabel);

    rebuildList();
    startTimerHz (15);
}

SampleBrowserTab::~SampleBrowserTab() = default;

void SampleBrowserTab::setRootFilter (const juce::File& root)
{
    rootFilter = root;
    filterBox.setSelectedId (1, juce::dontSendNotification);
    rebuildList();
}

void SampleBrowserTab::showFavoritesFilter()
{
    rootFilter = juce::File();
    filterBox.setSelectedId (2, juce::dontSendNotification);
    rebuildList();
}

void SampleBrowserTab::refreshList()
{
    rebuildList();
}

void SampleBrowserTab::resized()
{
    auto r = getLocalBounds().reduced (6);
    searchBox.setBounds (r.removeFromTop (24));
    r.removeFromTop (4);

    auto filterRow = r.removeFromTop (24);
    filterBox.setBounds (filterRow.removeFromLeft (filterRow.getWidth() / 2).reduced (0, 0));
    sortBox.setBounds (filterRow);

    r.removeFromTop (4);
    auto bottomRow = r.removeFromBottom (24);
    rescanButton.setBounds (bottomRow.removeFromRight (72));
    bottomRow.removeFromRight (6);
    statusLabel.setBounds (bottomRow);
    r.removeFromBottom (4);
    sampleList.setBounds (r);
}

void SampleBrowserTab::startSampleDrag (const juce::MouseEvent& e, int row)
{
    if (! juce::isPositiveAndBelow (row, displayedEntries.size()))
        return;

    const auto& entry = displayedEntries.getReference (row);

    if (auto* container = findParentComponentOfClass<juce::DragAndDropContainer>())
    {
        ContentDragPayload payload;
        payload.file = entry.file;
        container->startDragging (payload.encode(), &sampleList, juce::ScaledImage(), true, nullptr, &e.source);
    }
}

void SampleBrowserTab::mouseExit (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    stopPreview();
}

void SampleBrowserTab::visibilityChanged()
{
    if (! isShowing())
        stopPreview();
}

void SampleBrowserTab::textEditorTextChanged (juce::TextEditor&)
{
    rebuildList();
}

void SampleBrowserTab::comboBoxChanged (juce::ComboBox*)
{
    rebuildList();
}

void SampleBrowserTab::changeListenerCallback (juce::ChangeBroadcaster*)
{
    rebuildList();
}

void SampleBrowserTab::timerCallback()
{
    if (contentLibrary.isScanning())
    {
        statusLabel.setText ("Scanning...", juce::dontSendNotification);
        return;
    }

    statusLabel.setText (juce::String (displayedEntries.size()) + " samples",
                         juce::dontSendNotification);

    if (! isShowing() || ! isVisible())
    {
        stopPreview();
        return;
    }

    if (auto* tabs = findParentComponentOfClass<juce::TabbedComponent>())
        if (tabs->getCurrentContentComponent() != this)
        {
            stopPreview();
            return;
        }

    if (previewPlayer.isPlaying())
        sampleList.repaint();
}

void SampleBrowserTab::stopPreview()
{
    if (! previewPlayer.isPlaying())
        return;

    previewPlayer.stop();
    sampleList.repaint();
}

void SampleBrowserTab::rebuildList()
{
    const auto sort = sortBox.getSelectedId() == 2 ? ContentSortMode::dateModified
                    : sortBox.getSelectedId() == 3 ? ContentSortMode::duration
                    : ContentSortMode::name;

    const auto filter = filterBox.getSelectedId() == 2 ? ContentFilterMode::favorites
                      : filterBox.getSelectedId() == 3 ? ContentFilterMode::recent
                      : ContentFilterMode::all;

    displayedEntries = contentLibrary.getEntries (filter, sort, searchBox.getText(), rootFilter);
    sampleList.updateContent();
    sampleList.repaint();
}

void SampleBrowserTab::previewRow (int row)
{
    if (! juce::isPositiveAndBelow (row, displayedEntries.size()))
    {
        previewPlayer.stop();
        return;
    }

    previewPlayer.playFile (displayedEntries.getReference (row).file);
}

void SampleBrowserTab::showContextMenu (int row, juce::Point<int> screenPos)
{
    juce::ignoreUnused (screenPos);

    if (! juce::isPositiveAndBelow (row, displayedEntries.size()))
        return;

    const auto entry = displayedEntries.getReference (row);
    juce::PopupMenu menu;

    if (contentLibrary.isFavorite (entry.file))
        menu.addItem (1, "Remove from favorites");
    else
        menu.addItem (1, "Add to favorites");

    menu.addItem (2, "Reveal in Explorer");
    menu.addItem (3, "Rescan library");

    menu.showMenuAsync ({}, [this, entry] (int result)
    {
        switch (result)
        {
            case 1:
                if (contentLibrary.isFavorite (entry.file))
                    contentLibrary.removeFavorite (entry.file);
                else
                    contentLibrary.addFavorite (entry.file);
                rebuildList();
                break;
            case 2:
                entry.file.revealToUser();
                break;
            case 3:
                contentLibrary.rescanAll();
                break;
            default:
                break;
        }
    });
}

juce::String SampleBrowserTab::formatDuration (double seconds) const
{
    if (seconds <= 0.0)
        return {};

    const int totalMs = (int) std::round (seconds * 1000.0);
    const int mins = totalMs / 60000;
    const int secs = (totalMs / 1000) % 60;
    const int ms = (totalMs / 10) % 100;
    return juce::String::formatted ("%d:%02d.%02d", mins, secs, ms);
}

} // namespace skeletonhive
