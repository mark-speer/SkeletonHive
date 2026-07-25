#pragma once

#include "PluginHostCoordinator.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

struct SandboxEditorResult
{
    bool success = false;
    juce::String error;
    void* nativeHandle = nullptr;
    int width = 0;
    int height = 0;

    bool usesBridgeWindow() const { return success && nativeHandle == nullptr; }
};

class SandboxedPluginInstance : public juce::AudioPluginInstance,
                                private juce::Timer
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
    bool acceptsMidi() const override { return description.isInstrument; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    // Editors are opened via the sandbox bridge (requestBridgeEditor), not JUCE createEditor.
    // hasEditor must stay false: createEditor returns nullptr, and JUCE asserts if hasEditor is true.
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    void fillInPluginDescription (juce::PluginDescription& desc) const override { desc = description; }

    bool isLoading() const { return loading; }
    bool isLoaded() const { return loaded; }
    bool isCrashed() const { return crashed; }
    bool isSandboxed() const { return true; }

    void notifyBridgeCrashed();
    void openEditorInBridge();
    bool requestBridgeEditor (SandboxEditorResult& result);
    void closeEditorInBridge();

    PluginHostCoordinator* getCoordinator() { return getActiveCoordinator().get(); }

    static SandboxedPluginInstance* fromExternalPlugin (te::ExternalPlugin& plugin);
    static bool isSandboxedPlugin (const te::Plugin& plugin);

private:
    SandboxedPluginInstance (te::Engine& engine, const juce::PluginDescription& desc);

    bool initialiseBridge (double sampleRate, int blockSize, juce::String& errorMessage);

    std::shared_ptr<PluginHostCoordinator> getActiveCoordinator() const;
    void setActiveCoordinator (std::shared_ptr<PluginHostCoordinator> newCoordinator);

    void registerRoundTripSuccess();
    void registerRoundTripStall();

    void timerCallback() override;
    void attemptWatchdogRecovery();

    te::Engine& engineRef;
    juce::PluginDescription description;

    mutable juce::SpinLock coordinatorLock;
    std::shared_ptr<PluginHostCoordinator> coordinator;

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
    std::atomic<bool> prepareRequested { false };

    void finishPrepareOnMessageThread (double sampleRate, int samplesPerBlock);

    // Watchdog state: audio thread only writes consecutiveStalledChunks/lastSuccessMs/
    // recoveryRequested; the message-thread timer callback owns everything else and only
    // ever reads the audio-thread-written fields.
    std::atomic<int> consecutiveStalledChunks { 0 };
    std::atomic<double> lastSuccessMs { 0.0 };
    std::atomic<bool> recoveryRequested { false };
    std::atomic<bool> recovering { false };
    int restartAttempts = 0;
};

} // namespace skeletonhive
