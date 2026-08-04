#pragma once

#include "Engine/ContentLibraryManager.h"
#include "Engine/PreviewPlayer.h"
#include "Engine/WaveformCache.h"
#include "UI/Arrangement/EditViewState.h"

namespace skeletonhive
{

class SampleBrowserTab : public juce::Component,
                         private juce::TextEditor::Listener,
                         private juce::ComboBox::Listener,
                         private juce::ChangeListener,
                         private juce::Timer
{
public:
    SampleBrowserTab (ContentLibraryManager& library,
                      PreviewPlayer& preview,
                      WaveformCache& waveforms,
                      te::Engine& engine);
    ~SampleBrowserTab() override;

    void setRootFilter (const juce::File& root);
    void showFavoritesFilter();
    void refreshList();

    void resized() override;
    void mouseExit (const juce::MouseEvent& e) override;
    void visibilityChanged() override;

private:
    friend class SampleListModel;
    class SampleListModel;

    class DraggableSampleListBox : public juce::ListBox
    {
    public:
        std::function<void (const juce::MouseEvent&, int row)> onStartDrag;

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (e.getDistanceFromDragStart() < 6 || onStartDrag == nullptr)
                return;

            const int row = getRowContainingPosition (e.x, e.y);
            onStartDrag (e, row);
        }
    };

    void textEditorTextChanged (juce::TextEditor&) override;
    void comboBoxChanged (juce::ComboBox*) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void rebuildList();
    void stopPreview();
    void previewRow (int row);
    void startSampleDrag (const juce::MouseEvent& e, int row);
    void showContextMenu (int row, juce::Point<int> screenPos);
    juce::String formatDuration (double seconds) const;

    ContentLibraryManager& contentLibrary;
    PreviewPlayer& previewPlayer;
    WaveformCache& waveformCache;
    te::Engine& engineRef;

    juce::TextEditor searchBox;
    juce::ComboBox sortBox;
    juce::ComboBox filterBox;
    DraggableSampleListBox sampleList;
    juce::TextButton rescanButton { "Rescan" };
    juce::Label statusLabel;

    juce::File rootFilter;
    juce::Array<ContentEntry> displayedEntries;
    ContentEntry selectedEntry;
    std::unique_ptr<SampleListModel> listModel;
};

} // namespace skeletonhive
