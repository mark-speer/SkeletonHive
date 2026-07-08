#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

/** Frequency-response curve for te::EqualiserPlugin. */
class EqualiserCurveComponent : public juce::Component,
                                private juce::Timer
{
public:
    explicit EqualiserCurveComponent (te::EqualiserPlugin& eqPlugin);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    te::EqualiserPlugin& eq;
};

} // namespace skeletonhive
