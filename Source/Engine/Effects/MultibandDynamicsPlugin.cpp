#include "MultibandDynamicsPlugin.h"

namespace skeletonhive
{

namespace
{

constexpr float defaultAttack = 0.01f;
constexpr float defaultRelease = 0.1f;

float compressSample (float input, float& envelope, float thresholdLin, float ratio, float attack, float release)
{
    const float absIn = std::abs (input);
    const float coeff = absIn > envelope ? attack : release;
    envelope = envelope + coeff * (absIn - envelope);

    if (envelope <= thresholdLin || ratio <= 1.0f)
        return input;

    const float over = envelope / thresholdLin;
    const float gainReduction = std::pow (over, 1.0f - (1.0f / ratio));
    return input / gainReduction;
}

void filterBuffer (juce::dsp::LinkwitzRileyFilter<float>& filter,
                   juce::dsp::LinkwitzRileyFilterType type,
                   float cutoffHz,
                   juce::AudioBuffer<float>& buffer,
                   int channel,
                   int numSamples)
{
    filter.setType (type);
    filter.setCutoffFrequency (cutoffHz);

    float* data = buffer.getWritePointer (channel);
    juce::dsp::AudioBlock<float> block (&data, 1, (size_t) numSamples);
    juce::dsp::ProcessContextReplacing<float> context (block);
    filter.process (context);
}

} // namespace

const char* MultibandDynamicsPlugin::xmlTypeName = "skeletonhiveMultiband";

MultibandDynamicsPlugin::MultibandDynamicsPlugin (te::PluginCreationInfo info)
    : te::Plugin (info)
{
    auto* um = getUndoManager();

    lowCrossoverValue.referTo (state, "lowCrossover", um, 200.0f);
    highCrossoverValue.referTo (state, "highCrossover", um, 2000.0f);
    lowThresholdValue.referTo (state, "lowThreshold", um, -18.0f);
    midThresholdValue.referTo (state, "midThreshold", um, -18.0f);
    highThresholdValue.referTo (state, "highThreshold", um, -18.0f);
    lowRatioValue.referTo (state, "lowRatio", um, 3.0f);
    midRatioValue.referTo (state, "midRatio", um, 3.0f);
    highRatioValue.referTo (state, "highRatio", um, 3.0f);
    outputValue.referTo (state, "output", um, 0.0f);

    lowCrossoverParam = addParam ("lowCrossover", TRANS ("Low Xover"), { 40.0f, 800.0f },
                                  [] (float v) { return juce::String (juce::roundToInt (v)) + " Hz"; },
                                  [] (const juce::String& s) { return s.getFloatValue(); });
    highCrossoverParam = addParam ("highCrossover", TRANS ("High Xover"), { 800.0f, 12000.0f },
                                   [] (float v) { return juce::String (juce::roundToInt (v)) + " Hz"; },
                                   [] (const juce::String& s) { return s.getFloatValue(); });
    lowThresholdParam = addParam ("lowThreshold", TRANS ("Low Threshold"), { -60.0f, 0.0f },
                                  [] (float v) { return juce::Decibels::toString (v); },
                                  [] (const juce::String& s) { return s.getFloatValue(); });
    midThresholdParam = addParam ("midThreshold", TRANS ("Mid Threshold"), { -60.0f, 0.0f },
                                  [] (float v) { return juce::Decibels::toString (v); },
                                  [] (const juce::String& s) { return s.getFloatValue(); });
    highThresholdParam = addParam ("highThreshold", TRANS ("High Threshold"), { -60.0f, 0.0f },
                                   [] (float v) { return juce::Decibels::toString (v); },
                                   [] (const juce::String& s) { return s.getFloatValue(); });
    lowRatioParam = addParam ("lowRatio", TRANS ("Low Ratio"), { 1.0f, 20.0f },
                              [] (float v) { return juce::String (v, 1) + ":1"; },
                              [] (const juce::String& s) { return s.getFloatValue(); });
    midRatioParam = addParam ("midRatio", TRANS ("Mid Ratio"), { 1.0f, 20.0f },
                              [] (float v) { return juce::String (v, 1) + ":1"; },
                              [] (const juce::String& s) { return s.getFloatValue(); });
    highRatioParam = addParam ("highRatio", TRANS ("High Ratio"), { 1.0f, 20.0f },
                               [] (float v) { return juce::String (v, 1) + ":1"; },
                               [] (const juce::String& s) { return s.getFloatValue(); });
    outputParam = addParam ("output", TRANS ("Output"), { -24.0f, 24.0f },
                            [] (float v) { return juce::Decibels::toString (v); },
                            [] (const juce::String& s) { return s.getFloatValue(); });

    lowCrossoverParam->attachToCurrentValue (lowCrossoverValue);
    highCrossoverParam->attachToCurrentValue (highCrossoverValue);
    lowThresholdParam->attachToCurrentValue (lowThresholdValue);
    midThresholdParam->attachToCurrentValue (midThresholdValue);
    highThresholdParam->attachToCurrentValue (highThresholdValue);
    lowRatioParam->attachToCurrentValue (lowRatioValue);
    midRatioParam->attachToCurrentValue (midRatioValue);
    highRatioParam->attachToCurrentValue (highRatioValue);
    outputParam->attachToCurrentValue (outputValue);
}

MultibandDynamicsPlugin::~MultibandDynamicsPlugin()
{
    notifyListenersOfDeletion();

    const te::AutomatableParameter::Ptr params[] =
    {
        lowCrossoverParam, highCrossoverParam,
        lowThresholdParam, midThresholdParam, highThresholdParam,
        lowRatioParam, midRatioParam, highRatioParam, outputParam
    };

    for (auto& param : params)
        if (param != nullptr)
            param->detachFromCurrentValue();
}

juce::String MultibandDynamicsPlugin::getName() const
{
    return TRANS (getPluginName());
}

juce::String MultibandDynamicsPlugin::getPluginType()
{
    return xmlTypeName;
}

juce::String MultibandDynamicsPlugin::getSelectableDescription()
{
    return TRANS ("Multiband Dynamics");
}

int MultibandDynamicsPlugin::getNumOutputChannelsGivenInputs (int numInputChannels)
{
    return juce::jmin (numInputChannels, 2);
}

void MultibandDynamicsPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    processSpec.sampleRate = info.sampleRate;
    processSpec.maximumBlockSize = (juce::uint32) info.blockSizeSamples;
    processSpec.numChannels = 1;

    for (auto* band : { &lowBand, &midBand, &highBand })
    {
        band->lowPass.prepare (processSpec);
        band->highPass.prepare (processSpec);
        band->compressor.prepare (processSpec);
        band->compressor.setAttack (defaultAttack);
        band->compressor.setRelease (defaultRelease);
        band->buffer.setSize (2, info.blockSizeSamples);
        band->envelope = 0.0f;
    }

    scratch.setSize (2, info.blockSizeSamples);
}

void MultibandDynamicsPlugin::deinitialise()
{
    for (auto* band : { &lowBand, &midBand, &highBand })
    {
        band->lowPass.reset();
        band->highPass.reset();
        band->compressor.reset();
        band->buffer.setSize (0, 0);
    }

    scratch.setSize (0, 0);
}

void MultibandDynamicsPlugin::updateCrossovers()
{
}

void MultibandDynamicsPlugin::processBand (BandProcessor& band, juce::AudioBuffer<float>& source, int numChannels,
                                           int numSamples, float thresholdDb, float ratio,
                                           bool useLowPass, bool useHighPass, float crossoverHz)
{
    juce::ignoreUnused (useLowPass, useHighPass, crossoverHz);
    band.buffer.makeCopyOf (source, true);

    const float thresholdLin = juce::Decibels::decibelsToGain (thresholdDb);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = band.buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
            data[i] = compressSample (data[i], band.envelope, thresholdLin, ratio, defaultAttack, defaultRelease);
    }
}

void MultibandDynamicsPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    const int numSamples = fc.bufferNumSamples;
    const int numChannels = juce::jmin (fc.destBuffer->getNumChannels(), 2);
    const float lowHz = lowCrossoverParam->getCurrentValue();
    const float highHz = juce::jmax (highCrossoverParam->getCurrentValue(), lowHz * 1.5f);
    const float outputGain = juce::Decibels::decibelsToGain (outputParam->getCurrentValue());

    scratch.setSize (numChannels, numSamples, false, false, true);

    for (int ch = 0; ch < numChannels; ++ch)
        scratch.copyFrom (ch, 0, *fc.destBuffer, ch, fc.bufferStartSample, numSamples);

    juce::AudioBuffer<float> lowOut (numChannels, numSamples);
    juce::AudioBuffer<float> midOut (numChannels, numSamples);
    juce::AudioBuffer<float> highOut (numChannels, numSamples);

    lowOut.makeCopyOf (scratch, true);
    midOut.makeCopyOf (scratch, true);
    highOut.makeCopyOf (scratch, true);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        filterBuffer (lowBand.lowPass, juce::dsp::LinkwitzRileyFilterType::lowpass, lowHz, lowOut, ch, numSamples);
        filterBuffer (highBand.highPass, juce::dsp::LinkwitzRileyFilterType::highpass, highHz, highOut, ch, numSamples);
        filterBuffer (midBand.highPass, juce::dsp::LinkwitzRileyFilterType::highpass, lowHz, midOut, ch, numSamples);
        filterBuffer (midBand.lowPass, juce::dsp::LinkwitzRileyFilterType::lowpass, highHz, midOut, ch, numSamples);
    }

    processBand (lowBand, lowOut, numChannels, numSamples,
                 lowThresholdParam->getCurrentValue(), lowRatioParam->getCurrentValue(),
                 true, false, lowHz);
    processBand (midBand, midOut, numChannels, numSamples,
                 midThresholdParam->getCurrentValue(), midRatioParam->getCurrentValue(),
                 true, true, lowHz);
    processBand (highBand, highOut, numChannels, numSamples,
                 highThresholdParam->getCurrentValue(), highRatioParam->getCurrentValue(),
                 false, true, highHz);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* out = fc.destBuffer->getWritePointer (ch, fc.bufferStartSample);
        const float* lo = lowOut.getReadPointer (ch);
        const float* mid = midOut.getReadPointer (ch);
        const float* hi = highOut.getReadPointer (ch);

        for (int i = 0; i < numSamples; ++i)
            out[i] = (lo[i] + mid[i] + hi[i]) * outputGain;
    }
}

void MultibandDynamicsPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    tracktion::engine::copyPropertiesToCachedValues (v, lowCrossoverValue, highCrossoverValue,
                                                     lowThresholdValue, midThresholdValue, highThresholdValue,
                                                     lowRatioValue, midRatioValue, highRatioValue, outputValue);

    for (auto* p : getAutomatableParameters())
        if (p != nullptr)
            p->updateFromAttachedValue();
}

} // namespace skeletonhive
