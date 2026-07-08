#include "SaturationPlugin.h"

namespace skeletonhive
{

const char* SaturationPlugin::xmlTypeName = "skeletonhiveSaturation";

SaturationPlugin::SaturationPlugin (te::PluginCreationInfo info)
    : te::Plugin (info)
{
    shaper.functionToUse = [] (float x)
    {
        return std::tanh (x);
    };

    auto* um = getUndoManager();

    driveValue.referTo (state, "drive", um, 6.0f);
    mixValue.referTo (state, "mix", um, 1.0f);
    outputValue.referTo (state, "output", um, 0.0f);

    driveParam = addParam ("drive", TRANS ("Drive"), { 0.0f, 24.0f },
                           [] (float value) { return juce::Decibels::toString (value); },
                           [] (const juce::String& s) { return s.getFloatValue(); });
    mixParam = addParam ("mix", TRANS ("Mix"), { 0.0f, 1.0f },
                         [] (float value) { return juce::String (juce::roundToInt (value * 100.0f)) + "%"; },
                         [] (const juce::String& s) { return s.getFloatValue() / 100.0f; });
    outputParam = addParam ("output", TRANS ("Output"), { -24.0f, 24.0f },
                            [] (float value) { return juce::Decibels::toString (value); },
                            [] (const juce::String& s) { return s.getFloatValue(); });

    driveParam->attachToCurrentValue (driveValue);
    mixParam->attachToCurrentValue (mixValue);
    outputParam->attachToCurrentValue (outputValue);
}

SaturationPlugin::~SaturationPlugin()
{
    notifyListenersOfDeletion();

    if (driveParam != nullptr) driveParam->detachFromCurrentValue();
    if (mixParam != nullptr) mixParam->detachFromCurrentValue();
    if (outputParam != nullptr) outputParam->detachFromCurrentValue();
}

juce::String SaturationPlugin::getName() const
{
    return TRANS (getPluginName());
}

juce::String SaturationPlugin::getPluginType()
{
    return xmlTypeName;
}

juce::String SaturationPlugin::getSelectableDescription()
{
    return TRANS ("Saturation");
}

int SaturationPlugin::getNumOutputChannelsGivenInputs (int numInputChannels)
{
    return juce::jmin (numInputChannels, 2);
}

void SaturationPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    processSpec.sampleRate = info.sampleRate;
    processSpec.maximumBlockSize = (juce::uint32) info.blockSizeSamples;
    processSpec.numChannels = 2;

    shaper.prepare (processSpec);
    dryBuffer.setSize (2, info.blockSizeSamples);
}

void SaturationPlugin::deinitialise()
{
    shaper.reset();
    dryBuffer.setSize (0, 0);
}

void SaturationPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    const int numSamples = fc.bufferNumSamples;
    const int numChannels = juce::jmin (fc.destBuffer->getNumChannels(), 2);
    const float driveDb = driveParam != nullptr ? driveParam->getCurrentValue() : driveValue.get();
    const float mix = mixParam != nullptr ? mixParam->getCurrentValue() : mixValue.get();
    const float outputDb = outputParam != nullptr ? outputParam->getCurrentValue() : outputValue.get();
    const float driveGain = juce::Decibels::decibelsToGain (driveDb);
    const float outputGain = juce::Decibels::decibelsToGain (outputDb);
    const float dryMix = 1.0f - mix;

    dryBuffer.setSize (numChannels, numSamples, false, false, true);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* in = fc.destBuffer->getReadPointer (ch, fc.bufferStartSample);
        float* dry = dryBuffer.getWritePointer (ch);
        float* wet = fc.destBuffer->getWritePointer (ch, fc.bufferStartSample);

        juce::FloatVectorOperations::copy (dry, in, numSamples);

        for (int i = 0; i < numSamples; ++i)
            wet[i] = shaper.processSample (in[i] * driveGain);

        for (int i = 0; i < numSamples; ++i)
            wet[i] = (wet[i] * mix + dry[i] * dryMix) * outputGain;
    }
}

void SaturationPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    tracktion::engine::copyPropertiesToCachedValues (v, driveValue, mixValue, outputValue);

    for (auto* p : getAutomatableParameters())
        if (p != nullptr)
            p->updateFromAttachedValue();
}

} // namespace skeletonhive
