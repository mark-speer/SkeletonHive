#include "SynthHelpers.h"

namespace skeletonhive
{

juce::StringArray SynthHelpers::getOscillatorWaveNames()
{
    return { "None", "Sine", "Square", "Saw", "Triangle", "Noise" };
}

juce::StringArray SynthHelpers::getLfoWaveNames()
{
    return { "None", "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random" };
}

juce::StringArray SynthHelpers::getVoiceModeNames()
{
    return { "Mono", "Legato", "Poly" };
}

juce::StringArray SynthHelpers::getFilterTypeNames()
{
    return { "Off", "Low Pass", "High Pass", "Band Pass", "Notch" };
}

juce::StringArray SynthHelpers::getFilterSlopeValues()
{
    return { "12 dB", "24 dB" };
}

juce::Array<ModMatrixDestination> SynthHelpers::getModMatrixDestinations (te::FourOscPlugin& synth)
{
    juce::Array<ModMatrixDestination> destinations;

    auto add = [&] (const juce::String& label, const te::AutomatableParameter::Ptr& param)
    {
        if (param != nullptr)
            destinations.add ({ label, param });
    };

    add ("Filter Freq", synth.filterFreq);
    add ("Filter Resonance", synth.filterResonance);
    add ("Filter Amount", synth.filterAmount);
    add ("Amp Attack", synth.ampAttack);
    add ("Amp Decay", synth.ampDecay);
    add ("Amp Sustain", synth.ampSustain);
    add ("Amp Release", synth.ampRelease);

    for (int i = 0; i < synth.oscParams.size(); ++i)
    {
        if (auto* osc = synth.oscParams[i])
        {
            const auto oscNum = i + 1;
            add ("Osc " + juce::String (oscNum) + " Level", osc->level);
            add ("Osc " + juce::String (oscNum) + " Tune", osc->tune);
            add ("Osc " + juce::String (oscNum) + " Fine", osc->fineTune);
            add ("Osc " + juce::String (oscNum) + " Pulse", osc->pulseWidth);
            add ("Osc " + juce::String (oscNum) + " Detune", osc->detune);
            add ("Osc " + juce::String (oscNum) + " Spread", osc->spread);
            add ("Osc " + juce::String (oscNum) + " Pan", osc->pan);
        }
    }

    return destinations;
}

juce::Array<ModMatrixSourceOption> SynthHelpers::getModMatrixSources()
{
    juce::Array<ModMatrixSourceOption> sources;
    sources.add ({ "LFO 1", te::FourOscPlugin::lfo1 });
    sources.add ({ "LFO 2", te::FourOscPlugin::lfo2 });
    sources.add ({ "Env 1", te::FourOscPlugin::env1 });
    sources.add ({ "Env 2", te::FourOscPlugin::env2 });
    sources.add ({ "Velocity", te::FourOscPlugin::midiVelocity });
    sources.add ({ "Note Number", te::FourOscPlugin::midiNoteNum });
    return sources;
}

void SynthHelpers::persistPluginState (te::FourOscPlugin& synth)
{
    synth.edit.flushPluginStateIfNeeded (synth);
}

} // namespace skeletonhive
