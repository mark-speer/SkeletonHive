#include "SandboxedPluginInstance.h"
#include "PluginHostHelpers.h"

namespace skeletonhive
{

std::unique_ptr<SandboxedPluginInstance> SandboxedPluginInstance::create (te::Engine& engine,
                                                                          const juce::PluginDescription& desc,
                                                                          double sampleRate,
                                                                          int blockSize,
                                                                          juce::String& errorMessage)
{
    auto instance = std::unique_ptr<SandboxedPluginInstance> (new SandboxedPluginInstance (engine, desc));

    if (! instance->initialiseBridge (sampleRate, blockSize, errorMessage))
        return nullptr;

    return instance;
}

SandboxedPluginInstance::SandboxedPluginInstance (te::Engine& engine, const juce::PluginDescription& desc)
    : engineRef (engine),
      description (desc)
{
}

SandboxedPluginInstance::~SandboxedPluginInstance()
{
    stopTimer();
    closeEditorInBridge();

    // Detach before members (including atomics / shared_ptr coordinator) are destroyed.
    // Otherwise ~PluginHostCoordinator → handleConnectionLost → notifyBridgeCrashed can
    // write into already-destroyed atomics, or stopTimer on a half-torn-down Timer.
    if (auto active = getActiveCoordinator())
        active->setInstance (nullptr);

    setActiveCoordinator (nullptr);
}

std::shared_ptr<PluginHostCoordinator> SandboxedPluginInstance::getActiveCoordinator() const
{
    const juce::SpinLock::ScopedLockType sl (coordinatorLock);
    return coordinator;
}

void SandboxedPluginInstance::setActiveCoordinator (std::shared_ptr<PluginHostCoordinator> newCoordinator)
{
    const juce::SpinLock::ScopedLockType sl (coordinatorLock);
    coordinator = std::move (newCoordinator);
}

bool SandboxedPluginInstance::initialiseBridge (double sampleRate, int samplesPerBlock, juce::String& errorMessage)
{
    auto newCoordinator = std::make_shared<PluginHostCoordinator>();
    newCoordinator->setInstance (this);

    const auto executable = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    juce::String launchError;

    const auto launchResult = juce::MessageManager::callSync ([&]
    {
        return newCoordinator->launchAndConnect (executable, launchError);
    });

    if (! launchResult.has_value() || ! *launchResult)
    {
        errorMessage = launchError.isNotEmpty() ? launchError
                                                  : juce::String ("Failed to launch plugin sandbox process.");
        loading = false;
        crashed = true;
        return false;
    }

    PluginHostMessage reply;
    const auto loadPayload = PluginHostMessage::encodeLoadPlugin (description,
                                                                  newCoordinator->getSessionId(),
                                                                  sampleRate,
                                                                  samplesPerBlock);

    if (! newCoordinator->sendMessageAndWaitForAnyReply (PluginHostMessageType::loadPlugin,
                                                         loadPayload,
                                                         reply,
                                                         60000))
    {
        errorMessage = "Plugin sandbox failed to respond.";
        loading = false;
        crashed = newCoordinator->hasCrashed();
        newCoordinator->sendMessage (PluginHostMessageType::shutdown);
        DBG ("Sandbox load timeout: " + description.name);
        return false;
    }

    if (reply.type == PluginHostMessageType::pluginLoadFailed)
    {
        errorMessage = PluginHostMessage::decodeFailure (reply.payload);
        if (errorMessage.isEmpty())
            errorMessage = "Plugin sandbox failed to load the plugin.";
        loading = false;
        newCoordinator->sendMessage (PluginHostMessageType::shutdown);
        DBG ("Sandbox load failed for " + description.name + ": " + errorMessage);
        return false;
    }

    if (reply.type != PluginHostMessageType::pluginLoaded)
    {
        errorMessage = "Unexpected plugin sandbox response.";
        loading = false;
        newCoordinator->sendMessage (PluginHostMessageType::shutdown);
        DBG ("Sandbox unexpected response for " + description.name);
        return false;
    }

    juce::StringArray paramNames;
    if (! PluginHostMessage::decodePluginLoaded (reply.payload, pluginName, numInputChannels, numOutputChannels, paramNames))
    {
        errorMessage = "Invalid plugin sandbox load response.";
        loading = false;
        newCoordinator->sendMessage (PluginHostMessageType::shutdown);
        return false;
    }

    juce::ignoreUnused (paramNames);
    setActiveCoordinator (newCoordinator);
    setPlayConfigDetails (numInputChannels, numOutputChannels, sampleRate, samplesPerBlock);
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    processBuffer.setSize (juce::jmax (1, juce::jmax (numInputChannels, numOutputChannels)), samplesPerBlock);
    loading = false;
    loaded = true;
    lastSuccessMs.store (juce::Time::getMillisecondCounterHiRes(), std::memory_order_relaxed);

    prepareToPlay (sampleRate, samplesPerBlock);

    if (! prepared)
    {
        errorMessage = "Plugin sandbox failed to prepare.";
        loaded = false;
        newCoordinator->sendMessage (PluginHostMessageType::shutdown);
        setActiveCoordinator (nullptr);
        return false;
    }

    // Poll for stalls/hangs on the message thread so a single bad round trip on the
    // real-time audio thread never has to do anything more than flip an atomic flag.
    startTimer (250);

    return true;
}

void SandboxedPluginInstance::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Drop prepared so processBlock cannot race a transport prepare/reconfigure.
    prepared.store (false, std::memory_order_release);

    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    processBuffer.setSize (juce::jmax (1, juce::jmax (numInputChannels, numOutputChannels)), samplesPerBlock);

    auto activeCoordinator = getActiveCoordinator();

    if (activeCoordinator == nullptr || ! loaded || crashed)
        return;

    // Never block the host device callback on sandbox IPC (up to 10s). If TE invokes
    // prepare from the audio thread during a graph rebuild, queue work for the timer
    // and dry-passthrough until the worker answers on the message thread.
    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating();
        mm == nullptr || ! mm->isThisTheMessageThread())
    {
        prepareRequested.store (true, std::memory_order_release);
        return;
    }

    finishPrepareOnMessageThread (sampleRate, samplesPerBlock);
}

void SandboxedPluginInstance::finishPrepareOnMessageThread (double sampleRate, int samplesPerBlock)
{
    auto activeCoordinator = getActiveCoordinator();

    if (activeCoordinator == nullptr || ! loaded.load (std::memory_order_acquire)
        || crashed.load (std::memory_order_acquire))
        return;

    PluginHostMessage reply;
    const bool ok = activeCoordinator->sendMessageAndWaitForReply (PluginHostMessageType::prepare,
                                                                   PluginHostMessage::encodePrepare (sampleRate, samplesPerBlock),
                                                                   PluginHostMessageType::prepared,
                                                                   reply,
                                                                   10000);
    prepared.store (ok, std::memory_order_release);
}

void SandboxedPluginInstance::releaseResources()
{
    prepared.store (false, std::memory_order_release);
}

void SandboxedPluginInstance::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    // Snapshot once per callback so a watchdog-triggered coordinator swap on the message
    // thread can never happen mid-callback; this chunked round trip either finishes
    // against the old bridge or, on the very next callback, starts using the new one.
    auto activeCoordinator = getActiveCoordinator();

    if (! loaded.load (std::memory_order_acquire)
        || crashed.load (std::memory_order_acquire)
        || activeCoordinator == nullptr
        || ! prepared.load (std::memory_order_acquire)
        || recovering.load (std::memory_order_acquire))
    {
        // Effects: leave the buffer alone (dry passthrough) so a preparing/recovering
        // sandbox can't mute the track — or, via RT overruns, the whole engine.
        // Instruments have no meaningful dry signal, so clear.
        if (description.isInstrument)
            buffer.clear();
        return;
    }

    auto& sharedMemory = activeCoordinator->getSharedMemory();
    const int totalSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    int offset = 0;

    // The shared-memory transport only has room for PluginHostConstants::maxBlockSize
    // samples per round trip, but the host's actual audio block size can be larger
    // (many WASAPI/ASIO devices default to 1024+ samples). Slice the buffer into
    // transport-sized chunks so every sample gets sent to and returned from the
    // worker process, rather than leaving a raw/unprocessed tail in the buffer.
    while (offset < totalSamples)
    {
        const int chunkSamples = juce::jmin (totalSamples - offset, PluginHostConstants::maxBlockSize);

        juce::AudioBuffer<float> chunkView (buffer.getArrayOfWritePointers(), numChannels, offset, chunkSamples);

        sharedMemory.writeInput (chunkView);

        const auto hostSequence = sharedMemory.getHeader() != nullptr
                                      ? sharedMemory.getHeader()->hostSequence.load (std::memory_order_acquire)
                                      : (uint32_t) 0;

        if (! sharedMemory.waitForOutput (hostSequence))
        {
            DBG ("SandboxedPluginInstance: timed out waiting for bridge output for " + description.name);

            if (activeCoordinator->hasCrashed())
            {
                notifyBridgeCrashed();
                if (description.isInstrument)
                    buffer.clear();
                return;
            }

            // Leave this chunk as dry passthrough (better than silence) and keep
            // going so a single stall doesn't also corrupt the rest of the buffer.
            registerRoundTripStall();
            offset += chunkSamples;
            continue;
        }

        sharedMemory.readOutput (chunkView);
        registerRoundTripSuccess();
        offset += chunkSamples;
    }
}

void SandboxedPluginInstance::registerRoundTripSuccess()
{
    consecutiveStalledChunks.store (0, std::memory_order_relaxed);
    lastSuccessMs.store (juce::Time::getMillisecondCounterHiRes(), std::memory_order_relaxed);
}

void SandboxedPluginInstance::registerRoundTripStall()
{
    const auto stalls = consecutiveStalledChunks.fetch_add (1, std::memory_order_relaxed) + 1;

    // A recovery cycle is already under way (or one was already requested and hasn't
    // been picked up yet) — nothing more for the audio thread to do until it resolves.
    if (recovering.load (std::memory_order_relaxed) || recoveryRequested.load (std::memory_order_relaxed))
        return;

    const auto lastSuccess = lastSuccessMs.load (std::memory_order_relaxed);
    const auto sinceSuccessMs = juce::Time::getMillisecondCounterHiRes() - lastSuccess;

    if (stalls >= PluginHostConstants::maxConsecutiveStalledChunks
        || sinceSuccessMs >= (double) PluginHostConstants::hangRecoveryTimeoutMs)
    {
        recoveryRequested.store (true, std::memory_order_relaxed);
    }
}

void SandboxedPluginInstance::timerCallback()
{
    if (crashed)
    {
        stopTimer();
        return;
    }

    if (prepareRequested.exchange (false, std::memory_order_acq_rel))
        finishPrepareOnMessageThread (currentSampleRate, currentBlockSize);

    if (! recoveryRequested.exchange (false, std::memory_order_relaxed))
        return;

    if (recovering.load (std::memory_order_relaxed))
        return;

    attemptWatchdogRecovery();
}

void SandboxedPluginInstance::attemptWatchdogRecovery()
{
    recovering.store (true, std::memory_order_relaxed);

    auto oldCoordinator = getActiveCoordinator();

    juce::MemoryBlock savedState;
    if (oldCoordinator != nullptr)
    {
        PluginHostMessage stateReply;
        if (oldCoordinator->sendMessageAndWaitForReply (PluginHostMessageType::getState,
                                                         {},
                                                         PluginHostMessageType::stateBlob,
                                                         stateReply,
                                                         500))
            savedState = stateReply.payload;
    }

    ++restartAttempts;

    if (restartAttempts > PluginHostConstants::maxAutoRestartAttempts)
    {
        DBG ("SandboxedPluginInstance: giving up auto-recovery for " + description.name
             + " after " + juce::String (restartAttempts - 1) + " attempt(s)");
        notifyBridgeCrashed();
        recovering.store (false, std::memory_order_relaxed);
        return;
    }

    DBG ("SandboxedPluginInstance: bridge stalled for " + description.name
         + ", attempting auto-recovery (attempt " + juce::String (restartAttempts) + ")");

    auto newCoordinator = std::make_shared<PluginHostCoordinator>();
    newCoordinator->setInstance (this);

    const auto executable = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    juce::String launchError;

    if (! newCoordinator->launchAndConnect (executable, launchError))
    {
        DBG ("SandboxedPluginInstance: recovery relaunch failed for " + description.name + ": " + launchError);
        recovering.store (false, std::memory_order_relaxed);
        return;
    }

    PluginHostMessage loadReply;
    const auto loadPayload = PluginHostMessage::encodeLoadPlugin (description,
                                                                   newCoordinator->getSessionId(),
                                                                   currentSampleRate,
                                                                   currentBlockSize);

    if (! newCoordinator->sendMessageAndWaitForAnyReply (PluginHostMessageType::loadPlugin, loadPayload, loadReply, 60000)
        || loadReply.type != PluginHostMessageType::pluginLoaded)
    {
        DBG ("SandboxedPluginInstance: recovery reload failed for " + description.name);
        newCoordinator->sendMessage (PluginHostMessageType::shutdown);
        recovering.store (false, std::memory_order_relaxed);
        return;
    }

    if (savedState.getSize() > 0)
        newCoordinator->sendMessage (PluginHostMessageType::setState, savedState);

    PluginHostMessage prepareReply;
    const bool preparedOk = newCoordinator->sendMessageAndWaitForReply (PluginHostMessageType::prepare,
                                                                        PluginHostMessage::encodePrepare (currentSampleRate, currentBlockSize),
                                                                        PluginHostMessageType::prepared,
                                                                        prepareReply,
                                                                        10000);

    if (! preparedOk)
    {
        DBG ("SandboxedPluginInstance: recovery prepare failed for " + description.name);
        newCoordinator->sendMessage (PluginHostMessageType::shutdown);
        recovering.store (false, std::memory_order_relaxed);
        return;
    }

    // Detach the old (hung) coordinator from this instance before it's destroyed, so its
    // teardown (shutdown message + process kill) can never turn around and call
    // notifyBridgeCrashed() on the instance that has already moved on to newCoordinator.
    if (oldCoordinator != nullptr)
        oldCoordinator->setInstance (nullptr);

    setActiveCoordinator (newCoordinator);
    loaded = true;
    prepared = true;
    consecutiveStalledChunks.store (0, std::memory_order_relaxed);
    lastSuccessMs.store (juce::Time::getMillisecondCounterHiRes(), std::memory_order_relaxed);
    restartAttempts = 0;
    recovering.store (false, std::memory_order_relaxed);

    DBG ("SandboxedPluginInstance: auto-recovery succeeded for " + description.name);
}

void SandboxedPluginInstance::getStateInformation (juce::MemoryBlock& destData)
{
    destData.reset();

    auto activeCoordinator = getActiveCoordinator();

    if (activeCoordinator == nullptr || ! loaded || crashed)
        return;

    PluginHostMessage reply;
    if (activeCoordinator->sendMessageAndWaitForReply (PluginHostMessageType::getState,
                                                       {},
                                                       PluginHostMessageType::stateBlob,
                                                       reply,
                                                       5000))
        destData = reply.payload;
}

void SandboxedPluginInstance::setStateInformation (const void* data, int sizeInBytes)
{
    auto activeCoordinator = getActiveCoordinator();

    if (activeCoordinator == nullptr || ! loaded || crashed)
        return;

    juce::MemoryBlock payload (data, (size_t) sizeInBytes);
    activeCoordinator->sendMessage (PluginHostMessageType::setState, payload);
}

void SandboxedPluginInstance::notifyBridgeCrashed()
{
    crashed.store (true, std::memory_order_release);
    loaded.store (false, std::memory_order_release);
    prepared.store (false, std::memory_order_release);

    // NEVER call stopTimer() here: this is invoked from the audio thread (processBlock
    // timeout path) and from the IPC pipe thread (handleConnectionLost). JUCE Timers are
    // message-thread-only; stopping from another thread corrupts the timer list and
    // crashes the host. timerCallback() stops itself once it sees crashed==true.
}

void SandboxedPluginInstance::openEditorInBridge()
{
    SandboxEditorResult ignored;
    requestBridgeEditor (ignored);
}

bool SandboxedPluginInstance::requestBridgeEditor (SandboxEditorResult& result)
{
    result = {};

    auto activeCoordinator = getActiveCoordinator();

    if (activeCoordinator == nullptr || ! loaded || crashed)
    {
        result.error = "Plugin sandbox is not ready.";
        return false;
    }

    PluginHostMessage reply;

    if (! activeCoordinator->sendMessageAndWaitForAnyReply (PluginHostMessageType::openEditor,
                                                             {},
                                                             reply,
                                                             60000))
    {
        result.error = "Plugin sandbox failed to respond while opening the editor.";
        return false;
    }

    if (reply.type == PluginHostMessageType::editorOpened)
    {
        intptr_t nativeHandle = 0;
        if (PluginHostMessage::decodeEditorOpened (reply.payload, nativeHandle, result.width, result.height))
            result.nativeHandle = (void*) nativeHandle;

        result.success = true;
        return true;
    }

    if (reply.type == PluginHostMessageType::editorOpenFailed)
        result.error = PluginHostMessage::decodeFailure (reply.payload);

    if (result.error.isEmpty())
        result.error = "Failed to open plugin editor in sandbox.";

    DBG ("Sandbox editor open failed for " + description.name + ": " + result.error);
    return false;
}

void SandboxedPluginInstance::closeEditorInBridge()
{
    if (auto activeCoordinator = getActiveCoordinator())
        activeCoordinator->sendMessage (PluginHostMessageType::closeEditor);
}

SandboxedPluginInstance* SandboxedPluginInstance::fromExternalPlugin (te::ExternalPlugin& plugin)
{
    if (auto* instance = plugin.getAudioPluginInstance())
        return dynamic_cast<SandboxedPluginInstance*> (instance);

    return nullptr;
}

bool SandboxedPluginInstance::isSandboxedPlugin (const te::Plugin& plugin)
{
    if (auto* external = dynamic_cast<const te::ExternalPlugin*> (&plugin))
        if (auto* instance = external->getAudioPluginInstance())
            return dynamic_cast<const SandboxedPluginInstance*> (instance) != nullptr;

    return false;
}

} // namespace skeletonhive
