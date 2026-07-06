#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

/** Message-thread audio preview for browser hover audition. */
class PreviewPlayer : private juce::Timer
{
public:
    explicit PreviewPlayer (te::Engine& engine);
    ~PreviewPlayer() override;

    void playFile (const juce::File& file);
    void stop();

    bool isPlaying() const;
    double getPlaybackProgress() const;
    juce::File getCurrentFile() const { return currentFile; }

private:
    void timerCallback() override;
    void prepareReader (const juce::File& file);

    te::Engine& engineRef;
    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transportSource;
    juce::AudioSourcePlayer sourcePlayer;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::File currentFile;
};

} // namespace skeletonhive
