#include "PluginHostSharedMemory.h"

namespace skeletonhive
{

namespace
{
constexpr size_t headerSize = sizeof (PluginHostSharedMemory::Header);
constexpr size_t channelBlockBytes = (size_t) PluginHostConstants::maxChannels
                                   * (size_t) PluginHostConstants::maxBlockSize
                                   * sizeof (float);
} // namespace

size_t PluginHostSharedMemory::totalMappingSize()
{
    return headerSize + channelBlockBytes * 2;
}

float* PluginHostSharedMemory::inputData() const
{
    return reinterpret_cast<float*> (reinterpret_cast<char*> (header) + headerSize);
}

float* PluginHostSharedMemory::outputData() const
{
    return inputData() + (PluginHostConstants::maxChannels * PluginHostConstants::maxBlockSize);
}

bool PluginHostSharedMemory::create (const juce::String& name)
{
    close();
    mappingName = name;

    const auto parentDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("SkeletonHivePluginHost");

    parentDir.createDirectory();

    if (! parentDir.isDirectory())
    {
        DBG ("PluginHostSharedMemory: temp directory unavailable " + parentDir.getFullPathName());
        return false;
    }

    juce::File tempFile = parentDir.getChildFile (name + ".shmem");

    if (tempFile.existsAsFile())
        tempFile.deleteFile();

    const size_t size = totalMappingSize();
    juce::MemoryBlock blank (size);
    blank.fillWith (0);

    if (! tempFile.replaceWithData (blank.getData(), blank.getSize()))
    {
        DBG ("PluginHostSharedMemory: failed to write temp file " + tempFile.getFullPathName());
        return false;
    }

    mapping = std::make_unique<juce::MemoryMappedFile> (tempFile, juce::MemoryMappedFile::readWrite);

    if (mapping->getData() == nullptr)
    {
        DBG ("PluginHostSharedMemory: failed to map temp file " + tempFile.getFullPathName());
        mapping.reset();
        tempFile.deleteFile();
        return false;
    }

    header = reinterpret_cast<Header*> (mapping->getData());
    std::memset (header, 0, headerSize);
    header->workerReady.store (0, std::memory_order_release);
    return true;
}

bool PluginHostSharedMemory::openExisting (const juce::String& name)
{
    close();
    mappingName = name;

    juce::File tempFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("SkeletonHivePluginHost")
                              .getChildFile (name + ".shmem");

    mapping = std::make_unique<juce::MemoryMappedFile> (tempFile, juce::MemoryMappedFile::readWrite);
    if (mapping->getData() == nullptr)
        return false;

    header = reinterpret_cast<Header*> (mapping->getData());
    return header != nullptr;
}

void PluginHostSharedMemory::close()
{
    header = nullptr;
    mapping.reset();
    mappingName.clear();
}

void PluginHostSharedMemory::writeInput (const juce::AudioBuffer<float>& buffer)
{
    if (header == nullptr)
        return;

    const int numSamples = juce::jmin (buffer.getNumSamples(), PluginHostConstants::maxBlockSize);
    const int numChannels = juce::jmin (buffer.getNumChannels(), PluginHostConstants::maxChannels);
    auto* input = inputData();

    for (int ch = 0; ch < numChannels; ++ch)
        std::memcpy (input + (size_t) ch * PluginHostConstants::maxBlockSize,
                     buffer.getReadPointer (ch),
                     (size_t) numSamples * sizeof (float));

    header->numSamples.store ((uint32_t) numSamples, std::memory_order_release);
    header->numInputChannels.store ((uint32_t) numChannels, std::memory_order_release);
    header->hostSequence.fetch_add (1, std::memory_order_release);
}

bool PluginHostSharedMemory::waitForOutput()
{
    if (header == nullptr)
        return false;

    const uint32_t target = header->hostSequence.load (std::memory_order_acquire);

    for (int i = 0; i < PluginHostConstants::processSpinLimit; ++i)
    {
        if (header->workerSequence.load (std::memory_order_acquire) >= target)
            return true;
    }

    return false;
}

void PluginHostSharedMemory::readOutput (juce::AudioBuffer<float>& buffer)
{
    if (header == nullptr)
        return;

    const int numSamples = (int) header->numSamples.load (std::memory_order_acquire);
    const int numChannels = juce::jmin (buffer.getNumChannels(),
                                        (int) header->numOutputChannels.load (std::memory_order_acquire));
    auto* output = outputData();

    for (int ch = 0; ch < numChannels; ++ch)
        std::memcpy (buffer.getWritePointer (ch),
                     output + (size_t) ch * PluginHostConstants::maxBlockSize,
                     (size_t) numSamples * sizeof (float));
}

bool PluginHostSharedMemory::waitForInput()
{
    if (header == nullptr)
        return false;

    const uint32_t lastProcessed = header->workerSequence.load (std::memory_order_acquire);
    const uint32_t target = lastProcessed + 1;

    for (int i = 0; i < PluginHostConstants::processSpinLimit; ++i)
    {
        if (isShutdownRequested())
            return false;

        if (header->hostSequence.load (std::memory_order_acquire) >= target)
            return true;

        juce::Thread::sleep (1);
    }

    return false;
}

void PluginHostSharedMemory::readInput (juce::AudioBuffer<float>& buffer, int numInputChannels, int numSamples)
{
    auto* input = inputData();
    const int channels = juce::jmin (numInputChannels, buffer.getNumChannels());

    for (int ch = 0; ch < channels; ++ch)
        std::memcpy (buffer.getWritePointer (ch),
                     input + (size_t) ch * PluginHostConstants::maxBlockSize,
                     (size_t) numSamples * sizeof (float));
}

void PluginHostSharedMemory::writeOutput (const juce::AudioBuffer<float>& buffer, int numOutputChannels, int numSamples)
{
    if (header == nullptr)
        return;

    auto* output = outputData();
    const int channels = juce::jmin (numOutputChannels, buffer.getNumChannels());

    for (int ch = 0; ch < channels; ++ch)
        std::memcpy (output + (size_t) ch * PluginHostConstants::maxBlockSize,
                     buffer.getReadPointer (ch),
                     (size_t) numSamples * sizeof (float));

    header->numOutputChannels.store ((uint32_t) channels, std::memory_order_release);
    header->workerSequence.store (header->hostSequence.load (std::memory_order_acquire), std::memory_order_release);
}

void PluginHostSharedMemory::requestShutdown()
{
    if (header != nullptr)
        header->shutdownRequested.store (1, std::memory_order_release);
}

bool PluginHostSharedMemory::isShutdownRequested() const
{
    return header != nullptr && header->shutdownRequested.load (std::memory_order_acquire) != 0;
}

} // namespace skeletonhive
