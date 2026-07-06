#include "ClipsBrowserTab.h"
#include "Engine/ContentDragManager.h"

namespace skeletonhive
{

class ClipsBrowserTab::ClipListModel : public juce::ListBoxModel
{
public:
    explicit ClipListModel (ClipsBrowserTab& owner) : tab (owner) {}

    int getNumRows() override { return tab.displayedEntries.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, tab.displayedEntries.size()))
            return;

        const auto& entry = tab.displayedEntries.getReference (row);

        if (selected)
            g.fillAll (juce::Colours::white.withAlpha (0.18f));

        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.setFont (juce::FontOptions ((float) height * 0.42f));
        g.drawText (entry.name, 4, 0, width - 8, height / 2, juce::Justification::centredLeft, true);

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (juce::FontOptions ((float) height * 0.34f));
        const auto subtitle = entry.category + "  ·  " + entry.clipType + "  ·  "
                            + tab.formatDuration (entry.lengthSeconds);
        g.drawText (subtitle, 4, height / 2, width - 8, height / 2, juce::Justification::centredLeft, true);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            tab.showContextMenu (row, e.getScreenPosition());
    }

    ClipsBrowserTab& tab;
};

ClipsBrowserTab::ClipsBrowserTab (ClipLibraryManager& library, te::Edit& edit)
    : clipLibrary (library),
      editRef (edit)
{
    searchBox.setTextToShowWhenEmpty ("Search clips...", juce::Colours::grey);
    searchBox.addListener (this);

    sortBox.addItem ("Name", 1);
    sortBox.addItem ("Date modified", 2);
    sortBox.setSelectedId (1, juce::dontSendNotification);
    sortBox.addListener (this);

    listModel = std::make_unique<ClipListModel> (*this);
    clipList.setModel (listModel.get());
    clipList.setRowHeight (36);
    clipList.onStartDrag = [this] (const juce::MouseEvent& e, int row)
    {
        startClipDrag (e, row);
    };

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setFont (juce::FontOptions (11.0f));

    clipLibrary.addChangeListener (this);

    addAndMakeVisible (searchBox);
    addAndMakeVisible (sortBox);
    addAndMakeVisible (clipList);
    addAndMakeVisible (statusLabel);

    rebuildList();
}

void ClipsBrowserTab::refreshList()
{
    rebuildList();
}

void ClipsBrowserTab::resized()
{
    auto r = getLocalBounds().reduced (6);
    searchBox.setBounds (r.removeFromTop (24));
    r.removeFromTop (4);
    sortBox.setBounds (r.removeFromTop (24));
    r.removeFromTop (4);
    statusLabel.setBounds (r.removeFromBottom (20));
    r.removeFromBottom (4);
    clipList.setBounds (r);
}

bool ClipsBrowserTab::isInterestedInDragSource (const SourceDetails& details)
{
    return details.description.toString().startsWith (ContentDragTypes::clipExport);
}

void ClipsBrowserTab::itemDropped (const SourceDetails& details)
{
    const auto payload = ClipExportDragPayload::parse (details.description);

    if (payload.isValid())
        saveDroppedClip (te::EditItemID::fromRawID ((juce::uint64) payload.clipItemId));
}

void ClipsBrowserTab::textEditorTextChanged (juce::TextEditor&)
{
    rebuildList();
}

void ClipsBrowserTab::comboBoxChanged (juce::ComboBox*)
{
    rebuildList();
}

void ClipsBrowserTab::changeListenerCallback (juce::ChangeBroadcaster*)
{
    rebuildList();
}

void ClipsBrowserTab::rebuildList()
{
    const auto sort = sortBox.getSelectedId() == 2 ? ClipLibrarySortMode::dateModified
                                                   : ClipLibrarySortMode::name;
    displayedEntries = clipLibrary.getEntries (searchBox.getText(), sort);
    clipList.updateContent();
    clipList.repaint();
    statusLabel.setText (juce::String (displayedEntries.size()) + " clip presets",
                         juce::dontSendNotification);
}

void ClipsBrowserTab::startClipDrag (const juce::MouseEvent& e, int row)
{
    if (! juce::isPositiveAndBelow (row, displayedEntries.size()))
        return;

    const auto& entry = displayedEntries.getReference (row);

    if (auto* container = findParentComponentOfClass<juce::DragAndDropContainer>())
    {
        ClipPresetDragPayload payload;
        payload.presetFile = entry.presetFile;
        container->startDragging (payload.encode(), &clipList, juce::ScaledImage(), true, nullptr, &e.source);
    }
}

void ClipsBrowserTab::saveDroppedClip (te::EditItemID clipId)
{
    if (auto* clip = clipLibrary.findClipById (editRef, clipId))
        promptAndSaveClip (*clip);
}

void ClipsBrowserTab::promptAndSaveClip (te::Clip& clip)
{
    auto w = std::make_shared<juce::AlertWindow> ("Save Clip Preset",
                                                  "Enter a name for this clip preset:",
                                                  juce::AlertWindow::QuestionIcon);
    w->addTextEditor ("name", clip.getName());
    w->addTextEditor ("category", "User");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, this, clipPtr = te::Clip::Ptr (&clip)] (int result) mutable
    {
        if (result != 1 || clipPtr == nullptr)
            return;

        const auto name = w->getTextEditorContents ("name").trim();
        const auto category = w->getTextEditorContents ("category").trim();

        if (name.isEmpty())
            return;

        if (clipLibrary.saveClip (*clipPtr, name, category).existsAsFile())
        {
            if (onLibraryChanged)
                onLibraryChanged();
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                    "Save Clip Preset",
                                                    "Could not save the clip preset.");
        }
    }));
}

void ClipsBrowserTab::showContextMenu (int row, juce::Point<int> screenPos)
{
    juce::ignoreUnused (screenPos);

    if (! juce::isPositiveAndBelow (row, displayedEntries.size()))
        return;

    const auto entry = displayedEntries.getReference (row);
    juce::PopupMenu menu;
    menu.addItem (1, "Delete preset");

    menu.showMenuAsync ({}, [this, entry] (int result)
    {
        if (result != 1)
            return;

        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon,
                                            "Delete Clip Preset",
                                            "Delete \"" + entry.name + "\" from the library?",
                                            "Delete", "Cancel", nullptr,
                                            juce::ModalCallbackFunction::create ([this, entry] (int button)
        {
            if (button != 1)
                return;

            if (! clipLibrary.deletePreset (entry.presetFile))
            {
                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                        "Delete Clip Preset",
                                                        "Could not delete the clip preset.");
            }
            else if (onLibraryChanged)
            {
                onLibraryChanged();
            }
        }));
    });
}

juce::String ClipsBrowserTab::formatDuration (double seconds) const
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
