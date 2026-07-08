#include "ArrangementClipVisuals.h"
#include "TimelineLOD.h"
#include "Engine/EngineHelpers.h"

namespace skeletonhive
{

float clipCornerRadius (TimelineClipDetailLevel detail, int clipWidthPx)
{
    if (detail == TimelineClipDetailLevel::Summary)
        return 2.0f;

    return clipWidthPx >= 48 ? 6.0f : 4.0f;
}

void paintClipStateOverlay (juce::Graphics& g, EditViewState& editViewState, te::Clip& clip,
                            juce::Rectangle<int> bounds, float cornerRadius)
{
    if (bounds.isEmpty())
        return;

    const auto theme = AppLookAndFeel::getCurrentTheme();
    const auto boundsF = bounds.toFloat();

    if (clip.isMuted())
    {
        g.setColour (AppColours::clipMutedOverlay (theme));
        g.fillRoundedRectangle (boundsF, cornerRadius);
        auto badgeArea = bounds.withWidth (bounds.getWidth()).withHeight (bounds.getHeight());
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.setFont (juce::FontOptions (8.0f, juce::Font::bold));
        g.drawText ("M", badgeArea.removeFromRight (14).removeFromTop (12),
                    juce::Justification::centred, false);
    }
    else if (auto* track = clip.getTrack())
    {
        if (track->isMuted (false))
        {
            g.setColour (AppColours::clipMutedOverlay (theme).withAlpha (0.25f));
            g.fillRoundedRectangle (boundsF, cornerRadius);
        }
    }

    if (clip.disabled.get())
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (boundsF, cornerRadius);
    }

    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (clip.getTrack()))
    {
        if (audioTrack->isFrozen (te::Track::anyFreeze))
        {
            g.setColour (AppColours::accentFrozen (theme).withAlpha (0.18f));
            g.fillRoundedRectangle (boundsF, cornerRadius);
        }
    }

    auto& transport = editViewState.edit.getTransport();
    if (transport.isRecording() && clip.getTrack() != nullptr)
    {
        if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (clip.getTrack()))
        {
            if (EngineHelpers::isTrackArmed (*audioTrack))
            {
                const bool pulse = (juce::Time::getMillisecondCounter() / 500) % 2 == 0;
                g.setColour (juce::Colour (0xffef476f).withAlpha (pulse ? 0.85f : 0.35f));
                g.drawRoundedRectangle (boundsF.reduced (0.5f), cornerRadius, 1.5f);
            }
        }
    }

    if (EngineHelpers::getClipGroup (clip).isNotEmpty() || EngineHelpers::getClipOuterGroup (clip).isNotEmpty())
    {
        const auto groupColour = EngineHelpers::getClipGroupColour (clip);
        g.setColour (groupColour.withAlpha (0.9f));
        g.fillRect (bounds.getX(), bounds.getY(), bounds.getWidth(), 2);
    }
}

} // namespace skeletonhive
