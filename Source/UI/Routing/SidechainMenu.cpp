#include "SidechainMenu.h"
#include "Engine/SidechainRouting.h"

namespace skeletonhive
{

void SidechainMenu::addSidechainMenuItems (juce::PopupMenu& menu, te::Plugin& plugin)
{
    if (! plugin.canSidechain())
        return;

    menu.addItem (openMatrixId, "Sidechain Routing...");

    juce::PopupMenu quickPick;
    const auto sources = SidechainRouting::getCandidateSources (plugin.edit, plugin);
    quickPick.addItem (sidechainBase, "None", true,
                       SidechainRouting::isSourceSelected (plugin, {}));

    for (int i = 0; i < sources.size(); ++i)
    {
        const auto& track = *sources[i];
        quickPick.addItem (sidechainBase + 1 + i,
                           SidechainRouting::formatSourceMenuName (i + 1, track.getName()),
                           true,
                           SidechainRouting::isSourceSelected (plugin, track.itemID));
    }

    menu.addSubMenu ("Sidechain Source", quickPick, true);
}

bool SidechainMenu::handleSidechainMenuResult (int result,
                                               te::Plugin& plugin,
                                               int moveToRackBase,
                                               std::function<void()> onChanged)
{
    if (result == openMatrixId)
    {
        if (SidechainRouting::openMatrixForPlugin)
            SidechainRouting::openMatrixForPlugin (&plugin);
        return true;
    }

    if (result < sidechainBase || result >= moveToRackBase)
        return false;

    if (result == sidechainBase)
    {
        SidechainRouting::setSidechainSource (plugin, nullptr);
    }
    else
    {
        const auto sources = SidechainRouting::getCandidateSources (plugin.edit, plugin);
        const int idx = result - sidechainBase - 1;

        if (juce::isPositiveAndBelow (idx, sources.size()))
            SidechainRouting::setSidechainSource (plugin, sources[idx]);
    }

    plugin.edit.restartPlayback();

    if (onChanged)
        onChanged();

    return true;
}

} // namespace skeletonhive
