#include "DetailPanelStack.h"
#include "UI/AppLookAndFeel.h"

namespace skeletonhive
{

DetailPanelStack::DetailPanelStack (std::unique_ptr<PluginTrayComponent> tray,
                                    std::unique_ptr<ClipInspectorPanel> inspector)
    : pluginTray (std::move (tray)),
      clipInspector (std::move (inspector))
{
    devicesTab.setClickingTogglesState (true);
    clipTab.setClickingTogglesState (true);
    devicesTab.setRadioGroupId (1001);
    clipTab.setRadioGroupId (1001);
    devicesTab.setToggleState (true, juce::dontSendNotification);

    devicesTab.onClick = [this]
    {
        userPinnedView = true;
        setActiveView (DetailView::devices);
    };

    clipTab.onClick = [this]
    {
        userPinnedView = true;
        setActiveView (DetailView::clip);
    };

    addAndMakeVisible (devicesTab);
    addAndMakeVisible (clipTab);

    if (pluginTray != nullptr)
        addAndMakeVisible (*pluginTray);

    if (clipInspector != nullptr)
        addAndMakeVisible (*clipInspector);

    refreshVisibility();
}

void DetailPanelStack::setActiveView (DetailView view)
{
    activeView = view;
    devicesTab.setToggleState (view == DetailView::devices, juce::dontSendNotification);
    clipTab.setToggleState (view == DetailView::clip, juce::dontSendNotification);
    refreshVisibility();
    resized();
}

void DetailPanelStack::autoSelectView (bool hasAudioClipSelection)
{
    hasAudioClips = hasAudioClipSelection;
    const bool hasClipTab = clipInspector != nullptr && clipInspector->hasClipSelection();

    if (userPinnedView)
    {
        if (activeView == DetailView::clip && ! hasClipTab)
            setActiveView (DetailView::devices);

        return;
    }

    if (hasClipTab)
        setActiveView (DetailView::clip);
    else
        setActiveView (DetailView::devices);
}

void DetailPanelStack::updateClipSelection (const juce::Array<te::Clip*>& clips)
{
    if (clipInspector != nullptr)
        clipInspector->setClips (clips);

    autoSelectView (clipInspector != nullptr && clipInspector->hasAudioSelection());
}

int DetailPanelStack::getPreferredHeight() const
{
    const int contentHeight = activeView == DetailView::clip && clipInspector != nullptr
                                ? clipInspector->getPreferredHeight()
                                : devicesContentHeight;
    return tabBarHeight + contentHeight;
}

void DetailPanelStack::refreshVisibility()
{
    const bool showClipTab = clipInspector != nullptr && clipInspector->hasClipSelection();

    if (pluginTray != nullptr)
        pluginTray->setVisible (activeView == DetailView::devices);

    if (clipInspector != nullptr)
        clipInspector->setVisible (activeView == DetailView::clip && showClipTab);
}

void DetailPanelStack::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2e));
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());
}

void DetailPanelStack::resized()
{
    auto r = getLocalBounds();

    auto tabRow = r.removeFromTop (tabBarHeight).reduced (4, 2);
    devicesTab.setBounds (tabRow.removeFromLeft (72));
    clipTab.setBounds (tabRow.removeFromLeft (56));

    const int contentHeight = activeView == DetailView::clip && clipInspector != nullptr
                                ? clipInspector->getPreferredHeight()
                                : devicesContentHeight;
    auto contentArea = r.removeFromTop (contentHeight);

    if (pluginTray != nullptr && activeView == DetailView::devices)
        pluginTray->setBounds (contentArea);

    if (clipInspector != nullptr && activeView == DetailView::clip && clipInspector->hasClipSelection())
        clipInspector->setBounds (contentArea);
}

} // namespace skeletonhive
