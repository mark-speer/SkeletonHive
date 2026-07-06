#include "SceneLaunchColumn.h"

namespace skeletonhive
{

SceneLaunchColumn::SceneLaunchColumn (SessionManager& session)
    : sessionManager (session)
{
    sessionManager.addChangeListener (this);
    setSceneCount (sessionManager.getSceneCount());
}

void SceneLaunchColumn::setSceneCount (int count)
{
    sceneCount = juce::jmax (1, count);
    rebuildButtons();
}

void SceneLaunchColumn::layoutButtons (int rowHeight)
{
    rowHeightPx = juce::jmax (48, rowHeight);
    auto area = getLocalBounds().reduced (2);

    for (int i = 0; i < sceneButtons.size(); ++i)
    {
        if (auto* btn = sceneButtons[i])
            btn->setBounds (area.removeFromTop (rowHeightPx).reduced (2));
    }
}

void SceneLaunchColumn::rebuildButtons()
{
    sceneButtons.clear();

    for (int i = 0; i < sceneCount; ++i)
    {
        auto* btn = sceneButtons.add (new juce::TextButton (juce::String (i + 1)));
        btn->setTooltip ("Launch scene " + juce::String (i + 1));
        btn->onClick = [this, i] { sessionManager.launchScene (i); };
        addAndMakeVisible (*btn);
    }

    layoutButtons (rowHeightPx);
}

void SceneLaunchColumn::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (sceneCount != sessionManager.getSceneCount())
        setSceneCount (sessionManager.getSceneCount());
}

} // namespace skeletonhive
