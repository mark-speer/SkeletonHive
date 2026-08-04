#pragma once

#include "Engine/GroovePoolManager.h"
#include "Engine/EngineHelpers.h"

namespace skeletonhive
{

class GroovesBrowserTab : public juce::Component,
                          private juce::ChangeListener
{
public:
    GroovesBrowserTab (GroovePoolManager& pool, te::Edit& edit, te::SelectionManager& selection);
    ~GroovesBrowserTab() override;

    void refreshList();

    std::function<void()> onGrooveApplied;

    void resized() override;

private:
    friend class GrooveListModel;
    class GrooveListModel;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void applySelectedGroove();
    void showContextMenu (int row, juce::Point<int> screenPos);

    GroovePoolManager& groovePool;
    te::Edit& editRef;
    te::SelectionManager& selectionManager;

    juce::ListBox grooveList;
    juce::TextButton applyButton { "Apply to Selection" };
    juce::TextButton addButton { "New" };
    juce::Label statusLabel;

    juce::Array<GrooveTemplate> displayedTemplates;
    std::unique_ptr<GrooveListModel> listModel;
};

} // namespace skeletonhive
