#pragma once

#include "TracktionCommon.h"

#include <juce_dsp/juce_dsp.h>

namespace skeletonhive
{

/** 3-band dynamics processor with Linkwitz-Riley crossovers. */
class MultibandDynamicsPlugin : public te::Plugin
{
public:
    explicit MultibandDynamicsPlugin (te::PluginCreationInfo info);
    ~MultibandDynamicsPlugin() override;

    static const char* getPluginName() { return "Multiband Dynamics"; }
    static const char* xmlTypeName;

    juce::String getName() const override;
    juce::String getPluginType() override;
    juce::String getShortName (int) override { return "Multiband"; }
    juce::String getSelectableDescription() override;

    int getNumOutputChannelsGivenInputs (int numInputChannels) override;
    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const te::PluginRenderContext&) override;

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> lowCrossoverValue;
    juce::CachedValue<float> highCrossoverValue;
    juce::CachedValue<float> lowThresholdValue;
    juce::CachedValue<float> midThresholdValue;
    juce::CachedValue<float> highThresholdValue;
    juce::CachedValue<float> lowRatioValue;
    juce::CachedValue<float> midRatioValue;
    juce::CachedValue<float> highRatioValue;
    juce::CachedValue<float> outputValue;

    te::AutomatableParameter::Ptr lowCrossoverParam;
    te::AutomatableParameter::Ptr highCrossoverParam;
    te::AutomatableParameter::Ptr lowThresholdParam;
    te::AutomatableParameter::Ptr midThresholdParam;
    te::AutomatableParameter::Ptr highThresholdParam;
    te::AutomatableParameter::Ptr lowRatioParam;
    te::AutomatableParameter::Ptr midRatioParam;
    te::AutomatableParameter::Ptr highRatioParam;
    te::AutomatableParameter::Ptr outputParam;

private:
    struct BandProcessor
    {
        juce::dsp::LinkwitzRileyFilter<float> lowPass;
        juce::dsp::LinkwitzRileyFilter<float> highPass;
        juce::dsp::Compressor<float> compressor;
        juce::AudioBuffer<float> buffer;
        float envelope = 0.0f;
    };

    juce::dsp::ProcessSpec processSpec;
    BandProcessor lowBand;
    BandProcessor midBand;
    BandProcessor highBand;
    juce::AudioBuffer<float> scratch;

    void updateCrossovers();
    void processBand (BandProcessor& band, juce::AudioBuffer<float>& source, int numChannels, int numSamples,
                      float thresholdDb, float ratio, bool useLowPass, bool useHighPass, float crossoverHz);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultibandDynamicsPlugin)
};

} // namespace skeletonhive
