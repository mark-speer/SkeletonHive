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
    bool waitForOutput();
    void readOutput (juce::AudioBuffer<float>& buffer);

    bool waitForInput();
    void readInput (juce::AudioBuffer<float>& buffer, int numInputChannels, int numSamples);
    void writeOutput (const juce::AudioBuffer<float>& buffer, int numOutputChannels, int numSamples);

    void requestShutdown();
    bool isShutdownRequested() const;

private:
    static size_t totalMappingSize();
    float* inputData() const;
    float* outputData() const;

    juce::String mappingName;
    std::unique_ptr<juce::MemoryMappedFile> mapping;
    Header* header = nullptr;
};

} // namespace skeletonhive
