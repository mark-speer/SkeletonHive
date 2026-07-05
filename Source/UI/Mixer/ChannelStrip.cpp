#include "ChannelStrip.h"
#include "Engine/EngineHelpers.h"

namespace arrange
{

LevelMeter::LevelMeter (te::LevelMeasurer& m)
    : levelMeasurer (m)
{
    startTimerHz (30);
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

void LevelMeter::timerCallback()
{
    const auto [left, right] = levelMeasurer.getLevelCache();
    level = juce::jmax (left, right);
    repaint();
}

//==============================================================================

ChannelStrip::ChannelStrip (te::Track& t)
    : edit (t.edit), track (&t)
{
    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track))
    {
        volumePlugin = audioTrack->getVolumePlugin();

        if (auto* lmPlugin = audioTrack->getLevelMeterPlugin())
            meter = std::make_unique<LevelMeter> (lmPlugin->measurer);
    }

    track->state.addListener (this);
    initialise();
}

ChannelStrip::ChannelStrip (te::Edit& e)
    : edit (e), track (nullptr)
{
    volumePlugin = edit.getMasterVolumePlugin();

    if (auto* lmPlugin = edit.getMasterPluginList().findFirstPluginOfType<te::LevelMeterPlugin>())
        meter = std::make_unique<LevelMeter> (lmPlugin->measurer);

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
        track->state.removeListener (this);
}

void ChannelStrip::initialise()
{
    nameLabel.setText (isMasterStrip() ? "Master" : track->getName(), juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);

    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    fader.setRange (0.0, 1.0, 0.001);

    panSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    panSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    panSlider.setRange (-1.0, 1.0, 0.01);

    if (volumePlugin != nullptr)
    {
        // Sliders work in TE fader-position units, avoiding manual dB/gain conversion
        fader.onValueChange = [this] { volumePlugin->setSliderPos ((float) fader.getValue()); };
        panSlider.onValueChange = [this] { volumePlugin->setPan ((float) panSlider.getValue()); };

        volumePlugin->volParam->addListener (this);
        volumePlugin->panParam->addListener (this);
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

    sendSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    sendSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    sendSlider.setRange (-60.0, 6.0, 0.1);
    sendSlider.setTooltip ("Send level (dB)");
    sendSlider.onValueChange = [this]
    {
        if (auto* send = getSend())
            send->setGainDb ((float) sendSlider.getValue());
    };

    addSendButton.setTooltip ("Add a send to the return bus");
    addSendButton.onClick = [this]
    {
        if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track))
        {
            EngineHelpers::getOrCreateReturnTrack (edit, 0);
            if (auto* send = EngineHelpers::getOrCreateAuxSend (*audioTrack, 0))
                sendPlugin = send;

            refreshSendControls();
        }
    };

    addAndMakeVisible (nameLabel);
    addAndMakeVisible (fader);
    addAndMakeVisible (panSlider);

    if (! isMasterStrip())
    {
        addAndMakeVisible (muteButton);
        addAndMakeVisible (soloButton);
    }

    if (meter != nullptr)
        addAndMakeVisible (*meter);

    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track))
        if (auto* send = audioTrack->getAuxSendPlugin (0))
            sendPlugin = send;

    refreshSendControls();
    updateFromModel();
}

te::AuxSendPlugin* ChannelStrip::getSend() const
{
    return dynamic_cast<te::AuxSendPlugin*> (sendPlugin.get());
}

void ChannelStrip::refreshSendControls()
{
    removeChildComponent (&sendSlider);
    removeChildComponent (&addSendButton);

    if (isMasterStrip())
        return;

    // Return tracks (hosting the AuxReturn) don't get a send on themselves
    const bool isReturnTrack = track->pluginList.findFirstPluginOfType<te::AuxReturnPlugin>() != nullptr;
    if (isReturnTrack)
        return;

    if (auto* send = getSend())
    {
        sendSlider.setValue (send->getGainDb(), juce::dontSendNotification);
        addAndMakeVisible (sendSlider);
    }
    else if (dynamic_cast<te::AudioTrack*> (track) != nullptr)
    {
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

        if (sendSlider.isShowing() || sendSlider.getParentComponent() == this
            || addSendButton.getParentComponent() == this)
        {
            auto sendRow = r.removeFromBottom (20);
            sendSlider.setBounds (sendRow.reduced (1));
            addSendButton.setBounds (sendRow.reduced (1));
        }
    }

    panSlider.setBounds (r.removeFromBottom (40).reduced (2));

    if (meter != nullptr)
        meter->setBounds (r.removeFromRight (8));

    fader.setBounds (r);
}

} // namespace arrange
