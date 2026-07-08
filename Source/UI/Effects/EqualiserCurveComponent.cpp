#include "EqualiserCurveComponent.h"

#include "Engine/Effects/EqualiserHelpers.h"

namespace skeletonhive
{

EqualiserCurveComponent::EqualiserCurveComponent (te::EqualiserPlugin& eqPlugin)
    : eq (eqPlugin)
{
    startTimerHz (15);
}

void EqualiserCurveComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    juce::Path curve;
    const int numPoints = juce::jmax (64, getWidth());
    const float minFreq = 20.0f;
    const float maxFreq = 20000.0f;

    for (int i = 0; i < numPoints; ++i)
    {
        const float t = (float) i / (float) (numPoints - 1);
        const float freq = minFreq * std::pow (maxFreq / minFreq, t);
        const float gainDb = EqualiserHelpers::getGainAtFrequency (eq, freq);
        const float x = bounds.getX() + t * bounds.getWidth();
        const float y = juce::jmap (gainDb, 24.0f, -24.0f, bounds.getY(), bounds.getBottom());

        if (i == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }

    g.setColour (juce::Colours::aqua.withAlpha (0.9f));
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    g.setColour (juce::Colours::white.withAlpha (0.25f));
    const float zeroY = juce::jmap (0.0f, 24.0f, -24.0f, bounds.getY(), bounds.getBottom());
    g.drawHorizontalLine ((int) zeroY, bounds.getX(), bounds.getRight());
}

void EqualiserCurveComponent::resized()
{
}

void EqualiserCurveComponent::timerCallback()
{
    repaint();
}

} // namespace skeletonhive
