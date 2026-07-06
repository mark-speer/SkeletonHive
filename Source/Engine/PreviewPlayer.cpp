#include "PreviewPlayer.h"

namespace skeletonhive
{

PreviewPlayer::PreviewPlayer (te::Engine& engine)
    : engineRef (engine)
{
    formatManager.registerBasicFormats();
    engineRef.getDeviceManager().deviceManager.addAudioCallback (&sourcePlayer);
    sourcePlayer.setSource (&transportSource);
}

PreviewPlayer::~PreviewPlayer()
{
    stop();
    sourcePlayer.setSource (nullptr);
    engineRef.getDeviceManager().deviceManager.removeAudioCallback (&sourcePlayer);
}

void PreviewPlayer::prepareReader (const juce::File& file)
{
    transportSource.stop();
    transportSource.setSource (nullptr);
    readerSource.reset();

    if (auto* reader = formatManager.createReaderFor (file))
    {
        readerSource = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
        transportSource.setSource (readerSource.get(), 0, nullptr, reader->sampleRate);
    }
}

void PreviewPlayer::playFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    if (currentFile == file && transportSource.isPlaying())
        return;

    currentFile = file;
    prepareReader (file);

    if (readerSource == nullptr)
        return;

    transportSource.setPosition (0.0);
    transportSource.start();
    startTimerHz (30);
}

void PreviewPlayer::stop()
{
    stopTimer();
    transportSource.stop();
    transportSource.setSource (nullptr);
    readerSource.reset();
    currentFile = {};
}

bool PreviewPlayer::isPlaying() const
{
    return transportSource.isPlaying();
}

double PreviewPlayer::getPlaybackProgress() const
{
    const auto length = transportSource.getLengthInSeconds();
    if (length <= 0.0)
        return 0.0;

    return transportSource.getCurrentPosition() / length;
}

void PreviewPlayer::timerCallback()
{
    if (! transportSource.isPlaying())
        stop();
}

} // namespace skeletonhive
