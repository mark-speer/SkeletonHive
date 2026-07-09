#include "TrackHeaderControls.h"

namespace skeletonhive
{

MonitorModeButton::MonitorModeButton()
{
    setClickingTogglesState (false);
    onClick = [this] { cycleMode(); };
}

void MonitorModeButton::bindToTrack (te::Track::Ptr t)
{
    track = std::move (t);
    updateFromModel();
}

void MonitorModeButton::updateFromModel()
{
    if (track == nullptr)
        return;

    applyVisuals (TrackMonitorRouting::getMonitorMode (*track));
}

void MonitorModeButton::cycleMode()
{
    if (track == nullptr)
        return;

    const auto current = TrackMonitorRouting::getMonitorMode (*track);
    TrackMonitorMode next = TrackMonitorMode::autoMode;

    switch (current)
    {
        case TrackMonitorMode::off:      next = TrackMonitorMode::autoMode; break;
        case TrackMonitorMode::autoMode: next = TrackMonitorMode::in; break;
        case TrackMonitorMode::in:       next = TrackMonitorMode::off; break;
    }

    TrackMonitorRouting::setMonitorMode (*track, next, &track->edit.getUndoManager());
    applyVisuals (next);

    if (onModeChanged)
        onModeChanged (next);
}

void MonitorModeButton::applyVisuals (TrackMonitorMode mode)
{
    const auto theme = AppLookAndFeel::getCurrentTheme();
    setButtonText (TrackMonitorRouting::monitorModeDisplayName (mode));
    setTooltip (TrackMonitorRouting::monitorModeTooltip (mode));

    juce::Colour bg = AppColours::monitorOff (theme);

    switch (mode)
    {
        case TrackMonitorMode::autoMode: bg = AppColours::monitorAuto (theme); break;
        case TrackMonitorMode::in:       bg = AppColours::monitorIn (theme); break;
        case TrackMonitorMode::off:
        default:                         break;
    }

    setColour (juce::TextButton::buttonColourId, bg);
    setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

RoutingSelectorButton::RoutingSelectorButton()
{
    setClickingTogglesState (false);
    onClick = [this] { showMenu(); };
}

void RoutingSelectorButton::setDisplayText (const juce::String& displayText, bool available)
{
    setButtonText (displayText);
    setEnabled (true);
    setAlpha (available ? 1.0f : 0.55f);
}

void RoutingSelectorButton::setRouteTooltip (const juce::String& tip)
{
    setTooltip (tip);
}

void RoutingSelectorButton::showMenu()
{
    if (menuFactory == nullptr)
        return;

    auto menu = menuFactory();

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [this] (int result)
    {
        if (result > 0 && onItemSelected)
            onItemSelected (result);
    });
}

HeaderGainSlider::HeaderGainSlider()
{
    setSliderStyle (juce::Slider::LinearHorizontal);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRange (0.0, 1.0, 0.001);
}

HeaderGainSlider::~HeaderGainSlider()
{
    if (volumePlugin != nullptr && volumePlugin->volParam != nullptr)
        volumePlugin->volParam->removeListener (this);
}

void HeaderGainSlider::bindToPlugin (te::VolumeAndPanPlugin::Ptr plugin)
{
    if (volumePlugin != nullptr && volumePlugin->volParam != nullptr)
        volumePlugin->volParam->removeListener (this);

    volumePlugin = plugin;

    if (volumePlugin != nullptr && volumePlugin->volParam != nullptr)
    {
        volumePlugin->volParam->addListener (this);
        updateFromModel();
        onValueChange = [this]
        {
            if (volumePlugin != nullptr)
                volumePlugin->setSliderPos ((float) getValue());
        };
    }
    else
    {
        onValueChange = nullptr;
    }
}

void HeaderGainSlider::updateFromModel()
{
    if (volumePlugin != nullptr)
        setValue (volumePlugin->getSliderPos(), juce::dontSendNotification);
}

HeaderPanSlider::HeaderPanSlider()
{
    setSliderStyle (juce::Slider::LinearHorizontal);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRange (-1.0, 1.0, 0.01);
}

HeaderPanSlider::~HeaderPanSlider()
{
    if (volumePlugin != nullptr && volumePlugin->panParam != nullptr)
        volumePlugin->panParam->removeListener (this);
}

void HeaderPanSlider::bindToPlugin (te::VolumeAndPanPlugin::Ptr plugin)
{
    if (volumePlugin != nullptr && volumePlugin->panParam != nullptr)
        volumePlugin->panParam->removeListener (this);

    volumePlugin = plugin;

    if (volumePlugin != nullptr && volumePlugin->panParam != nullptr)
    {
        volumePlugin->panParam->addListener (this);
        updateFromModel();
        onValueChange = [this]
        {
            if (volumePlugin != nullptr)
                volumePlugin->setPan ((float) getValue());
        };
    }
    else
    {
        onValueChange = nullptr;
    }
}

void HeaderPanSlider::updateFromModel()
{
    if (volumePlugin != nullptr)
        setValue (volumePlugin->getPan(), juce::dontSendNotification);
}

TrackColourSwatch::TrackColourSwatch()
{
    setInterceptsMouseClicks (true, false);
}

void TrackColourSwatch::bindToTrack (te::Track::Ptr t)
{
    track = std::move (t);
    updateFromModel();
}

void TrackColourSwatch::updateFromModel()
{
    repaint();
}

void TrackColourSwatch::mouseDown (const juce::MouseEvent& e)
{
    if (track == nullptr || ! e.mods.isLeftButtonDown())
        return;

    auto selector = std::make_unique<juce::ColourSelector>();
    selector->setName ("Track Colour");
    selector->setCurrentColour (EngineHelpers::getTrackDisplayColour (*track));
    selector->setSize (300, 400);

    auto* selectorPtr = selector.get();
    selectorPtr->addChangeListener (this);
    activeSelector = selectorPtr;

    juce::CallOutBox::launchAsynchronously (std::move (selector), getScreenBounds(), nullptr);
}

void TrackColourSwatch::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (track == nullptr)
        return;

    if (auto* selector = dynamic_cast<juce::ColourSelector*> (source))
    {
        EngineHelpers::setTrackDisplayColour (*track, selector->getCurrentColour());
        repaint();
    }
}

void TrackColourSwatch::paint (juce::Graphics& g)
{
    if (track == nullptr)
        return;

    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (EngineHelpers::getTrackDisplayColour (*track));
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
}

} // namespace skeletonhive
