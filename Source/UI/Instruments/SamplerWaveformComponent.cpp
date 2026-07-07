#include "SamplerWaveformComponent.h"

#include "Engine/SamplerHelpers.h"

namespace skeletonhive
{

namespace
{
constexpr int handleWidth = 6;
constexpr int handleHitPadding = 4;
} // namespace

SamplerWaveformComponent::SamplerWaveformComponent (te::SamplerPlugin& samplerPlugin)
    : sampler (samplerPlugin),
      cachedAudioFile (samplerPlugin.edit.engine)
{
}

void SamplerWaveformComponent::setSoundIndex (int index)
{
    if (soundIndex == index)
        return;

    soundIndex = index;
    refreshThumbnail();
    repaint();
}

void SamplerWaveformComponent::refreshThumbnail()
{
    releaseThumbnail();

    if (! juce::isPositiveAndBelow (soundIndex, sampler.getNumSounds()))
        return;

    cachedAudioFile = sampler.getSoundFile (soundIndex);

    if (! cachedAudioFile.isValid())
        return;

    thumbnail = std::make_shared<te::SmartThumbnail> (sampler.edit.engine, cachedAudioFile, *this, &sampler.edit);
}

void SamplerWaveformComponent::releaseThumbnail()
{
    thumbnail.reset();
    cachedAudioFile = te::AudioFile (sampler.edit.engine);
}

juce::Rectangle<int> SamplerWaveformComponent::waveformArea() const
{
    return getLocalBounds().reduced (4, 6);
}

double SamplerWaveformComponent::timeAtX (int x) const
{
    const auto area = waveformArea();

    if (area.isEmpty() || ! cachedAudioFile.isValid())
        return 0.0;

    const double fileLength = juce::jmax (0.001, cachedAudioFile.getLength());
    const double rel = juce::jlimit (0.0, 1.0, double (x - area.getX()) / double (juce::jmax (1, area.getWidth())));
    return rel * fileLength;
}

int SamplerWaveformComponent::xForTime (double time) const
{
    const auto area = waveformArea();

    if (area.isEmpty() || ! cachedAudioFile.isValid())
        return area.getX();

    const double fileLength = juce::jmax (0.001, cachedAudioFile.getLength());
    const double rel = juce::jlimit (0.0, 1.0, time / fileLength);
    return area.getX() + juce::roundToInt (rel * area.getWidth());
}

SamplerWaveformComponent::DragTarget SamplerWaveformComponent::hitTestHandle (juce::Point<int> pos) const
{
    if (! juce::isPositiveAndBelow (soundIndex, sampler.getNumSounds()) || ! cachedAudioFile.isValid())
        return DragTarget::none;

    const double startTime = sampler.getSoundStartTime (soundIndex);
    const double effectiveLength = SamplerHelpers::getEffectiveLength (sampler, soundIndex);
    const int startX = xForTime (startTime);
    const int endX = xForTime (startTime + effectiveLength);

    const auto startHandle = juce::Rectangle<int> (startX - handleHitPadding, waveformArea().getY(),
                                                   handleWidth + handleHitPadding * 2, waveformArea().getHeight());
    const auto endHandle = juce::Rectangle<int> (endX - handleWidth - handleHitPadding, waveformArea().getY(),
                                                 handleWidth + handleHitPadding * 2, waveformArea().getHeight());

    if (startHandle.contains (pos))
        return DragTarget::start;

    if (endHandle.contains (pos))
        return DragTarget::end;

    return DragTarget::none;
}

void SamplerWaveformComponent::applyExcerpt (double startTime, double length)
{
    if (! juce::isPositiveAndBelow (soundIndex, sampler.getNumSounds()))
        return;

    const auto excerpt = SamplerHelpers::clampExcerpt (cachedAudioFile, startTime, length);
    sampler.setSoundExcerpt (soundIndex, excerpt.startTime, excerpt.length);

    if (onExcerptChanged)
        onExcerptChanged();

    repaint();
}

void SamplerWaveformComponent::resetExcerptToFullFile()
{
    applyExcerpt (0.0, 0.0);
}

void SamplerWaveformComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillRoundedRectangle (bounds, 4.0f);

    const auto area = waveformArea();

    if (area.isEmpty())
        return;

    if (thumbnail != nullptr && cachedAudioFile.isValid())
    {
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        const te::TimeRange viewRange { 0s, te::TimeDuration::fromSeconds (cachedAudioFile.getLength()) };
        thumbnail->drawChannels (g, area, viewRange, 1.0f);
    }
    else
    {
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("No sample loaded", area, juce::Justification::centred, true);
        return;
    }

    if (! juce::isPositiveAndBelow (soundIndex, sampler.getNumSounds()))
        return;

    const double startTime = sampler.getSoundStartTime (soundIndex);
    const double effectiveLength = SamplerHelpers::getEffectiveLength (sampler, soundIndex);
    const int startX = xForTime (startTime);
    const int endX = xForTime (startTime + effectiveLength);

    g.setColour (juce::Colour (0xff5a189a).withAlpha (0.25f));
    g.fillRect (startX, area.getY(), juce::jmax (1, endX - startX), area.getHeight());

    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.drawVerticalLine (startX, (float) area.getY(), (float) area.getBottom());
    g.drawVerticalLine (endX, (float) area.getY(), (float) area.getBottom());

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawVerticalLine (startX, (float) area.getY(), (float) area.getBottom());

    g.setColour (juce::Colours::white);
    g.fillRect (startX - handleWidth / 2, area.getY(), handleWidth, area.getHeight());
    g.fillRect (endX - handleWidth / 2, area.getY(), handleWidth, area.getHeight());

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("Start", startX + 4, area.getY() + 2, 40, 14, juce::Justification::centredLeft, true);
    g.drawText ("End", endX - 44, area.getY() + 2, 40, 14, juce::Justification::centredRight, true);
}

void SamplerWaveformComponent::mouseDown (const juce::MouseEvent& e)
{
    activeDrag = hitTestHandle (e.getPosition());

    if (activeDrag != DragTarget::none)
    {
        dragStartTime = sampler.getSoundStartTime (soundIndex);
        dragLength = sampler.getSoundLength (soundIndex);
    }
}

void SamplerWaveformComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (activeDrag == DragTarget::none || ! cachedAudioFile.isValid())
        return;

    const double time = timeAtX (e.x);
    const double minLength = 32.0 / juce::jmax (1.0, cachedAudioFile.getSampleRate());
    const double fileLength = cachedAudioFile.getLength();

    if (activeDrag == DragTarget::start)
    {
        const double effectiveLength = dragLength > 0.0 ? dragLength : juce::jmax (minLength, fileLength - dragStartTime);
        const double endTime = dragStartTime + effectiveLength;
        const double newStart = juce::jlimit (0.0, endTime - minLength, time);
        applyExcerpt (newStart, endTime - newStart);
        return;
    }

    if (activeDrag == DragTarget::end)
    {
        const double newEnd = juce::jlimit (dragStartTime + minLength, fileLength, time);
        applyExcerpt (dragStartTime, newEnd - dragStartTime);
    }
}

void SamplerWaveformComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (hitTestHandle (e.getPosition()) != DragTarget::none)
        resetExcerptToFullFile();
}

} // namespace skeletonhive
