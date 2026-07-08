#pragma once

#include "TracktionCommon.h"

#include <juce_dsp/juce_dsp.h>

namespace skeletonhive
{

/** juce::dsp soft-clip saturation device registered as a TE built-in plugin. */
class SaturationPlugin : public te::Plugin
{
public:
    explicit SaturationPlugin (te::PluginCreationInfo info);
    ~SaturationPlugin() override;

    static const char* getPluginName() { return "Saturation"; }
    static const char* xmlTypeName;

    juce::String getName() const override;
    juce::String getPluginType() override;
    juce::String getShortName (int) override { return "Saturate"; }
    juce::String getSelectableDescription() override;

    int getNumOutputChannelsGivenInputs (int numInputChannels) override;
    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const te::PluginRenderContext&) override;

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> driveValue;
    juce::CachedValue<float> mixValue;
    juce::CachedValue<float> outputValue;

    te::AutomatableParameter::Ptr driveParam;
    te::AutomatableParameter::Ptr mixParam;
    te::AutomatableParameter::Ptr outputParam;

private:
    juce::dsp::WaveShaper<float> shaper;
    juce::dsp::ProcessSpec processSpec;
    juce::AudioBuffer<float> dryBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaturationPlugin)
};

} // namespace skeletonhive
