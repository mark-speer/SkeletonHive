#include "PlacesBrowserTab.h"

namespace skeletonhive
{

class PlacesBrowserTab::RootTreeItem : public juce::TreeViewItem
{
public:
    enum class ItemKind { sectionHeader, folder, favoritesShortcut };

    RootTreeItem (PlacesBrowserTab& owner, juce::String title, juce::File folder, ItemKind kind)
        : tab (owner), label (std::move (title)), rootFolder (std::move (folder)), itemKind (kind)
    {
    }

    bool mightContainSubItems() override { return itemKind == ItemKind::folder && rootFolder.isDirectory(); }

    void paintItem (juce::Graphics& g, int width, int height) override
    {
        if (isSelected())
            g.fillAll (juce::Colours::white.withAlpha (0.12f));

        g.setColour (itemKind == ItemKind::sectionHeader ? juce::Colours::white.withAlpha (0.55f)
                                                           : juce::Colours::white.withAlpha (0.9f));
        g.setFont (juce::FontOptions (itemKind == ItemKind::sectionHeader ? 11.0f : 13.0f));
        g.drawText (label, 4, 0, width - 8, height, juce::Justification::centredLeft, true);
    }

    void itemClicked (const juce::MouseEvent&) override
    {
        if (itemKind == ItemKind::favoritesShortcut)
        {
            if (tab.onShowFavorites)
                tab.onShowFavorites();
            return;
        }

        if (itemKind != ItemKind::folder || ! rootFolder.isDirectory())
            return;

        if (tab.onPlaceSelected)
            tab.onPlaceSelected (rootFolder);
    }

    void itemOpennessChanged (bool isNowOpen) override
    {
        if (! isNowOpen || itemKind != ItemKind::folder)
            return;

        clearSubItems();

        for (const auto& child : rootFolder.findChildFiles (juce::File::findDirectories, false))
            addSubItem (new RootTreeItem (tab, child.getFileName(), child, ItemKind::folder));
    }

private:
    PlacesBrowserTab& tab;
    juce::String label;
    juce::File rootFolder;
    ItemKind itemKind;
};

PlacesBrowserTab::PlacesBrowserTab (ContentLibraryManager& library)
    : contentLibrary (library)
{
    addAndMakeVisible (treeView);
    refreshPlaces();
}

void PlacesBrowserTab::refreshPlaces()
{
    rootItem.reset (new RootTreeItem (*this, "Places", juce::File(), RootTreeItem::ItemKind::sectionHeader));

    if (const auto project = contentLibrary.getProjectFolder(); project.isDirectory())
        rootItem->addSubItem (new RootTreeItem (*this, "Project: " + project.getFileName(), project, RootTreeItem::ItemKind::folder));

    for (const auto& path : contentLibrary.getLibraryRoots())
        rootItem->addSubItem (new RootTreeItem (*this, path.getFileName(), path, RootTreeItem::ItemKind::folder));

    rootItem->addSubItem (new RootTreeItem (*this, "Favorites", juce::File(), RootTreeItem::ItemKind::favoritesShortcut));

    treeView.setRootItem (rootItem.get());
    treeView.setRootItemVisible (false);
    treeView.setDefaultOpenness (true);
}

void PlacesBrowserTab::resized()
{
    treeView.setBounds (getLocalBounds().reduced (4));
}

} // namespace skeletonhive
