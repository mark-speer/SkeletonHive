#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

struct ModMatrixDestination
{
    juce::String label;
    te::AutomatableParameter::Ptr parameter;
};

struct ModMatrixSourceOption
{
    juce::String label;
    te::FourOscPlugin::ModSource source = te::FourOscPlugin::none;
};

/** Shared helpers for te::FourOscPlugin UI and modulation routing. */
class SynthHelpers
{
public:
    static juce::StringArray getOscillatorWaveNames();
    static juce::StringArray getLfoWaveNames();
    static juce::StringArray getVoiceModeNames();
    static juce::StringArray getFilterTypeNames();
    static juce::StringArray getFilterSlopeValues();

    static juce::Array<ModMatrixDestination> getModMatrixDestinations (te::FourOscPlugin& synth);
    static juce::Array<ModMatrixSourceOption> getModMatrixSources();

    static void persistPluginState (te::FourOscPlugin& synth);
};

} // namespace skeletonhive
