#include "PluginHostWorker.h"

#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace skeletonhive
{

namespace
{
PluginHostWorker* activeWorker = nullptr;

class BridgeEditorWindow : public juce::DocumentWindow
{
public:
    BridgeEditorWindow (const juce::String& name, juce::Component* content)
        : DocumentWindow (name + " (Sandboxed)",
                          juce::Desktop::getInstance().getDefaultLookAndFeel()
                              .findColour (juce::ResizableWindow::backgroundColourId),
                          DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (content, true);

        const int editorW = juce::jmax (content->getWidth(), 400);
        const int editorH = juce::jmax (content->getHeight(), 300);
        centreWithSize (editorW, editorH);
        setResizable (true, false);
        setVisible (true);
        toFront (true);
    }

    void showEditor()
    {
        setVisible (true);
        toFront (true);
    }

    void closeButtonPressed() override
    {
        setVisible (false);
    }
};

struct EditorOpenResult
{
    bool success = false;
    juce::String error;
    intptr_t nativeHandle = 0;
    int width = 0;
    int height = 0;
};

struct AsyncEditorOpenState
{
    std::atomic<bool> cancelled { false };
    juce::WaitableEvent done;
    EditorOpenResult result;
};

struct AsyncLoadState
{
    std::atomic<bool> cancelled { false };
    juce::WaitableEvent done;
    std::unique_ptr<juce::AudioPluginInstance> instance;
    juce::String error;
};

struct AsyncCloseState
{
    std::atomic<bool> cancelled { false };
    juce::WaitableEvent done;
};

#if JUCE_WINDOWS
HWND findEmbeddableHwnd (HWND root)
{
    if (root == nullptr || ! IsWindow (root))
        return nullptr;

    RECT rect {};
    if (GetClientRect (root, &rect))
    {
        const int w = rect.right - rect.left;
        const int h = rect.bottom - rect.top;

        if (w > 50 && h > 50)
            return root;
    }

    HWND firstChild = nullptr;

    EnumChildWindows (root,
                      [] (HWND hwnd, LPARAM lParam) -> BOOL
                      {
                          *reinterpret_cast<HWND*> (lParam) = hwnd;
                          return FALSE;
                      },
                      reinterpret_cast<LPARAM> (&firstChild));

    return firstChild;
}

intptr_t getEditorNativeHandle (juce::AudioProcessorEditor& editor)
{
    if (auto* peer = editor.getPeer())
    {
        if (auto hwnd = findEmbeddableHwnd ((HWND) peer->getNativeHandle()))
            return (intptr_t) hwnd;
    }

    return 0;
}
#endif

void fillExistingEditorResult (juce::AudioProcessorEditor* pluginEditor,
                               juce::DocumentWindow* editorWindow,
                               EditorOpenResult& result)
{
    if (pluginEditor != nullptr)
    {
       #if JUCE_WINDOWS
        result.nativeHandle = getEditorNativeHandle (*pluginEditor);
        result.width = pluginEditor->getWidth();
        result.height = pluginEditor->getHeight();
       #endif
        result.success = true;
        return;
    }

    if (editorWindow != nullptr)
    {
        editorWindow->setVisible (true);
        editorWindow->toFront (true);
        result.success = true;
    }
}

/** Create the editor while DSP is suspended. Only the unique_ptr ownership swap
    is guarded by pluginInstanceMutex — never hold that mutex across createEditor.
*/
void openEditorOnMessageThread (juce::AudioPluginInstance* pluginInstance,
                                std::unique_ptr<juce::AudioProcessorEditor>& pluginEditor,
                                std::unique_ptr<juce::DocumentWindow>& editorWindow,
                                std::mutex& pluginInstanceMutex,
                                EditorOpenResult& result)
{
    if (pluginInstance == nullptr)
    {
        result.error = "Plugin instance is not available.";
        return;
    }

    {
        const std::scoped_lock lock (pluginInstanceMutex);
        if (pluginEditor != nullptr || editorWindow != nullptr)
        {
            fillExistingEditorResult (pluginEditor.get(), editorWindow.get(), result);
            return;
        }
    }

    if (! pluginInstance->hasEditor())
    {
        result.error = "Plugin reports no editor.";
        return;
    }

    // Long / plugin-owned work: DSP is already suspended so processBlock will not run.
    std::unique_ptr<juce::AudioProcessorEditor> editor (pluginInstance->createEditorAndMakeActive());

    if (editor == nullptr)
    {
        result.error = "Failed to create plugin editor.";
        DBG ("PluginHostWorker: editor creation failed for " + pluginInstance->getName());
        return;
    }

   #if JUCE_WINDOWS
    editor->setVisible (true);

    if (editor->getPeer() == nullptr)
        editor->addToDesktop (juce::ComponentPeer::windowIsTemporary);

    const auto nativeHandle = getEditorNativeHandle (*editor);

    if (nativeHandle != 0)
    {
        result.width = juce::jmax (editor->getWidth(), 400);
        result.height = juce::jmax (editor->getHeight(), 300);
        result.nativeHandle = nativeHandle;

        const std::scoped_lock lock (pluginInstanceMutex);
        pluginEditor = std::move (editor);
        result.success = true;
        return;
    }
   #endif

    auto window = std::make_unique<BridgeEditorWindow> (pluginInstance->getName(), editor.release());

    const std::scoped_lock lock (pluginInstanceMutex);
    editorWindow = std::move (window);
    result.success = true;
}

class PluginHostAudioThread : public juce::Thread
{
public:
    PluginHostAudioThread() : Thread ("PluginHostAudio") {}

    void run() override
    {
        if (activeWorker != nullptr)
            activeWorker->audioLoop();
    }
};

void loadPluginOnMessageThread (juce::AudioPluginFormatManager& formatManager,
                                const juce::PluginDescription& desc,
                                double sampleRate,
                                int blockSize,
                                std::unique_ptr<juce::AudioPluginInstance>& instanceOut,
                                juce::String& errorOut)
{
    if (formatManager.getNumFormats() == 0)
        formatManager.addFormat (std::make_unique<juce::VST3PluginFormat>());

    // Heap-backed state so a wait timeout cannot UAF stack locals from the async lambda.
    auto state = std::make_shared<AsyncLoadState>();
    const auto descCopy = desc;

    juce::MessageManager::callAsync ([&formatManager, descCopy, sampleRate, blockSize, state]
    {
        if (! state->cancelled.load (std::memory_order_acquire))
            state->instance = formatManager.createPluginInstance (descCopy, sampleRate, blockSize, state->error);

        state->done.signal();
    });

    if (! state->done.wait (60000))
    {
        state->cancelled.store (true, std::memory_order_release);
        // Wait for the lambda to exit so we never move instance concurrently.
        state->done.wait (-1);
    }

    instanceOut = std::move (state->instance);
    errorOut = state->error;
}

void clearProcessBufferRegion (juce::AudioBuffer<float>& buffer, int numSamples)
{
    const int samples = juce::jmin (numSamples, buffer.getNumSamples());

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, samples);
}
} // namespace

bool PluginHostWorker::tryInitialiseFromCommandLine (const juce::String& commandLine)
{
    const auto workerPrefix = juce::String ("--") + PluginHostConstants::workerUniqueId + ":";
    const bool isWorkerCommand = commandLine.contains (workerPrefix);

    static PluginHostWorker worker;
    activeWorker = &worker;

    if (worker.initialiseFromCommandLine (commandLine, PluginHostConstants::workerUniqueId, 30000))
    {
        juce::MessageManager::getInstance()->runDispatchLoop();
        return true;
    }

    activeWorker = nullptr;

    if (isWorkerCommand)
        juce::JUCEApplication::getInstance()->systemRequestedQuit();

    return false;
}

void PluginHostWorker::handleConnectionLost()
{
    handleShutdown();
}

void PluginHostWorker::handleMessageFromCoordinator (const juce::MemoryBlock& message)
{
    PluginHostMessage decoded;
    if (! PluginHostMessage::decode (message, decoded))
        return;

    switch (decoded.type)
    {
        case PluginHostMessageType::ping:           handlePing(); break;
        case PluginHostMessageType::loadPlugin:     handleLoadPlugin (decoded.payload); break;
        case PluginHostMessageType::prepare:        handlePrepare (decoded.payload); break;
        case PluginHostMessageType::setParameter:   handleSetParameter (decoded.payload); break;
        case PluginHostMessageType::getState:       handleGetState(); break;
        case PluginHostMessageType::setState:       handleSetState (decoded.payload); break;
        case PluginHostMessageType::openEditor:     handleOpenEditor(); break;
        case PluginHostMessageType::closeEditor:   handleCloseEditor(); break;
        case PluginHostMessageType::shutdown:       handleShutdown(); break;
        default: break;
    }
}

void PluginHostWorker::sendReply (PluginHostMessageType type, const juce::MemoryBlock& payload)
{
    sendMessageToCoordinator (PluginHostMessage::encode (type, payload));
}

void PluginHostWorker::suspendProcessing()
{
    processingSuspended.store (true, std::memory_order_release);

    const auto deadline = juce::Time::getMillisecondCounter() + 100;

    while (processBlockActive.load (std::memory_order_acquire)
           && juce::Time::getMillisecondCounter() < deadline)
        juce::Thread::sleep (1);
}

void PluginHostWorker::resumeProcessing()
{
    processingSuspended.store (false, std::memory_order_release);
}

bool PluginHostWorker::handlePing()
{
    sendReply (PluginHostMessageType::pong);
    return true;
}

bool PluginHostWorker::handleLoadPlugin (const juce::MemoryBlock& payload)
{
    juce::PluginDescription desc;
    juce::String sharedMemoryName;
    double sampleRate = 44100.0;
    int blockSize = 512;

    if (! PluginHostMessage::decodeLoadPlugin (payload, desc, sharedMemoryName, sampleRate, blockSize))
    {
        sendReply (PluginHostMessageType::pluginLoadFailed,
                   PluginHostMessage::encodeFailure ("Invalid load request."));
        return false;
    }

    sessionId = sharedMemoryName;

    if (! sharedMemory.openExisting (sharedMemoryName))
    {
        sendReply (PluginHostMessageType::pluginLoadFailed,
                   PluginHostMessage::encodeFailure ("Failed to open shared memory."));
        return false;
    }

    currentSampleRate = sampleRate;
    currentBlockSize = juce::jmin (blockSize, PluginHostConstants::maxBlockSize);

    juce::String loadError;
    loadPluginOnMessageThread (formatManager, desc, currentSampleRate, currentBlockSize,
                               pluginInstance, loadError);

    if (pluginInstance == nullptr)
    {
        const auto error = loadError.isNotEmpty() ? loadError : juce::String ("Plugin load failed.");
        sendReply (PluginHostMessageType::pluginLoadFailed,
                   PluginHostMessage::encodeFailure (error));
        return false;
    }

    numInputChannels = juce::jmin (juce::jmax (0, pluginInstance->getTotalNumInputChannels()),
                                   PluginHostConstants::maxChannels);
    numOutputChannels = juce::jmin (juce::jmax (1, pluginInstance->getTotalNumOutputChannels()),
                                    PluginHostConstants::maxChannels);
    // Pre-size once to the transport maximum so the audio loop never reallocates.
    processBuffer.setSize (juce::jmax (1, juce::jmax (numInputChannels, numOutputChannels)),
                           PluginHostConstants::maxBlockSize);
    pluginInstance->enableAllBuses();
    pluginLoaded = true;

    if (auto* header = sharedMemory.getHeader())
        header->workerReady.store (1, std::memory_order_release);

    juce::StringArray paramNames;
    for (int i = 0; i < pluginInstance->getNumParameters(); ++i)
        paramNames.add (pluginInstance->getParameterName (i));

    sendReply (PluginHostMessageType::pluginLoaded,
               PluginHostMessage::encodePluginLoaded (pluginInstance->getName(),
                                                        numInputChannels,
                                                        numOutputChannels,
                                                        paramNames));
    startAudioLoop();
    return true;
}

bool PluginHostWorker::handlePrepare (const juce::MemoryBlock& payload)
{
    if (! pluginLoaded || pluginInstance == nullptr)
        return false;

    if (! PluginHostMessage::decodePrepare (payload, currentSampleRate, currentBlockSize))
        return false;

    currentBlockSize = juce::jmin (currentBlockSize, PluginHostConstants::maxBlockSize);

    ScopedProcessingSuspend suspend (*this);

    // Keep capacity at maxBlockSize; never shrink under a live audio thread.
    processBuffer.setSize (juce::jmax (1, juce::jmax (numInputChannels, numOutputChannels)),
                           PluginHostConstants::maxBlockSize,
                           false, false, true);

    {
        const std::scoped_lock lock (pluginInstanceMutex);
        pluginInstance->prepareToPlay (currentSampleRate, currentBlockSize);
        prepared = true;
    }

    sendReply (PluginHostMessageType::prepared);
    return true;
}

bool PluginHostWorker::handleSetParameter (const juce::MemoryBlock& payload)
{
    if (pluginInstance == nullptr)
        return false;

    int index = 0;
    float value = 0.0f;
    if (! PluginHostMessage::decodeSetParameter (payload, index, value))
        return false;

    const std::scoped_lock lock (pluginInstanceMutex);
    pluginInstance->setParameter (index, value);
    return true;
}

bool PluginHostWorker::handleGetState()
{
    if (pluginInstance == nullptr)
        return false;

    juce::MemoryBlock state;
    {
        ScopedProcessingSuspend suspend (*this);
        const std::scoped_lock lock (pluginInstanceMutex);
        pluginInstance->getStateInformation (state);
    }
    sendReply (PluginHostMessageType::stateBlob, state);
    return true;
}

bool PluginHostWorker::handleSetState (const juce::MemoryBlock& payload)
{
    if (pluginInstance == nullptr)
        return false;

    ScopedProcessingSuspend suspend (*this);
    const std::scoped_lock lock (pluginInstanceMutex);
    pluginInstance->setStateInformation (payload.getData(), (int) payload.getSize());
    return true;
}

bool PluginHostWorker::handleOpenEditor()
{
    if (pluginInstance == nullptr)
    {
        sendReply (PluginHostMessageType::editorOpenFailed,
                   PluginHostMessage::encodeFailure ("Plugin is not loaded."));
        return false;
    }

    auto state = std::make_shared<AsyncEditorOpenState>();

    juce::MessageManager::callAsync ([this, state]
    {
        if (state->cancelled.load (std::memory_order_acquire))
        {
            state->done.signal();
            return;
        }

        ScopedProcessingSuspend suspend (*this);

        juce::AudioPluginInstance* instance = nullptr;
        {
            const std::scoped_lock lock (pluginInstanceMutex);
            instance = pluginInstance.get();
        }

        openEditorOnMessageThread (instance,
                                   pluginEditor,
                                   editorWindow,
                                   pluginInstanceMutex,
                                   state->result);
        state->done.signal();
    });

    if (! state->done.wait (60000))
    {
        state->cancelled.store (true, std::memory_order_release);
        state->done.wait (-1);
        sendReply (PluginHostMessageType::editorOpenFailed,
                   PluginHostMessage::encodeFailure ("Timed out opening plugin editor."));
        return false;
    }

    if (! state->result.success)
    {
        sendReply (PluginHostMessageType::editorOpenFailed,
                   PluginHostMessage::encodeFailure (state->result.error.isNotEmpty() ? state->result.error
                                                                                       : juce::String ("Failed to open editor.")));
        return false;
    }

    sendReply (PluginHostMessageType::editorOpened,
               PluginHostMessage::encodeEditorOpened (state->result.nativeHandle,
                                                      state->result.width,
                                                      state->result.height));
    return true;
}

bool PluginHostWorker::handleCloseEditor()
{
    auto state = std::make_shared<AsyncCloseState>();

    juce::MessageManager::callAsync ([this, state]
    {
        if (state->cancelled.load (std::memory_order_acquire))
        {
            state->done.signal();
            return;
        }

        ScopedProcessingSuspend suspend (*this);

        {
            const std::scoped_lock lock (pluginInstanceMutex);
            pluginEditor.reset();
            editorWindow.reset();
        }

        state->done.signal();
    });

    if (! state->done.wait (5000))
    {
        state->cancelled.store (true, std::memory_order_release);
        state->done.wait (-1);
    }

    sendReply (PluginHostMessageType::editorClosed);
    return true;
}

void PluginHostWorker::handleShutdown()
{
    stopAudioLoop (true);
    handleCloseEditor();
    sharedMemory.requestShutdown();

    if (pluginInstance != nullptr)
    {
        pluginInstance->releaseResources();
        pluginInstance.reset();
    }

    sharedMemory.close();
    juce::MessageManager::getInstance()->stopDispatchLoop();
}

void PluginHostWorker::startAudioLoop()
{
    if (audioRunning.exchange (true))
        return;

    audioThread = std::make_unique<PluginHostAudioThread>();
    audioThread->startThread (juce::Thread::Priority::high);
}

void PluginHostWorker::stopAudioLoop (bool requestShutdownFlag)
{
    audioRunning = false;

    if (requestShutdownFlag)
        sharedMemory.requestShutdown();

    if (audioThread != nullptr)
    {
        audioThread->stopThread (2000);
        audioThread.reset();
    }
}

void PluginHostWorker::audioLoop()
{
    while (audioRunning && ! sharedMemory.isShutdownRequested())
    {
        uint32_t hostSequenceToProcess = 0;

        if (! sharedMemory.waitForInput (hostSequenceToProcess))
            continue;

        const int numSamples = (int) sharedMemory.getHeader()->numSamples.load (std::memory_order_acquire);
        const int inChannels = juce::jmin ((int) sharedMemory.getHeader()->numInputChannels.load (std::memory_order_acquire),
                                           numInputChannels);

        if (numSamples <= 0 || numSamples > PluginHostConstants::maxBlockSize)
            continue;

        // processBuffer is pre-sized to maxBlockSize at load — never realloc here.
        clearProcessBufferRegion (processBuffer, numSamples);
        sharedMemory.readInput (processBuffer, inChannels, numSamples);

        if (inChannels == 1 && numInputChannels > 1)
            processBuffer.copyFrom (1, 0, processBuffer, 0, 0, numSamples);

        const bool suspended = processingSuspended.load (std::memory_order_acquire);

        if (! suspended && prepared && pluginInstance != nullptr)
        {
            midiBuffer.clear();

            // try_lock so a stuck message-thread op can never starve the SHM handshake;
            // on contention we keep the dry input already in processBuffer.
            if (pluginInstanceMutex.try_lock())
            {
                processBlockActive.store (true, std::memory_order_release);
                pluginInstance->processBlock (processBuffer, midiBuffer);
                processBlockActive.store (false, std::memory_order_release);
                pluginInstanceMutex.unlock();
            }
        }

        sharedMemory.writeOutput (processBuffer, numOutputChannels, numSamples, hostSequenceToProcess);
    }
}

} // namespace skeletonhive
