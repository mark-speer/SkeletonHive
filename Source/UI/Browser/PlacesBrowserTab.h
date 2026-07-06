#pragma once

#include "Engine/ContentLibraryManager.h"

namespace skeletonhive
{

class PlacesBrowserTab : public juce::Component
{
public:
    PlacesBrowserTab (ContentLibraryManager& library);

    std::function<void (const juce::File& root)> onPlaceSelected;
    std::function<void()> onShowFavorites;

    void refreshPlaces();
    void resized() override;

private:
    class RootTreeItem;

    ContentLibraryManager& contentLibrary;
    juce::TreeView treeView;
    std::unique_ptr<RootTreeItem> rootItem;
};

} // namespace skeletonhive
