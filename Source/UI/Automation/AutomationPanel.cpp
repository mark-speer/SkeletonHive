#include "AutomationPanel.h"
#include "UI/AppLookAndFeel.h"

namespace skeletonhive
{

AutomationPanel::AutomationPanel (te::Edit& e, EditViewState& vs)
    : edit (e), editViewState (vs)
{
    trackLabel.setJustificationType (juce::Justification::centredLeft);
    trackLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));

    for (auto* button : { &readButton, &touchButton, &latchButton })
    {
        button->setRadioGroupId (100);
        button->setClickingTogglesState (true);
    }

    readButton.setTooltip ("Play back automation; no recording");
    touchButton.setTooltip ("Record automation while a control is held, punch out on release");
    latchButton.setTooltip ("Record automation from the first control move until playback stops");

    readButton.onClick = [this] { setMode (te::AutomationMode::read); };
    touchButton.onClick = [this] { setMode (te::AutomationMode::touch); };
    latchButton.onClick = [this] { setMode (te::AutomationMode::latch); };

    addParamBox.setTextWhenNothingSelected ("Add lane...");
    addParamBox.onChange = [this]
    {
        const int idx = addParamBox.getSelectedId() - 1;
        if (auto* param = parameterChoices[idx])
            addLaneForParameter (*param);

        addParamBox.setSelectedId (0, juce::dontSendNotification);
    };

    lanesViewport.setViewedComponent (&lanesHolder, false);
    lanesViewport.setScrollBarsShown (true, false);

    addAndMakeVisible (trackLabel);
    addAndMakeVisible (readButton);
    addAndMakeVisible (touchButton);
    addAndMakeVisible (latchButton);
    addAndMakeVisible (addParamBox);
    addAndMakeVisible (lanesViewport);

    edit.getAutomationRecordManager().setReadingAutomation (true);
}

void AutomationPanel::setTrack (te::Track* newTrack)
{
    if (track.get() == newTrack)
        return;

    track = newTrack;
    trackLabel.setText (track != nullptr ? track->getName() : juce::String(), juce::dontSendNotification);

    rebuildLanes();
    rebuildParameterChoices();
    syncModeButtons();
}

void AutomationPanel::rebuildLanes()
{
    lanes.clear();
    lanesHolder.removeAllChildren();

    if (track == nullptr)
        return;

    juce::Array<te::AutomatableParameter*> toShow;

    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get()))
    {
        if (auto* vol = audioTrack->getVolumePlugin())
        {
            toShow.addIfNotAlreadyThere (vol->volParam.get());
            toShow.addIfNotAlreadyThere (vol->panParam.get());
        }
    }
    else if (track->isMasterTrack())
    {
        if (auto vol = edit.getMasterVolumePlugin())
        {
            toShow.addIfNotAlreadyThere (vol->volParam.get());
            toShow.addIfNotAlreadyThere (vol->panParam.get());
        }
    }

    for (auto* param : track->getAllAutomatableParams())
        if (param != nullptr && param->hasAutomationPoints())
            toShow.addIfNotAlreadyThere (param);

    for (auto* param : toShow)
        if (param != nullptr)
            addLaneForParameter (*param);
}

void AutomationPanel::rebuildParameterChoices()
{
    parameterChoices.clear();
    addParamBox.clear (juce::dontSendNotification);

    if (track == nullptr)
        return;

    int id = 1;
    for (auto* param : track->getAllAutomatableParams())
    {
        if (param == nullptr)
            continue;

        parameterChoices.add (param);
        addParamBox.addItem (param->getFullName(), id++);
    }
}

void AutomationPanel::addLaneForParameter (te::AutomatableParameter& param)
{
    for (auto* lane : lanes)
        if (&lane->getParameter() == &param)
            return;

    auto* lane = new AutomationLaneComponent (te::AutomatableParameter::Ptr (&param), editViewState);
    lane->onRemoveLane = [this] (AutomationLaneComponent& l) { removeLane (l); };
    lanes.add (lane);
    lanesHolder.addAndMakeVisible (lane);

    layoutLanes();

    if (auto* parent = getParentComponent())
        parent->resized();
}

void AutomationPanel::removeLane (AutomationLaneComponent& lane)
{
    lanes.removeObject (&lane);
    layoutLanes();

    if (auto* parent = getParentComponent())
        parent->resized();
}

void AutomationPanel::setMode (te::AutomationMode mode)
{
    if (track != nullptr)
        track->automationMode = mode;

    auto& arm = edit.getAutomationRecordManager();
    arm.setReadingAutomation (true);

    if (mode == te::AutomationMode::read)
    {
        if (arm.isWritingAutomation())
        {
            arm.punchOut (false);
            arm.setWritingAutomation (false);
        }
    }
    else
    {
        arm.setWritingAutomation (true);
    }
}

void AutomationPanel::syncModeButtons()
{
    auto mode = te::AutomationMode::read;
    if (track != nullptr)
        mode = track->automationMode.get();

    readButton.setToggleState (mode == te::AutomationMode::read, juce::dontSendNotification);
    touchButton.setToggleState (mode == te::AutomationMode::touch, juce::dontSendNotification);
    latchButton.setToggleState (mode == te::AutomationMode::latch
                                    || mode == te::AutomationMode::write,
                                juce::dontSendNotification);
}

int AutomationPanel::getPreferredHeight() const
{
    if (track == nullptr)
        return headerHeight;

    return juce::jmin (maxPanelHeight, headerHeight + juce::jmax (1, lanes.size()) * laneHeight);
}

void AutomationPanel::layoutLanes()
{
    const int width = juce::jmax (1, lanesViewport.getWidth() - lanesViewport.getScrollBarThickness());
    lanesHolder.setSize (width, lanes.size() * laneHeight);

    int y = 0;
    for (auto* lane : lanes)
    {
        lane->setBounds (0, y, width, laneHeight);
        y += laneHeight;
    }
}

void AutomationPanel::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (headerHeight).reduced (2);

    trackLabel.setBounds (header.removeFromLeft (160));
    readButton.setBounds (header.removeFromLeft (56).reduced (1));
    touchButton.setBounds (header.removeFromLeft (56).reduced (1));
    latchButton.setBounds (header.removeFromLeft (56).reduced (1));
    header.removeFromLeft (8);
    addParamBox.setBounds (header.removeFromLeft (220).reduced (1));

    lanesViewport.setBounds (r);
    layoutLanes();
}

void AutomationPanel::paint (juce::Graphics& g)
{
    g.fillAll (AppColours::automationPanelBackground (AppLookAndFeel::getCurrentTheme()));

    if (track == nullptr)
    {
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.drawText ("Select a track (or Master) to edit its automation",
                    getLocalBounds(), juce::Justification::centred);
    }
}

} // namespace skeletonhive
