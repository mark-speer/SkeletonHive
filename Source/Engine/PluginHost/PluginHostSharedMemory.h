#pragma once

#include "PluginHostConstants.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

/** Lock-free stereo audio exchange between host and bridge processes. */
class PluginHostSharedMemory
{
public:
    struct Header
    {
        std::atomic<uint32_t> hostSequence { 0 };
        std::atomic<uint32_t> workerSequence { 0 };
        std::atomic<uint32_t> numSamples { 0 };
        std::atomic<uint32_t> numInputChannels { 0 };
        std::atomic<uint32_t> numOutputChannels { 0 };
        std::atomic<uint32_t> workerReady { 0 };
        std::atomic<uint32_t> shutdownRequested { 0 };
    };

    PluginHostSharedMemory() = default;

    bool create (const juce::String& name);
    bool openExisting (const juce::String& name);
    void close();

    juce::String getName() const { return mappingName; }
    Header* getHeader() const { return header; }

    void writeInput (const juce::AudioBuffer<float>& buffer);
    /** Host audio-thread wait: spin-only, never blocks on OS waits. */
    bool waitForOutput (uint32_t hostSequence);
    void readOutput (juce::AudioBuffer<float>& buffer);

    /** Worker-side wait: may block on the wake event after a short spin. */
    bool waitForInput (uint32_t& hostSequenceToProcess);
    void readInput (juce::AudioBuffer<float>& buffer, int numInputChannels, int numSamples);
    void writeOutput (const juce::AudioBuffer<float>& buffer, int numOutputChannels, int numSamples, uint32_t processedHostSequence);

    void requestShutdown();
    bool isShutdownRequested() const;

private:
    static size_t totalMappingSize();
    float* inputData() const;
    float* outputData() const;

    bool openWaitEvents (bool asCreator);
    void closeWaitEvents();
    void signalEvent (void* event) const;

    /** Busy-spin until isReady()/shouldAbort() or the microsecond budget elapses.
        Safe for the host audio callback (no WaitForSingleObject / Sleep).
    */
    template <typename IsReadyFn, typename ShouldAbortFn>
    bool waitSpinOnly (IsReadyFn&& isReady, ShouldAbortFn&& shouldAbort, int budgetMicroseconds) const;

    /** Hybrid wait for the worker process: short spin, then block on wakeEvent.
        Must not be used from the host device callback.
    */
    template <typename IsReadyFn, typename ShouldAbortFn>
    bool waitWithHybridSpin (IsReadyFn&& isReady, ShouldAbortFn&& shouldAbort, void* wakeEvent) const;

    juce::String mappingName;
    std::unique_ptr<juce::MemoryMappedFile> mapping;
    Header* header = nullptr;

    void* inputReadyEvent = nullptr;
    void* outputReadyEvent = nullptr;
};

} // namespace skeletonhive
