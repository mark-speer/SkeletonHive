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

void openEditorOnMessageThread (juce::AudioPluginInstance* pluginInstance,
                                std::unique_ptr<juce::AudioProcessorEditor>& pluginEditor,
                                std::unique_ptr<juce::DocumentWindow>& editorWindow,
                                EditorOpenResult& result)
{
    if (pluginInstance == nullptr)
    {
        result.error = "Plugin instance is not available.";
        return;
    }

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
        return;
    }

    if (! pluginInstance->hasEditor())
    {
        result.error = "Plugin reports no editor.";
        return;
    }

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
        pluginEditor = std::move (editor);
        result.success = true;
        return;
    }
   #endif

    editorWindow = std::make_unique<BridgeEditorWindow> (pluginInstance->getName(), editor.release());
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

struct PluginLoadResult
{
    std::unique_ptr<juce::AudioPluginInstance> instance;
    juce::String error;
};

void loadPluginOnMessageThread (juce::AudioPluginFormatManager& formatManager,
                                const juce::PluginDescription& desc,
                                double sampleRate,
                                int blockSize,
                                PluginLoadResult& result)
{
    if (formatManager.getNumFormats() == 0)
        formatManager.addFormat (std::make_unique<juce::VST3PluginFormat>());

    juce::WaitableEvent done;

    juce::MessageManager::callAsync ([&formatManager, &desc, sampleRate, blockSize, &result, &done]
    {
        result.instance = formatManager.createPluginInstance (desc, sampleRate, blockSize, result.error);
        done.signal();
    });

    done.wait (60000);
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

    PluginLoadResult loadResult;
    loadPluginOnMessageThread (formatManager, desc, currentSampleRate, currentBlockSize, loadResult);
    pluginInstance = std::move (loadResult.instance);

    if (pluginInstance == nullptr)
    {
        const auto error = loadResult.error.isNotEmpty() ? loadResult.error : juce::String ("Plugin load failed.");
        sendReply (PluginHostMessageType::pluginLoadFailed,
                   PluginHostMessage::encodeFailure (error));
        return false;
    }

    numInputChannels = juce::jmin (juce::jmax (0, pluginInstance->getTotalNumInputChannels()),
                                   PluginHostConstants::maxChannels);
    numOutputChannels = juce::jmin (juce::jmax (1, pluginInstance->getTotalNumOutputChannels()),
                                    PluginHostConstants::maxChannels);
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
    processBuffer.setSize (juce::jmax (1, juce::jmax (numInputChannels, numOutputChannels)), currentBlockSize);

    const std::scoped_lock lock (pluginInstanceMutex);
    pluginInstance->prepareToPlay (currentSampleRate, currentBlockSize);
    prepared = true;
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

    EditorOpenResult result;
    juce::WaitableEvent done;

    juce::MessageManager::callAsync ([this, &result, &done]
    {
        const std::scoped_lock lock (pluginInstanceMutex);
        openEditorOnMessageThread (pluginInstance.get(), pluginEditor, editorWindow, result);
        done.signal();
    });

    if (! done.wait (60000))
    {
        sendReply (PluginHostMessageType::editorOpenFailed,
                   PluginHostMessage::encodeFailure ("Timed out opening plugin editor."));
        return false;
    }

    if (! result.success)
    {
        sendReply (PluginHostMessageType::editorOpenFailed,
                   PluginHostMessage::encodeFailure (result.error.isNotEmpty() ? result.error
                                                                               : juce::String ("Failed to open editor.")));
        return false;
    }

    sendReply (PluginHostMessageType::editorOpened,
               PluginHostMessage::encodeEditorOpened (result.nativeHandle, result.width, result.height));
    return true;
}

bool PluginHostWorker::handleCloseEditor()
{
    juce::WaitableEvent done;

    juce::MessageManager::callAsync ([this, &done]
    {
        const std::scoped_lock lock (pluginInstanceMutex);
        pluginEditor.reset();
        editorWindow.reset();
        done.signal();
    });

    done.wait (5000);
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

        if (numSamples <= 0)
            continue;

        processBuffer.setSize (juce::jmax (1, juce::jmax (numInputChannels, numOutputChannels)),
                               numSamples, false, false, true);
        processBuffer.clear();
        sharedMemory.readInput (processBuffer, inChannels, numSamples);

        if (inChannels == 1 && numInputChannels > 1)
            processBuffer.copyFrom (1, 0, processBuffer, 0, 0, numSamples);

        if (! prepared || pluginInstance == nullptr)
        {
            sharedMemory.writeOutput (processBuffer, numOutputChannels, numSamples, hostSequenceToProcess);
            continue;
        }

        midiBuffer.clear();

        {
            const std::scoped_lock lock (pluginInstanceMutex);
            pluginInstance->processBlock (processBuffer, midiBuffer);
        }

        sharedMemory.writeOutput (processBuffer, numOutputChannels, numSamples, hostSequenceToProcess);
    }
}

} // namespace skeletonhive
