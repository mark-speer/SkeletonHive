#pragma once

#include "TracktionCommon.h"

#include <atomic>
#include <memory>

namespace nam
{
class DSP;
}

namespace skeletonhive
{

/** Neural Amp Modeler wrapper registered as a TE built-in plugin. */
class NamPlugin : public te::Plugin
{
public:
    explicit NamPlugin (te::PluginCreationInfo info);
    ~NamPlugin() override;

    static const char* getPluginName() { return "Neural Amp Modeler"; }
    static const char* xmlTypeName;

    juce::String getName() const override;
    juce::String getPluginType() override;
    juce::String getShortName (int) override { return "NAM"; }
    juce::String getSelectableDescription() override;

    int getNumOutputChannelsGivenInputs (int numInputChannels) override;
    BusLayout getBusses() const override { return BusLayout::singleStereoInOut(); }
    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const te::PluginRenderContext&) override;

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    /** Load a .nam model from disk on a background thread. */
    void loadModelFile (const juce::String& absolutePath);

    juce::String getModelPath() const;
    juce::String getStatusMessage() const;
    bool isModelLoaded() const;

    juce::CachedValue<float> inputValue;
    juce::CachedValue<float> outputValue;
    juce::CachedValue<juce::String> modelPathValue;
    juce::CachedValue<juce::String> statusValue;

    te::AutomatableParameter::Ptr inputParam;
    te::AutomatableParameter::Ptr outputParam;

private:
    class LoadThread;
    class RetireDrainer;

    void installModel (std::shared_ptr<nam::DSP> model, const juce::String& path,
                       const juce::String& status, double modelSampleRate, int modelMaxBlock);
    void reloadModelFromState();
    void drainRetiredModels();
    void scheduleRetiredDrain();
    void startLoadThread (uint64_t generation, const juce::String& path);
    void loadThreadFinished();

    double sampleRate = 44100.0;
    int blockSizeSamples = 512;
    bool isPrepared = false;
    double preparedSampleRate = 0.0;
    int preparedMaxBlock = 0;
    bool reloadQueued = false;

    // Plain shared_ptr published with std::atomic_* free functions — portable on
    // Apple libc++, which still lacks std::atomic<std::shared_ptr<T>> in CI Xcode.
    std::shared_ptr<nam::DSP> activeModel;
    std::atomic<uint64_t> loadGeneration { 0 };

    juce::CriticalSection retireLock;
    std::vector<std::shared_ptr<nam::DSP>> retiredModels;
    std::unique_ptr<RetireDrainer> retireDrainer;

    std::unique_ptr<LoadThread> loadThread;

    juce::AudioBuffer<float> monoIn;
    juce::AudioBuffer<float> monoOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NamPlugin)
};

} // namespace skeletonhive
