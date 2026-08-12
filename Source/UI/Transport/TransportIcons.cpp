#include "TransportIcons.h"
#include "BinaryData.h"

namespace skeletonhive
{
namespace TransportIcons
{

namespace
{
// Pinned JUCE Colour(uint32) is not constexpr; keep a runtime constant.
const juce::Colour kSvgSourceBlack { 0xff000000 };

juce::Colour buttonIconColour (juce::Component& source)
{
    return source.findColour (juce::TextButton::textColourOffId);
}

juce::Colour buttonIconOnColour (juce::Component& source, juce::Colour accent)
{
    return accent.isTransparent() ? source.findColour (juce::TextButton::textColourOnId)
                                  : accent;
}
} // namespace

std::unique_ptr<juce::Drawable> loadSvg (const void* data, size_t dataSize)
{
    if (data == nullptr || dataSize == 0)
        return {};

    const auto xml = juce::parseXML (juce::String::createStringFromData (data, (int) dataSize));

    if (xml == nullptr)
        return {};

    return juce::Drawable::createFromSVG (*xml);
}

void tintDrawable (juce::Drawable& drawable, juce::Colour colour)
{
    drawable.replaceColour (kSvgSourceBlack, colour);
}

std::unique_ptr<juce::Drawable> makeTintedSvg (const void* data, size_t dataSize, juce::Colour colour)
{
    auto drawable = loadSvg (data, dataSize);

    if (drawable != nullptr)
        tintDrawable (*drawable, colour);

    return drawable;
}

void setButtonImage (juce::DrawableButton& button,
                     const void* data, size_t dataSize,
                     juce::Colour colour,
                     juce::Component& colourSource)
{
    const auto resolvedColour = colour.isTransparent() ? buttonIconColour (colourSource) : colour;
    auto normal = makeTintedSvg (data, dataSize, resolvedColour);
    auto over = makeTintedSvg (data, dataSize, resolvedColour.brighter (0.15f));
    auto down = makeTintedSvg (data, dataSize, resolvedColour.brighter (0.3f));

    button.setImages (normal.get(), over.get(), down.get());
}

void setButtonImages (juce::DrawableButton& button,
                      const void* normalData, size_t normalSize,
                      juce::Colour normalColour,
                      const void* onData, size_t onSize,
                      juce::Colour onColour,
                      juce::Component& colourSource)
{
    const auto resolvedNormal = normalColour.isTransparent() ? buttonIconColour (colourSource) : normalColour;
    const auto resolvedOn = buttonIconOnColour (colourSource, onColour);

    auto normal = makeTintedSvg (normalData, normalSize, resolvedNormal);
    auto over = makeTintedSvg (normalData, normalSize, resolvedNormal.brighter (0.15f));
    auto down = makeTintedSvg (normalData, normalSize, resolvedNormal.brighter (0.3f));
    auto on = makeTintedSvg (onData, onSize, resolvedOn);

    button.setImages (normal.get(), over.get(), down.get(), nullptr, on.get());
}

void updatePlayButton (juce::DrawableButton& button, bool isPlaying, juce::Component& colourSource)
{
    if (isPlaying)
        setButtonImage (button, BinaryData::pause_svg, BinaryData::pause_svgSize, {}, colourSource);
    else
        setButtonImage (button, BinaryData::play_svg, BinaryData::play_svgSize, {}, colourSource);
}

void updateRecordButton (juce::DrawableButton& button, bool isRecording, juce::Component& colourSource)
{
    if (isRecording)
    {
        setButtonImage (button,
                        BinaryData::record_active_svg,
                        BinaryData::record_active_svgSize,
                        juce::Colour (0xffef476f),
                        colourSource);
    }
    else
    {
        setButtonImage (button,
                        BinaryData::record_svg,
                        BinaryData::record_svgSize,
                        juce::Colour (0xffef476f),
                        colourSource);
    }
}

} // namespace TransportIcons
} // namespace skeletonhive
