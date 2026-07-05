#include "MixerPanel.h"

namespace arrange
{

MixerPanel::MixerPanel (te::Edit& e)
    : edit (e)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&stripContainer, false);
    viewport.setScrollBarsShown (false, true);

    edit.state.addListener (this);
    rebuild();
}

MixerPanel::~MixerPanel()
{
    edit.state.removeListener (this);
}

void MixerPanel::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& child)
{
    if (te::TrackList::isTrack (child))
        triggerAsyncUpdate();
}

void MixerPanel::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& child, int)
{
    if (te::TrackList::isTrack (child))
        triggerAsyncUpdate();
}

void MixerPanel::rebuild()
{
    strips.clear();
    masterStrip = nullptr;
    stripContainer.removeAllChildren();

    const int stripWidth = 80;
    const int stripHeight = juce::jmax (200, getHeight());
    int x = 0;

    for (auto track : te::getAllTracks (edit))
    {
        if (track->isMarkerTrack() || track->isTempoTrack() || track->isChordTrack()
            || track->isMasterTrack() || track->isArrangerTrack())
            continue;

        auto* strip = new ChannelStrip (*track);
        strip->setBounds (x, 0, stripWidth, stripHeight);
        stripContainer.addAndMakeVisible (strip);
        strips.add (strip);
        x += stripWidth;
    }

    x += 8;   // gap before the master bus
    masterStrip = std::make_unique<ChannelStrip> (edit);
    masterStrip->setBounds (x, 0, stripWidth + 10, stripHeight);
    stripContainer.addAndMakeVisible (*masterStrip);
    x += stripWidth + 10;

    stripContainer.setSize (x, stripHeight);
}

void MixerPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0f0f23));
}

void MixerPanel::resized()
{
    viewport.setBounds (getLocalBounds());
}

} // namespace arrange
