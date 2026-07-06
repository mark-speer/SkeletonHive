#pragma once

#include "Engine/ClipLibraryManager.h"

namespace skeletonhive
{

class ClipsBrowserTab : public juce::Component,
                        private juce::TextEditor::Listener,
                        private juce::ComboBox::Listener,
                        private juce::ChangeListener,
                        public juce::DragAndDropTarget
{
public:
    ClipsBrowserTab (ClipLibraryManager& library, te::Edit& edit);

    void refreshList();

    std::function<void()> onLibraryChanged;

    void resized() override;

    bool isInterestedInDragSource (const SourceDetails& dragSourceDetails) override;
    void itemDropped (const SourceDetails& dragSourceDetails) override;

private:
    friend class ClipListModel;
    class ClipListModel;

    class DraggableClipListBox : public juce::ListBox
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

    void rebuildList();
    void startClipDrag (const juce::MouseEvent& e, int row);
    void saveDroppedClip (te::EditItemID clipId);
    void promptAndSaveClip (te::Clip& clip);
    void showContextMenu (int row, juce::Point<int> screenPos);
    juce::String formatDuration (double seconds) const;

    ClipLibraryManager& clipLibrary;
    te::Edit& editRef;

    juce::TextEditor searchBox;
    juce::ComboBox sortBox;
    DraggableClipListBox clipList;
    juce::Label statusLabel;

    juce::Array<ClipLibraryEntry> displayedEntries;
    std::unique_ptr<ClipListModel> listModel;
};

} // namespace skeletonhive
