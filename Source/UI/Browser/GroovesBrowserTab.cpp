#include "GroovesBrowserTab.h"

namespace skeletonhive
{

class GroovesBrowserTab::GrooveListModel : public juce::ListBoxModel
{
public:
    explicit GrooveListModel (GroovesBrowserTab& owner) : tab (owner) {}

    int getNumRows() override { return tab.displayedTemplates.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (! juce::isPositiveAndBelow (row, tab.displayedTemplates.size()))
            return;

        if (rowIsSelected)
            g.fillAll (juce::Colours::steelblue.withAlpha (0.35f));

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (13.0f));

        const auto& t = tab.displayedTemplates.getReference (row);
        juce::String suffix;

        if (t.isBuiltIn)
            suffix = " (built-in)";
        else if (t.isRandom)
            suffix = " (random)";

        g.drawText (t.name + suffix, 4, 0, width - 8, height, juce::Justification::centredLeft, true);
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
    {
        if (juce::isPositiveAndBelow (row, tab.displayedTemplates.size()))
        {
            tab.grooveList.selectRow (row);
            tab.applySelectedGroove();
        }
    }

    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            tab.showContextMenu (row, e.getScreenPosition());
    }

private:
    GroovesBrowserTab& tab;
};

GroovesBrowserTab::GroovesBrowserTab (GroovePoolManager& pool, te::Edit& edit, te::SelectionManager& selection)
    : groovePool (pool),
      editRef (edit),
      selectionManager (selection)
{
    listModel = std::make_unique<GrooveListModel> (*this);
    grooveList.setModel (listModel.get());
    grooveList.setRowHeight (22);

    applyButton.onClick = [this] { applySelectedGroove(); };
    addButton.onClick = [this]
    {
        groovePool.addTemplate ("New Groove");
        refreshList();
        grooveList.selectRow (groovePool.getAllTemplates().size() - 1);
    };

    groovePool.addChangeListener (this);

    addAndMakeVisible (grooveList);
    addAndMakeVisible (applyButton);
    addAndMakeVisible (addButton);
    addAndMakeVisible (statusLabel);

    statusLabel.setFont (juce::FontOptions (11.0f));
    statusLabel.setJustificationType (juce::Justification::centredLeft);

    refreshList();
}

void GroovesBrowserTab::refreshList()
{
    displayedTemplates = groovePool.getAllTemplates();
    grooveList.updateContent();

    const int selectedIdx = groovePool.indexOfTemplate (groovePool.getSelectedGrooveId());

    if (selectedIdx >= 0)
        grooveList.selectRow (selectedIdx);

    statusLabel.setText (juce::String (displayedTemplates.size()) + " groove"
                         + (displayedTemplates.size() == 1 ? "" : "s"),
                         juce::dontSendNotification);
}

void GroovesBrowserTab::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &groovePool)
        refreshList();
}

void GroovesBrowserTab::applySelectedGroove()
{
    const int row = grooveList.getSelectedRow();

    if (! juce::isPositiveAndBelow (row, displayedTemplates.size()))
        return;

    const auto& groove = displayedTemplates.getReference (row);
    groovePool.setSelectedGrooveId (groove.id);

    juce::String error;
    const int count = EngineHelpers::applyGrooveToSelection (editRef, selectionManager, groove, &error);

    if (count == 0 && error.isNotEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "Apply Groove", error);
    else if (onGrooveApplied)
        onGrooveApplied();
}

void GroovesBrowserTab::showContextMenu (int row, juce::Point<int> screenPos)
{
    if (! juce::isPositiveAndBelow (row, displayedTemplates.size()))
        return;

    const auto& groove = displayedTemplates.getReference (row);

    juce::PopupMenu menu;
    menu.addItem (1, "Apply to Selection");
    menu.addItem (2, "Duplicate", ! groove.isBuiltIn);
    menu.addItem (3, "Rename...", ! groove.isBuiltIn);
    menu.addItem (4, "Delete", ! groove.isBuiltIn);

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (this)
                            .withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
                        [this, groove] (int result)
    {
        switch (result)
        {
            case 1:
                groovePool.setSelectedGrooveId (groove.id);
                applySelectedGroove();
                break;
            case 2:
                groovePool.duplicateTemplate (groove.id);
                refreshList();
                break;
            case 3:
            {
                auto w = std::make_shared<juce::AlertWindow> ("Rename Groove",
                                                              "Enter a new name:",
                                                              juce::AlertWindow::QuestionIcon);
                w->addTextEditor ("name", groove.name);
                w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
                w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

                w->enterModalState (true, juce::ModalCallbackFunction::create ([w, this, id = groove.id] (int result)
                {
                    if (result != 1)
                        return;

                    const auto newName = w->getTextEditorContents ("name").trim();

                    if (newName.isNotEmpty())
                        groovePool.renameTemplate (id, newName);
                }));
                break;
            }
            case 4:
                groovePool.removeTemplate (groove.id);
                refreshList();
                break;
            default:
                break;
        }
    });
}

void GroovesBrowserTab::resized()
{
    auto r = getLocalBounds().reduced (4);
    auto buttonRow = r.removeFromBottom (28);
    applyButton.setBounds (buttonRow.removeFromLeft (120).reduced (0, 2));
    addButton.setBounds (buttonRow.removeFromLeft (60).reduced (2, 2));
    statusLabel.setBounds (buttonRow.reduced (4, 0));
    grooveList.setBounds (r);
}

} // namespace skeletonhive
