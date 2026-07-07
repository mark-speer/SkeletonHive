#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

/** Interactive waveform with draggable start/end region handles for te::SamplerPlugin. */
class SamplerWaveformComponent : public juce::Component
{
public:
    SamplerWaveformComponent (te::SamplerPlugin& samplerPlugin);

    void setSoundIndex (int index);
    int getSoundIndex() const { return soundIndex; }

    std::function<void()> onExcerptChanged;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    enum class DragTarget { none, start, end };

    void refreshThumbnail();
    void releaseThumbnail();
    double timeAtX (int x) const;
    int xForTime (double time) const;
    juce::Rectangle<int> waveformArea() const;
    DragTarget hitTestHandle (juce::Point<int> pos) const;
    void applyExcerpt (double startTime, double length);
    void resetExcerptToFullFile();

    te::SamplerPlugin& sampler;
    int soundIndex = -1;
    std::shared_ptr<te::SmartThumbnail> thumbnail;
    te::AudioFile cachedAudioFile;
    DragTarget activeDrag = DragTarget::none;
    double dragStartTime = 0.0;
    double dragLength = 0.0;
};

} // namespace skeletonhive
