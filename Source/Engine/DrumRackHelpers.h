#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

/** Helpers for the native 16-pad Drum Rack built on te::RackType + te::SamplerPlugin. */
class DrumRackHelpers
{
public:
    static constexpr int defaultPadCount = 16;
    static constexpr int firstPadMidiNote = 36;
    static constexpr const char* drumRackXmlTypeName = "DrumRack";

    static const juce::Identifier& drumRackProperty();
    static const juce::Identifier& padCountProperty();

    static bool isDrumRack (const te::RackType& rackType);
    static bool isDrumRack (const te::RackInstance& rack);

    static int getPadCount (const te::RackInstance& rack);
    static int midiNoteForPad (int padIndex);

    static te::SamplerPlugin* getPadSampler (te::RackInstance& rack, int padIndex);
    static const te::SamplerPlugin* getPadSampler (const te::RackInstance& rack, int padIndex);

    static juce::String getPadSampleName (const te::RackInstance& rack, int padIndex);

    static juce::String assignSampleToPad (te::RackInstance& rack, int padIndex, const juce::File& file);
    static void clearPadSample (te::RackInstance& rack, int padIndex);

    /** Creates a drum-rack RackType (pads + macros) and returns a RackInstance plugin. */
    static te::Plugin::Ptr createDrumRack (te::Edit& edit, int padCount = defaultPadCount);

    static te::RackInstance* insertDrumRackOnTrack (te::AudioTrack& track, int padCount = defaultPadCount);

    /** Re-attaches macro listeners after project load (idempotent). */
    static void ensureMacroBindings (te::RackInstance& rack);
};

} // namespace skeletonhive
