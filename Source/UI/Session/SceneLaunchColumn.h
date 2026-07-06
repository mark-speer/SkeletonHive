#pragma once

#include "Engine/SessionManager.h"

namespace skeletonhive
{

class SceneLaunchColumn : public juce::Component,
                          private juce::ChangeListener
{
public:
    explicit SceneLaunchColumn (SessionManager& session);

    void setSceneCount (int count);
    void layoutButtons (int rowHeight);

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void rebuildButtons();

    SessionManager& sessionManager;
    juce::OwnedArray<juce::TextButton> sceneButtons;
    int sceneCount = 8;
    int rowHeightPx = 80;
};

} // namespace skeletonhive
