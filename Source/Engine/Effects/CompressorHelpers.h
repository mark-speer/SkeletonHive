#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

/** Thin helpers over te::CompressorPlugin. */
class CompressorHelpers
{
public:
    static float getThreshold (const te::CompressorPlugin& comp);
    static float getRatio (const te::CompressorPlugin& comp);
    static float getAttackMs (const te::CompressorPlugin& comp);
    static float getReleaseMs (const te::CompressorPlugin& comp);
    static float getOutputDb (const te::CompressorPlugin& comp);
    static float getSidechainDb (const te::CompressorPlugin& comp);
    static bool getUseSidechain (const te::CompressorPlugin& comp);

    static void setThreshold (te::CompressorPlugin& comp, float value, juce::UndoManager* um);
    static void setRatio (te::CompressorPlugin& comp, float value, juce::UndoManager* um);
    static void setAttackMs (te::CompressorPlugin& comp, float ms, juce::UndoManager* um);
    static void setReleaseMs (te::CompressorPlugin& comp, float ms, juce::UndoManager* um);
    static void setOutputDb (te::CompressorPlugin& comp, float db, juce::UndoManager* um);
    static void setSidechainDb (te::CompressorPlugin& comp, float db, juce::UndoManager* um);
    static void setUseSidechain (te::CompressorPlugin& comp, bool enabled, juce::UndoManager* um);
};

} // namespace skeletonhive
