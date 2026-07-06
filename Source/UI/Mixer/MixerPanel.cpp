#include "MixerPanel.h"

namespace skeletonhive
{

MixerPanel::MixerPanel (te::Edit& e, UiTelemetryHub* hub)
    : edit (e), telemetryHub (hub)
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

bool MixerPanel::isMixerTrack (const juce::ValueTree& child)
{
    return te::TrackList::isTrack (child);
}

void MixerPanel::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& child)
{
    if (isMixerTrack (child))
        markAndUpdate (rebuildTrackList);
}

void MixerPanel::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& child, int)
{
    if (isMixerTrack (child))
        markAndUpdate (rebuildTrackList);
}

void MixerPanel::valueTreeChildOrderChanged (juce::ValueTree&, int, int)
{
    markAndUpdate (relayoutStrips);
}

void MixerPanel::handleAsyncUpdate()
{
    if (compareAndReset (rebuildTrackList))
        syncTrackStrips();
    else if (compareAndReset (relayoutStrips))
        layoutStrips();
}

juce::Array<te::Track*> MixerPanel::collectMixerTracks() const
{
    juce::Array<te::Track*> tracks;

    for (auto track : te::getAllTracks (edit))
    {
        if (track->isMarkerTrack() || track->isTempoTrack() || track->isChordTrack()
            || track->isMasterTrack() || track->isArrangerTrack())
            continue;

        tracks.add (track);
    }

    return tracks;
}

ChannelStrip* MixerPanel::findStripForTrack (te::Track& track) const
{
    for (auto* strip : strips)
        if (strip->getTrack() == &track)
            return strip;

    return nullptr;
}

void MixerPanel::syncTrackStrips()
{
    const auto desiredTracks = collectMixerTracks();
    juce::OwnedArray<ChannelStrip> nextStrips;

    for (auto* track : desiredTracks)
    {
        if (auto* existing = findStripForTrack (*track))
        {
            nextStrips.add (existing);
            strips.removeObject (existing, false);
        }
        else
        {
            nextStrips.add (new ChannelStrip (*track, telemetryHub));
        }
    }

    strips.clear();
    strips.swapWith (nextStrips);

    if (masterStrip == nullptr)
        masterStrip = std::make_unique<ChannelStrip> (edit, telemetryHub);

    layoutStrips();
}

void MixerPanel::layoutStrips()
{
    const int stripWidth = 80;
    const int stripHeight = juce::jmax (200, getHeight());
    int x = 0;

    stripContainer.removeAllChildren();

    for (auto* strip : strips)
    {
        strip->setBounds (x, 0, stripWidth, stripHeight);
        stripContainer.addAndMakeVisible (strip);
        x += stripWidth;
    }

    x += 8;

    if (masterStrip != nullptr)
    {
        masterStrip->setBounds (x, 0, stripWidth + 10, stripHeight);
        stripContainer.addAndMakeVisible (*masterStrip);
        x += stripWidth + 10;
    }

    stripContainer.setSize (x, stripHeight);
}

void MixerPanel::rebuild()
{
    strips.clear();
    masterStrip = nullptr;
    syncTrackStrips();
}

void MixerPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0f0f23));
}

void MixerPanel::resized()
{
    viewport.setBounds (getLocalBounds());
    layoutStrips();
}

} // namespace skeletonhive
