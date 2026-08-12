#pragma once

#include <JuceHeader.h>

namespace skeletonhive
{

/** Helpers for loading and tinting embedded transport SVG icons. */
namespace TransportIcons
{
    std::unique_ptr<juce::Drawable> loadSvg (const void* data, size_t dataSize);

    void tintDrawable (juce::Drawable& drawable, juce::Colour colour);

    std::unique_ptr<juce::Drawable> makeTintedSvg (const void* data, size_t dataSize, juce::Colour colour);

    void setButtonImages (juce::DrawableButton& button,
                          const void* normalData, size_t normalSize,
                          juce::Colour normalColour,
                          const void* onData, size_t onSize,
                          juce::Colour onColour,
                          juce::Component& colourSource);

    void setButtonImage (juce::DrawableButton& button,
                         const void* data, size_t dataSize,
                         juce::Colour colour,
                         juce::Component& colourSource);

    void updatePlayButton (juce::DrawableButton& button,
                           bool isPlaying,
                           juce::Component& colourSource);

    void updateRecordButton (juce::DrawableButton& button,
                             bool isRecording,
                             juce::Component& colourSource);
}

} // namespace skeletonhive
