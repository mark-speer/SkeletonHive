#include "ChannelStrip.h"
#include "Engine/EngineHelpers.h"
#include "Engine/UiTelemetryHub.h"

namespace skeletonhive
{

namespace
{
juce::String formatVolumeDbText (float db)
{
    if (db <= -99.5f)
        return "-inf dB";

    return juce::String (db, 1) + " dB";
}

juce::String formatPanText (float pan)
{
    if (std::abs (pan) < 0.005f)
        return "C";

    const int amount = juce::jlimit (1, 50, juce::roundToInt (std::abs (pan) * 50.0f));
    return juce::String (amount) + (pan < 0.0f ? "L" : "R");
}
} // namespace

LevelMeter::LevelMeter (te::LevelMeasurer& m, UiTelemetryHub* hub)
    : levelMeasurer (m), telemetryHub (hub)
{
    if (telemetryHub != nullptr)
        telemetryHub->registerMeter (this);
}

LevelMeter::~LevelMeter()
{
    if (telemetryHub != nullptr)
        telemetryHub->unregisterMeter (this);
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black);
    g.fillRect (bounds);

    g.setColour (juce::Colours::green);
    const float h = bounds.getHeight() * level;
    g.fillRect (bounds.getX(), bounds.getBottom() - h, bounds.getWidth(), h);
}

void LevelMeter::updateFromMeasurer()
{
    const auto [left, right] = levelMeasurer.getLevelCache();
    const float db = juce::jmax (left, right);
    const float newLevel = juce::jlimit (0.0f, 1.0f, juce::Decibels::decibelsToGain (db));

    if (newLevel != level)
    {
        level = newLevel;
        repaint();
    }
}

//==============================================================================

class ChannelStrip::SendControlRow : public juce::Component
{
public:
    SendControlRow (te::Edit& e, te::AudioTrack& at, te::AuxSendPlugin& s)
        : edit (e), audioTrack (at), send (s)
    {
        busLabel.setText (EngineHelpers::auxBusName (send.getBusNumber()), juce::dontSendNotification);
        busLabel.setJustificationType (juce::Justification::centred);

        levelSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        levelSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        levelSlider.setRange (-60.0, 6.0, 0.1);
        levelSlider.setValue (send.getGainDb(), juce::dontSendNotification);
        levelSlider.onValueChange = [this] { send.setGainDb ((float) levelSlider.getValue()); };

        preFaderButton.setClickingTogglesState (true);
        preFaderButton.setToggleState (EngineHelpers::isSendPreFader (audioTrack, send), juce::dontSendNotification);
        preFaderButton.setTooltip ("Pre-fader (on) vs post-fader (off) send tap");
        preFaderButton.onClick = [this]
        {
            EngineHelpers::setSendPreFader (audioTrack, send, preFaderButton.getToggleState());
        };

        muteButton.setClickingTogglesState (true);
        muteButton.setToggleState (send.isMute(), juce::dontSendNotification);
        muteButton.onClick = [this] { send.setMute (muteButton.getToggleState()); };

        addAndMakeVisible (busLabel);
        addAndMakeVisible (levelSlider);
        addAndMakeVisible (preFaderButton);
        addAndMakeVisible (muteButton);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (1);
        busLabel.setBounds (r.removeFromLeft (14));
        muteButton.setBounds (r.removeFromRight (18));
        preFaderButton.setBounds (r.removeFromRight (28));
        levelSlider.setBounds (r);
    }

private:
    te::Edit& edit;
    te::AudioTrack& audioTrack;
    te::AuxSendPlugin& send;
    juce::Label busLabel;
    juce::Slider levelSlider;
    juce::ToggleButton preFaderButton { "Pre" };
    juce::TextButton muteButton { "M" };
};

//==============================================================================

ChannelStrip::ChannelStrip (te::Track& t, UiTelemetryHub* hub)
    : edit (t.edit), track (&t), telemetryHub (hub)
{
    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track))
    {
        volumePlugin = audioTrack->getVolumePlugin();

        if (auto* lmPlugin = audioTrack->getLevelMeterPlugin())
            meter = std::make_unique<LevelMeter> (lmPlugin->measurer, telemetryHub);

        audioTrack->pluginList.state.addListener (this);
    }
    else if (auto* folderTrack = dynamic_cast<te::FolderTrack*> (track))
        volumePlugin = folderTrack->getVolumePlugin();

    track->state.addListener (this);
    initialise();
}

ChannelStrip::ChannelStrip (te::Edit& e, UiTelemetryHub* hub)
    : edit (e), track (nullptr), telemetryHub (hub)
{
    volumePlugin = edit.getMasterVolumePlugin();

    if (auto* lmPlugin = edit.getMasterPluginList().findFirstPluginOfType<te::LevelMeterPlugin>())
        meter = std::make_unique<LevelMeter> (lmPlugin->measurer, telemetryHub);

    initialise();
}

ChannelStrip::~ChannelStrip()
{
    if (volumePlugin != nullptr)
    {
        volumePlugin->volParam->removeListener (this);
        volumePlugin->panParam->removeListener (this);
    }

    if (track != nullptr)
    {
        track->state.removeListener (this);

        if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track))
            audioTrack->pluginList.state.removeListener (this);
    }
}

void ChannelStrip::initialise()
{
    nameLabel.setText (isMasterStrip() ? "Master" : track->getName(), juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);

    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    fader.setRange (0.0, 1.0, 0.001);
    fader.addMouseListener (this, false);

    panSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    panSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    panSlider.setRange (-1.0, 1.0, 0.01);
    panSlider.addMouseListener (this, false);

    if (volumePlugin != nullptr)
    {
        fader.onValueChange = [this]
        {
            volumePlugin->setSliderPos ((float) fader.getValue());
            updateControlLabels();
        };
        panSlider.onValueChange = [this]
        {
            volumePlugin->setPan ((float) panSlider.getValue());
            updateControlLabels();
        };

        volumePlugin->volParam->addListener (this);
        volumePlugin->panParam->addListener (this);
    }

    for (auto* label : { &volumeValueLabel, &panValueLabel })
    {
        label->setJustificationType (juce::Justification::centred);
        label->setFont (juce::FontOptions (10.0f));
        label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
    }

    muteButton.onClick = [this]
    {
        if (track != nullptr)
            track->setMute (! track->isMuted (false));
    };
    soloButton.onClick = [this]
    {
        if (track != nullptr)
            track->setSolo (! track->isSolo (false));
    };

    addSendButton.setTooltip ("Add a send to a return bus (A/B/C)");
    addSendButton.onClick = [this] { showAddSendMenu(); };

    addAndMakeVisible (nameLabel);
    addAndMakeVisible (fader);
    addAndMakeVisible (panSlider);
    addAndMakeVisible (volumeValueLabel);
    addAndMakeVisible (panValueLabel);

    if (! isMasterStrip())
    {
        addAndMakeVisible (muteButton);
        addAndMakeVisible (soloButton);
    }

    if (meter != nullptr)
        addAndMakeVisible (*meter);

    refreshSendControls();
    updateFromModel();
}

void ChannelStrip::showAddSendMenu()
{
    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track))
    {
        juce::PopupMenu menu;
        const auto existing = EngineHelpers::getAllAuxSends (*audioTrack);

        for (int bus = 0; bus < EngineHelpers::maxAuxBuses; ++bus)
        {
            const bool inUse = existing.indexOf (audioTrack->getAuxSendPlugin (bus)) >= 0;
            menu.addItem (bus + 1, "Send " + EngineHelpers::auxBusName (bus), ! inUse);
        }

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [this, audioTrack] (int result)
        {
            if (result <= 0)
                return;

            const int bus = result - 1;
            EngineHelpers::getOrCreateReturnTrack (edit, bus);
            EngineHelpers::addAuxSend (*audioTrack, bus);
            refreshSendControls();
        });
    }
}

void ChannelStrip::refreshSendControls()
{
    sendRows.clear();
    removeChildComponent (&addSendButton);

    if (isMasterStrip() || EngineHelpers::isReturnTrack (*track))
        return;

    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track))
    {
        for (auto* send : EngineHelpers::getAllAuxSends (*audioTrack))
        {
            auto* row = new SendControlRow (edit, *audioTrack, *send);
            sendRows.add (row);
            addAndMakeVisible (row);
        }

        if (sendRows.size() < EngineHelpers::maxAuxBuses)
            addAndMakeVisible (addSendButton);
    }

    resized();
}

void ChannelStrip::updateFromModel()
{
    if (volumePlugin != nullptr)
    {
        fader.setValue (volumePlugin->getSliderPos(), juce::dontSendNotification);
        panSlider.setValue (volumePlugin->getPan(), juce::dontSendNotification);
    }

    if (track != nullptr)
    {
        nameLabel.setText (track->getName(), juce::dontSendNotification);

        const bool muted = track->isMuted (false);
        const bool solo = track->isSolo (false);
        muteButton.setColour (juce::TextButton::buttonColourId,
                              muted ? juce::Colours::orangered : findColour (juce::TextButton::buttonColourId));
        muteButton.setToggleState (muted, juce::dontSendNotification);
        soloButton.setColour (juce::TextButton::buttonColourId,
                              solo ? juce::Colours::gold : findColour (juce::TextButton::buttonColourId));
        soloButton.setToggleState (solo, juce::dontSendNotification);
    }

    updateControlLabels();
}

void ChannelStrip::updateControlLabels()
{
    if (volumePlugin == nullptr)
    {
        volumeValueLabel.setText ({}, juce::dontSendNotification);
        panValueLabel.setText ({}, juce::dontSendNotification);
        return;
    }

    volumeValueLabel.setText (formatVolumeDbText (volumePlugin->getVolumeDb()), juce::dontSendNotification);
    panValueLabel.setText (formatPanText (volumePlugin->getPan()), juce::dontSendNotification);
}

void ChannelStrip::showParameterContextMenu (te::AutomatableParameter& param, juce::Component& target)
{
    juce::PopupMenu menu;
    menu.addItem (1, "MIDI Learn...");
    if (EngineHelpers::isParameterMidiMapped (edit, param))
        menu.addItem (2, "Remove MIDI Mapping");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target),
                        [this, paramPtr = te::AutomatableParameter::Ptr (&param)] (int result) mutable
    {
        if (result == 1)
            EngineHelpers::startParameterMidiLearn (edit, *paramPtr);
        else if (result == 2)
            EngineHelpers::removeParameterMidiMapping (edit, *paramPtr);
    });
}

void ChannelStrip::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu() || volumePlugin == nullptr)
        return;

    if (e.eventComponent == &fader)
        showParameterContextMenu (*volumePlugin->volParam, fader);
    else if (e.eventComponent == &panSlider)
        showParameterContextMenu (*volumePlugin->panParam, panSlider);
}

void ChannelStrip::currentValueChanged (te::AutomatableParameter&)
{
    updateFromModel();
}

void ChannelStrip::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& id)
{
    if (id == te::IDs::mute || id == te::IDs::solo || id == te::IDs::name)
        updateFromModel();
}

void ChannelStrip::paint (juce::Graphics& g)
{
    g.fillAll (isMasterStrip() ? juce::Colour (0xff222240) : juce::Colour (0xff1a1a2e));
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawRect (getLocalBounds());
}

void ChannelStrip::resized()
{
    auto r = getLocalBounds().reduced (4);
    nameLabel.setBounds (r.removeFromTop (18));

    if (! isMasterStrip())
    {
        auto btnRow = r.removeFromBottom (22);
        muteButton.setBounds (btnRow.removeFromLeft (btnRow.getWidth() / 2).reduced (1));
        soloButton.setBounds (btnRow.reduced (1));

        if (addSendButton.isShowing() || ! sendRows.isEmpty())
        {
            auto sendArea = r.removeFromBottom (juce::jmax (20, sendRows.size() * 22 + (addSendButton.isShowing() ? 22 : 0)));
            auto addRow = sendArea.removeFromBottom (addSendButton.isShowing() ? 22 : 0);
            addSendButton.setBounds (addRow.reduced (1));

            for (auto* row : sendRows)
            {
                row->setBounds (sendArea.removeFromTop (22));
            }
        }
    }

    auto panArea = r.removeFromBottom (54);
    panValueLabel.setBounds (panArea.removeFromBottom (14));
    panSlider.setBounds (panArea.reduced (2));

    volumeValueLabel.setBounds (r.removeFromBottom (14));

    if (meter != nullptr)
        meter->setBounds (r.removeFromRight (8));

    fader.setBounds (r);
}

} // namespace skeletonhive
