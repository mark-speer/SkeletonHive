#include "DrumRackHelpers.h"

#include "EngineHelpers.h"

namespace skeletonhive
{

namespace
{

const juce::Identifier& drumRackId()
{
    static const juce::Identifier id { "skeletonhiveDrumRack" };
    return id;
}

const juce::Identifier& padCountId()
{
    static const juce::Identifier id { "skeletonhivePadCount" };
    return id;
}

juce::String padDisplayName (int padIndex)
{
    return "Pad " + juce::String (padIndex + 1).paddedLeft ('0', 2);
}

float macroValueToGainDb (float normalised)
{
    return juce::jmap (normalised, 0.0f, 1.0f, -48.0f, 6.0f);
}

int macroValueToTuneSemitones (float normalised)
{
    return juce::roundToInt (juce::jmap (normalised, 0.0f, 1.0f, -12.0f, 12.0f));
}

class PadMacroBinding : private te::AutomatableParameter::Listener
{
public:
    enum class Kind { volume, tune };

    PadMacroBinding (te::SamplerPlugin& samplerPlugin, int soundIndex, int padMidiNote,
                     te::AutomatableParameter& macro, Kind bindingKind)
        : sampler (samplerPlugin),
          soundIdx (soundIndex),
          padNote (padMidiNote),
          macroParam (&macro),
          kind (bindingKind)
    {
        macroParam->addListener (this);
        applyMacroValue (macroParam->getCurrentNormalisedValue());
    }

    ~PadMacroBinding() override
    {
        if (macroParam != nullptr)
            macroParam->removeListener (this);
    }

private:
    void curveHasChanged (te::AutomatableParameter&) override {}

    void currentValueChanged (te::AutomatableParameter&) override
    {
        if (macroParam != nullptr)
            applyMacroValue (macroParam->getCurrentNormalisedValue());
    }

    void applyMacroValue (float normalised)
    {
        if (soundIdx < 0 || soundIdx >= sampler.getNumSounds())
            return;

        if (kind == Kind::volume)
        {
            const float pan = sampler.getSoundPan (soundIdx);
            sampler.setSoundGains (soundIdx, macroValueToGainDb (normalised), pan);
            return;
        }

        const int keyNote = juce::jlimit (0, 127, padNote + macroValueToTuneSemitones (normalised));
        sampler.setSoundParams (soundIdx, keyNote, padNote, padNote);
    }

    te::SamplerPlugin& sampler;
    int soundIdx;
    int padNote;
    te::AutomatableParameter::Ptr macroParam;
    Kind kind;
};

struct DrumRackBindingSet
{
    te::RackType::Ptr rackType;
    juce::OwnedArray<PadMacroBinding> bindings;
};

juce::Array<DrumRackBindingSet*>& activeBindingSets()
{
    static juce::Array<DrumRackBindingSet*> sets;
    return sets;
}

bool hasBindingsFor (const te::RackType& rackType)
{
    for (auto* set : activeBindingSets())
        if (set != nullptr && set->rackType.get() == &rackType)
            return true;

    return false;
}

void registerBindings (te::RackType& rackType, juce::OwnedArray<PadMacroBinding>& bindings)
{
    if (bindings.isEmpty())
        return;

    auto* set = new DrumRackBindingSet();
    set->rackType = &rackType;
    set->bindings.swapWith (bindings);
    activeBindingSets().add (set);
}

juce::Array<te::SamplerPlugin*> getPadSamplers (te::RackInstance& rack)
{
    juce::Array<te::SamplerPlugin*> samplers;

    if (rack.type == nullptr)
        return samplers;

    for (auto* plugin : rack.type->getPlugins())
        if (auto* sampler = dynamic_cast<te::SamplerPlugin*> (plugin))
            samplers.add (sampler);

    return samplers;
}

void bindExistingPadMacros (te::RackType& rackType, te::SamplerPlugin& sampler, int padIndex,
                            juce::OwnedArray<PadMacroBinding>& bindings)
{
    const int padNote = DrumRackHelpers::midiNoteForPad (padIndex);
    const juce::String volPrefix = padDisplayName (padIndex) + " Vol";
    const juce::String tunePrefix = padDisplayName (padIndex) + " Tune";

    te::AutomatableParameter* volMacro = nullptr;
    te::AutomatableParameter* tuneMacro = nullptr;

    for (auto macro : rackType.getMacroParameters())
    {
        if (macro == nullptr)
            continue;

        const auto name = macro->getParameterName();

        if (name == volPrefix)
            volMacro = macro;
        else if (name == tunePrefix)
            tuneMacro = macro;
    }

    if (volMacro != nullptr)
        bindings.add (new PadMacroBinding (sampler, 0, padNote, *volMacro, PadMacroBinding::Kind::volume));

    if (tuneMacro != nullptr)
        bindings.add (new PadMacroBinding (sampler, 0, padNote, *tuneMacro, PadMacroBinding::Kind::tune));
}

void bindPadMacros (te::RackType& rackType, te::SamplerPlugin& sampler, int padIndex,
                    juce::OwnedArray<PadMacroBinding>& bindings)
{
    const int padNote = DrumRackHelpers::midiNoteForPad (padIndex);
    const juce::String volName = padDisplayName (padIndex) + " Vol";
    const juce::String tuneName = padDisplayName (padIndex) + " Tune";

    auto& macroList = rackType.getMacroParameterListForWriting();
    auto* volMacro = macroList.createMacroParameter();
    auto* tuneMacro = macroList.createMacroParameter();

    if (volMacro != nullptr)
    {
        volMacro->macroName = volName;
        volMacro->setNormalisedParameter (0.85f, juce::dontSendNotification);
        bindings.add (new PadMacroBinding (sampler, 0, padNote, *volMacro, PadMacroBinding::Kind::volume));
    }

    if (tuneMacro != nullptr)
    {
        tuneMacro->macroName = tuneName;
        tuneMacro->setNormalisedParameter (0.5f, juce::dontSendNotification);
        bindings.add (new PadMacroBinding (sampler, 0, padNote, *tuneMacro, PadMacroBinding::Kind::tune));
    }
}

} // namespace

const juce::Identifier& DrumRackHelpers::drumRackProperty()
{
    return drumRackId();
}

const juce::Identifier& DrumRackHelpers::padCountProperty()
{
    return padCountId();
}

bool DrumRackHelpers::isDrumRack (const te::RackType& rackType)
{
    return rackType.state.getProperty (drumRackId(), false);
}

bool DrumRackHelpers::isDrumRack (const te::RackInstance& rack)
{
    return rack.type != nullptr && isDrumRack (*rack.type);
}

int DrumRackHelpers::getPadCount (const te::RackInstance& rack)
{
    if (rack.type == nullptr)
        return 0;

    return rack.type->state.getProperty (padCountId(), defaultPadCount);
}

int DrumRackHelpers::midiNoteForPad (int padIndex)
{
    return firstPadMidiNote + padIndex;
}

te::SamplerPlugin* DrumRackHelpers::getPadSampler (te::RackInstance& rack, int padIndex)
{
    const auto samplers = getPadSamplers (rack);

    if (! juce::isPositiveAndBelow (padIndex, samplers.size()))
        return nullptr;

    return samplers.getReference (padIndex);
}

const te::SamplerPlugin* DrumRackHelpers::getPadSampler (const te::RackInstance& rack, int padIndex)
{
    return getPadSampler (const_cast<te::RackInstance&> (rack), padIndex);
}

juce::String DrumRackHelpers::getPadSampleName (const te::RackInstance& rack, int padIndex)
{
    if (const auto* sampler = getPadSampler (rack, padIndex))
        if (sampler->getNumSounds() > 0)
            return sampler->getSoundName (0);

    return {};
}

juce::String DrumRackHelpers::assignSampleToPad (te::RackInstance& rack, int padIndex, const juce::File& file)
{
    if (! file.existsAsFile())
        return "Sample file not found.";

    auto* sampler = getPadSampler (rack, padIndex);

    if (sampler == nullptr)
        return "Invalid pad.";

    while (sampler->getNumSounds() > 0)
        sampler->removeSound (0);

    const int padNote = midiNoteForPad (padIndex);
    const auto error = sampler->addSound (file.getFullPathName(),
                                          file.getFileNameWithoutExtension(),
                                          0.0, 0.0, 0.0f);

    if (error.isNotEmpty())
        return error;

    sampler->setSoundParams (sampler->getNumSounds() - 1, padNote, padNote, padNote);
    sampler->setSoundGains (sampler->getNumSounds() - 1, 0.0f, 0.0f);
    return {};
}

void DrumRackHelpers::clearPadSample (te::RackInstance& rack, int padIndex)
{
    if (auto* sampler = getPadSampler (rack, padIndex))
        while (sampler->getNumSounds() > 0)
            sampler->removeSound (0);
}

te::Plugin::Ptr DrumRackHelpers::createDrumRack (te::Edit& edit, int padCount)
{
    padCount = juce::jmax (1, padCount);

    auto rackType = edit.getRackList().addNewRack();

    if (rackType == nullptr)
        return {};

    auto& um = edit.getUndoManager();
    rackType->rackName = "Drum Rack";
    rackType->state.setProperty (drumRackId(), true, &um);
    rackType->state.setProperty (padCountId(), padCount, &um);

    juce::OwnedArray<PadMacroBinding> macroBindings;

    for (int padIndex = 0; padIndex < padCount; ++padIndex)
    {
        auto sampler = edit.getPluginCache().createNewPlugin (te::SamplerPlugin::xmlTypeName, {});

        if (sampler == nullptr)
            continue;

        EngineHelpers::renamePlugin (*sampler, padDisplayName (padIndex));

        if (! rackType->addPlugin (sampler, {}, false))
            continue;

        if (auto* samplerPlugin = dynamic_cast<te::SamplerPlugin*> (sampler.get()))
            bindPadMacros (*rackType, *samplerPlugin, padIndex, macroBindings);
    }

    registerBindings (*rackType, macroBindings);

    return edit.getPluginCache().createNewPlugin (te::RackInstance::create (*rackType));
}

te::RackInstance* DrumRackHelpers::insertDrumRackOnTrack (te::AudioTrack& track, int padCount)
{
    if (auto plugin = createDrumRack (track.edit, padCount))
        return dynamic_cast<te::RackInstance*> (EngineHelpers::insertPluginOnTrack (track, plugin));

    return nullptr;
}

void DrumRackHelpers::ensureMacroBindings (te::RackInstance& rack)
{
    if (! isDrumRack (rack) || rack.type == nullptr || hasBindingsFor (*rack.type))
        return;

    juce::OwnedArray<PadMacroBinding> bindings;
    const auto samplers = getPadSamplers (rack);

    for (int padIndex = 0; padIndex < samplers.size(); ++padIndex)
        if (auto* sampler = samplers[padIndex])
            bindExistingPadMacros (*rack.type, *sampler, padIndex, bindings);

    registerBindings (*rack.type, bindings);
}

} // namespace skeletonhive
