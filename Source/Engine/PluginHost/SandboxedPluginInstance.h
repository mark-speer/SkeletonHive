#pragma once

#include "PluginHostCoordinator.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

class SandboxedPluginInstance : public juce::AudioPluginInstance
{
public:
    static std::unique_ptr<SandboxedPluginInstance> create (te::Engine& engine,
                                                            const juce::PluginDescription& desc,
                                                            double sampleRate,
                                                            int blockSize,
                                                            juce::String& errorMessage);

    ~SandboxedPluginInstance() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    const juce::String getName() const override { return pluginName; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    bool hasEditor() const override { return loaded && ! crashed; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    void fillInPluginDescription (juce::PluginDescription& desc) const override { desc = description; }

    bool isLoading() const { return loading; }
    bool isLoaded() const { return loaded; }
    bool isCrashed() const { return crashed; }
    bool isSandboxed() const { return true; }

    void notifyBridgeCrashed();
    void openEditorInBridge();
    bool requestBridgeEditor (juce::String& errorMessage);
    void closeEditorInBridge();

    PluginHostCoordinator* getCoordinator() { return coordinator.get(); }

    static SandboxedPluginInstance* fromExternalPlugin (te::ExternalPlugin& plugin);
    static bool isSandboxedPlugin (const te::Plugin& plugin);

private:
    SandboxedPluginInstance (te::Engine& engine, const juce::PluginDescription& desc);

    bool initialiseBridge (double sampleRate, int blockSize, juce::String& errorMessage);

    te::Engine& engineRef;
    juce::PluginDescription description;
    std::unique_ptr<PluginHostCoordinator> coordinator;
    juce::String pluginName;
    juce::AudioBuffer<float> processBuffer;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    int numInputChannels = 2;
    int numOutputChannels = 2;
    std::atomic<bool> loading { true };
    std::atomic<bool> loaded { false };
    std::atomic<bool> crashed { false };
    std::atomic<bool> prepared { false };
};

} // namespace skeletonhive
