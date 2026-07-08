#include "PluginHostWorker.h"

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
                          juce::Colours::darkgrey,
                          DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (content, true);
        centreWithSize (800, 600);
        setVisible (true);
    }

    void closeButtonPressed() override { setVisible (false); }
};

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
} // namespace

bool PluginHostWorker::tryInitialiseFromCommandLine (const juce::String& commandLine)
{
    static PluginHostWorker worker;
    activeWorker = &worker;

    if (worker.initialiseFromCommandLine (commandLine, PluginHostConstants::workerUniqueId, 10000))
    {
        juce::MessageManager::getInstance()->runDispatchLoop();
        return true;
    }

    activeWorker = nullptr;
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

    if (! PluginHostMessage::decodeLoadPlugin (payload, desc, sharedMemoryName))
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

    formatManager.addFormat (std::make_unique<juce::VST3PluginFormat>());
    juce::String error;

    pluginInstance = formatManager.createPluginInstance (desc, 44100.0, 512, error);

    if (pluginInstance == nullptr)
    {
        sendReply (PluginHostMessageType::pluginLoadFailed,
                   PluginHostMessage::encodeFailure (error.isNotEmpty() ? error : "Plugin load failed."));
        return false;
    }

    numInputChannels = juce::jmax (1, pluginInstance->getTotalNumInputChannels());
    numOutputChannels = juce::jmax (1, pluginInstance->getTotalNumOutputChannels());
    processBuffer.setSize (juce::jmax (numInputChannels, numOutputChannels), PluginHostConstants::maxBlockSize);
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
    processBuffer.setSize (juce::jmax (numInputChannels, numOutputChannels), currentBlockSize);
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

    pluginInstance->setParameter (index, value);
    return true;
}

bool PluginHostWorker::handleGetState()
{
    if (pluginInstance == nullptr)
        return false;

    juce::MemoryBlock state;
    pluginInstance->getStateInformation (state);
    sendReply (PluginHostMessageType::stateBlob, state);
    return true;
}

bool PluginHostWorker::handleSetState (const juce::MemoryBlock& payload)
{
    if (pluginInstance == nullptr)
        return false;

    pluginInstance->setStateInformation (payload.getData(), (int) payload.getSize());
    return true;
}

bool PluginHostWorker::handleOpenEditor()
{
    if (pluginInstance == nullptr || ! pluginInstance->hasEditor())
        return false;

    juce::MessageManager::callAsync ([this]
    {
        if (pluginInstance == nullptr)
            return;

        pluginEditor.reset (pluginInstance->createEditorIfNeeded());

        if (pluginEditor == nullptr)
            return;

        editorWindow = std::make_unique<BridgeEditorWindow> (pluginInstance->getName(), pluginEditor.release());
        sendReply (PluginHostMessageType::editorOpened);
    });

    return true;
}

bool PluginHostWorker::handleCloseEditor()
{
    juce::MessageManager::callAsync ([this]
    {
        editorWindow.reset();
        pluginEditor.reset();
        sendReply (PluginHostMessageType::editorClosed);
    });
    return true;
}

void PluginHostWorker::handleShutdown()
{
    stopAudioLoop();
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

void PluginHostWorker::stopAudioLoop()
{
    audioRunning = false;
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
        if (! sharedMemory.waitForInput())
            continue;

        if (! prepared || pluginInstance == nullptr)
            continue;

        const int numSamples = (int) sharedMemory.getHeader()->numSamples.load (std::memory_order_acquire);
        const int inChannels = (int) sharedMemory.getHeader()->numInputChannels.load (std::memory_order_acquire);

        sharedMemory.readInput (processBuffer, inChannels, numSamples);
        midiBuffer.clear();
        pluginInstance->processBlock (processBuffer, midiBuffer);
        sharedMemory.writeOutput (processBuffer, numOutputChannels, numSamples);
    }
}

} // namespace skeletonhive
