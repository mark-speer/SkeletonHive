#include "PluginHostSharedMemory.h"

#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace skeletonhive
{

namespace
{
constexpr size_t headerSize = sizeof (PluginHostSharedMemory::Header);
constexpr size_t channelBlockBytes = (size_t) PluginHostConstants::maxChannels
                                   * (size_t) PluginHostConstants::maxBlockSize
                                   * sizeof (float);

juce::String makeEventName (const juce::String& mappingName, const char* suffix)
{
    return "Local\\SkeletonHivePluginHost_" + mappingName + "_" + suffix;
}
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

    if (! openWaitEvents (true))
        DBG ("PluginHostSharedMemory: failed to create wait events for " + name + " (falling back to spin-only waits)");

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

    if (header == nullptr)
        return false;

    if (! openWaitEvents (false))
        DBG ("PluginHostSharedMemory: failed to open wait events for " + name + " (falling back to spin-only waits)");

    return true;
}

void PluginHostSharedMemory::close()
{
    closeWaitEvents();
    header = nullptr;
    mapping.reset();
    mappingName.clear();
}

bool PluginHostSharedMemory::openWaitEvents (bool asCreator)
{
   #if JUCE_WINDOWS
    const auto inName = makeEventName (mappingName, "in");
    const auto outName = makeEventName (mappingName, "out");

    if (asCreator)
    {
        inputReadyEvent = (void*) CreateEventW (nullptr, FALSE, FALSE, inName.toWideCharPointer());
        outputReadyEvent = (void*) CreateEventW (nullptr, FALSE, FALSE, outName.toWideCharPointer());
    }
    else
    {
        inputReadyEvent = (void*) OpenEventW (EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, inName.toWideCharPointer());
        outputReadyEvent = (void*) OpenEventW (EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, outName.toWideCharPointer());
    }

    if (inputReadyEvent == nullptr || outputReadyEvent == nullptr)
    {
        closeWaitEvents();
        return false;
    }

    return true;
   #else
    juce::ignoreUnused (asCreator);
    return false;
   #endif
}

void PluginHostSharedMemory::closeWaitEvents()
{
   #if JUCE_WINDOWS
    if (inputReadyEvent != nullptr)
    {
        CloseHandle ((HANDLE) inputReadyEvent);
        inputReadyEvent = nullptr;
    }

    if (outputReadyEvent != nullptr)
    {
        CloseHandle ((HANDLE) outputReadyEvent);
        outputReadyEvent = nullptr;
    }
   #endif
}

void PluginHostSharedMemory::signalEvent (void* event) const
{
   #if JUCE_WINDOWS
    if (event != nullptr)
        SetEvent ((HANDLE) event);
   #else
    juce::ignoreUnused (event);
   #endif
}

template <typename IsReadyFn, typename ShouldAbortFn>
bool PluginHostSharedMemory::waitSpinOnly (IsReadyFn&& isReady, ShouldAbortFn&& shouldAbort, int budgetMicroseconds) const
{
    const auto ticksPerSecond = (double) juce::Time::getHighResolutionTicksPerSecond();
    const auto startTicks = juce::Time::getHighResolutionTicks();
    const auto deadlineTicks = startTicks
                             + (int64_t) ((juce::jmax (1, budgetMicroseconds) / 1.0e6) * ticksPerSecond);

    for (;;)
    {
        if (isReady())
            return true;

        if (shouldAbort())
            return false;

        if (juce::Time::getHighResolutionTicks() >= deadlineTicks)
            return isReady();
    }
}

template <typename IsReadyFn, typename ShouldAbortFn>
bool PluginHostSharedMemory::waitWithHybridSpin (IsReadyFn&& isReady, ShouldAbortFn&& shouldAbort, void* wakeEvent) const
{
    const auto ticksPerSecond = (double) juce::Time::getHighResolutionTicksPerSecond();
    const auto startTicks = juce::Time::getHighResolutionTicks();
    const auto spinDeadlineTicks = startTicks
                                  + (int64_t) ((PluginHostConstants::spinBeforeBlockMicroseconds / 1.0e6) * ticksPerSecond);
    const auto hardDeadlineTicks = startTicks
                                  + (int64_t) ((PluginHostConstants::chunkStallTimeoutMicroseconds / 1.0e6) * ticksPerSecond);

    // Phase 1: tight busy-spin. This is what preserves today's best-case latency, since
    // the worker almost always finishes well inside this short window.
    for (;;)
    {
        if (isReady())
            return true;

        if (shouldAbort())
            return false;

        if (juce::Time::getHighResolutionTicks() >= spinDeadlineTicks)
            break;
    }

   #if JUCE_WINDOWS
    if (wakeEvent != nullptr)
    {
        for (;;)
        {
            if (isReady())
                return true;

            if (shouldAbort())
                return false;

            const auto nowTicks = juce::Time::getHighResolutionTicks();

            if (nowTicks >= hardDeadlineTicks)
                break;

            const auto remainingMs = (DWORD) juce::jmax ((int64_t) 1,
                                                          (int64_t) (((hardDeadlineTicks - nowTicks) * 1000) / (int64_t) ticksPerSecond) + 1);

            WaitForSingleObject ((HANDLE) wakeEvent, remainingMs);
        }

        return isReady();
    }
   #else
    juce::ignoreUnused (wakeEvent);
   #endif

    // No wake event available (non-Windows, or event creation failed) — fall back to
    // spinning with periodic OS yields for the remainder of the deadline, matching the
    // previous spin-only behaviour rather than silently regressing.
    while (juce::Time::getHighResolutionTicks() < hardDeadlineTicks)
    {
        if (isReady())
            return true;

        if (shouldAbort())
            return false;

        juce::Thread::yield();
    }

    return isReady();
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
    signalEvent (inputReadyEvent);
}

bool PluginHostSharedMemory::waitForOutput (uint32_t hostSequence)
{
    if (header == nullptr)
        return false;

    // Host device callback path: spin only. Blocking here (WaitForSingleObject) can
    // stall the entire audio engine when a sandboxed effect is slow or still starting.
    return waitSpinOnly ([this, hostSequence]
                         {
                             return header->workerSequence.load (std::memory_order_acquire) >= hostSequence;
                         },
                         [this] { return isShutdownRequested(); },
                         PluginHostConstants::hostRealtimeRoundTripBudgetMicroseconds);
}

void PluginHostSharedMemory::readOutput (juce::AudioBuffer<float>& buffer)
{
    if (header == nullptr)
        return;

    const int numSamples = juce::jmin (buffer.getNumSamples(),
                                       (int) header->numSamples.load (std::memory_order_acquire));
    const int numChannels = juce::jmin (buffer.getNumChannels(),
                                        (int) header->numOutputChannels.load (std::memory_order_acquire));
    auto* output = outputData();

    if (numSamples <= 0)
        return;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    for (int ch = 0; ch < numChannels; ++ch)
        std::memcpy (buffer.getWritePointer (ch),
                     output + (size_t) ch * PluginHostConstants::maxBlockSize,
                     (size_t) numSamples * sizeof (float));
}

bool PluginHostSharedMemory::waitForInput (uint32_t& hostSequenceToProcess)
{
    if (header == nullptr)
        return false;

    const auto ready = waitWithHybridSpin ([this, &hostSequenceToProcess]
                                           {
                                               const uint32_t hostSequence = header->hostSequence.load (std::memory_order_acquire);
                                               const uint32_t workerSequence = header->workerSequence.load (std::memory_order_acquire);

                                               if (hostSequence > workerSequence)
                                               {
                                                   hostSequenceToProcess = hostSequence;
                                                   return true;
                                               }

                                               return false;
                                           },
                                           [this] { return isShutdownRequested(); },
                                           inputReadyEvent);

    return ready && ! isShutdownRequested();
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

void PluginHostSharedMemory::writeOutput (const juce::AudioBuffer<float>& buffer, int numOutputChannels, int numSamples,
                                          uint32_t processedHostSequence)
{
    if (header == nullptr)
        return;

    auto* output = outputData();
    const int channels = juce::jmin (numOutputChannels,
                                     buffer.getNumChannels(),
                                     PluginHostConstants::maxChannels);

    for (int ch = 0; ch < channels; ++ch)
        std::memcpy (output + (size_t) ch * PluginHostConstants::maxBlockSize,
                     buffer.getReadPointer (ch),
                     (size_t) numSamples * sizeof (float));

    header->numOutputChannels.store ((uint32_t) channels, std::memory_order_release);
    header->workerSequence.store (processedHostSequence, std::memory_order_release);
    signalEvent (outputReadyEvent);
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
