#pragma once

#include "PluginHostConstants.h"
#include "PluginHostProtocol.h"
#include "PluginHostSharedMemory.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

class PluginHostWorker : public juce::ChildProcessWorker
{
public:
    static bool tryInitialiseFromCommandLine (const juce::String& commandLine);

    void audioLoop();
    void sendReply (PluginHostMessageType type, const juce::MemoryBlock& payload = {});

private:
    PluginHostWorker() = default;

    void handleMessageFromCoordinator (const juce::MemoryBlock& message) override;
    void handleConnectionLost() override;

    bool handlePing();
    bool handleLoadPlugin (const juce::MemoryBlock& payload);
    bool handlePrepare (const juce::MemoryBlock& payload);
    bool handleSetParameter (const juce::MemoryBlock& payload);
    bool handleGetState();
    bool handleSetState (const juce::MemoryBlock& payload);
    bool handleOpenEditor();
    bool handleCloseEditor();
    void handleShutdown();

    void startAudioLoop();
    void stopAudioLoop();

    juce::AudioPluginFormatManager formatManager;
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
    PluginHostSharedMemory sharedMemory;
    juce::AudioBuffer<float> processBuffer;
    juce::MidiBuffer midiBuffer;

    std::unique_ptr<juce::DocumentWindow> editorWindow;

    std::unique_ptr<juce::Thread> audioThread;
    std::atomic<bool> audioRunning { false };
    juce::String sessionId;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    int numInputChannels = 2;
    int numOutputChannels = 2;
    bool pluginLoaded = false;
    bool prepared = false;
};

} // namespace skeletonhive
