#pragma once

#include "Engine/EngineHelpers.h"
#include "Engine/TrackInputRouting.h"
#include "Engine/TrackMonitorRouting.h"
#include "Engine/TrackOutputRouting.h"
#include "UI/AppLookAndFeel.h"
#include <functional>

namespace skeletonhive
{

class MonitorModeButton : public juce::TextButton
{
public:
    MonitorModeButton();

    void bindToTrack (te::Track::Ptr t);
    void updateFromModel();

    std::function<void (TrackMonitorMode)> onModeChanged;

private:
    void cycleMode();
    void applyVisuals (TrackMonitorMode mode);

    te::Track::Ptr track;
};

class RoutingSelectorButton : public juce::TextButton
{
public:
    RoutingSelectorButton();

    void setDisplayText (const juce::String& text, bool available = true);
    void setRouteTooltip (const juce::String& tip);

    std::function<juce::PopupMenu()> menuFactory;
    std::function<void (int)> onItemSelected;

private:
    void showMenu();
};

class HeaderGainSlider : public juce::Slider,
                         private te::AutomatableParameter::Listener
{
public:
    HeaderGainSlider();
    ~HeaderGainSlider() override;

    void bindToPlugin (te::VolumeAndPanPlugin::Ptr plugin);
    void updateFromModel();

private:
    void curveHasChanged (te::AutomatableParameter&) override {}
    void currentValueChanged (te::AutomatableParameter&) override { updateFromModel(); }

    te::VolumeAndPanPlugin::Ptr volumePlugin;
};

class HeaderPanSlider : public juce::Slider,
                        private te::AutomatableParameter::Listener
{
public:
    HeaderPanSlider();
    ~HeaderPanSlider() override;

    void bindToPlugin (te::VolumeAndPanPlugin::Ptr plugin);
    void updateFromModel();

private:
    void curveHasChanged (te::AutomatableParameter&) override {}
    void currentValueChanged (te::AutomatableParameter&) override { updateFromModel(); }

    te::VolumeAndPanPlugin::Ptr volumePlugin;
};

class TrackColourSwatch : public juce::Component,
                          public juce::SettableTooltipClient,
                          private juce::ChangeListener
{
public:
    TrackColourSwatch();

    void bindToTrack (te::Track::Ptr t);
    void updateFromModel();
    void setDescription (const juce::String& tip) { setTooltip (tip); }

    void mouseDown (const juce::MouseEvent& e) override;
    void paint (juce::Graphics& g) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    te::Track::Ptr track;
    juce::Component::SafePointer<juce::ColourSelector> activeSelector;
};

} // namespace skeletonhive
