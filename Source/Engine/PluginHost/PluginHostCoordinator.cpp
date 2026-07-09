#include "PluginHostCoordinator.h"
#include "PluginHostHelpers.h"
#include "SandboxedPluginInstance.h"

namespace skeletonhive
{

PluginHostCoordinator::PluginHostCoordinator()
{
    sessionId = PluginHostHelpers::makeSessionId();
}

PluginHostCoordinator::~PluginHostCoordinator()
{
    if (connected)
        sendMessage (PluginHostMessageType::shutdown);

    killWorkerProcess();
    sharedMemory.requestShutdown();
    sharedMemory.close();
}

bool PluginHostCoordinator::launchAndConnect (const juce::File& executable, juce::String& errorMessage)
{
    if (! sharedMemory.create (sessionId))
    {
        errorMessage = "Failed to create plugin sandbox shared memory.";
        DBG ("PluginHostCoordinator: shared memory create failed for session " + sessionId);
        return false;
    }

    const auto workerExecutable = executable.existsAsFile() ? executable
                                                            : juce::File::getSpecialLocation (juce::File::currentExecutableFile);

    if (! workerExecutable.existsAsFile())
    {
        errorMessage = "Plugin sandbox executable not found: " + workerExecutable.getFullPathName();
        DBG ("PluginHostCoordinator: executable missing at " + workerExecutable.getFullPathName());
        return false;
    }

    if (! launchWorkerProcess (workerExecutable, PluginHostConstants::workerUniqueId, 30000))
    {
        errorMessage = "Failed to launch plugin sandbox process.";
        DBG ("PluginHostCoordinator: launchWorkerProcess failed for " + workerExecutable.getFullPathName());
        return false;
    }

    connected = true;
    return true;
}

bool PluginHostCoordinator::sendMessage (PluginHostMessageType type, const juce::MemoryBlock& payload)
{
    if (! connected)
        return false;

    return sendMessageToWorker (PluginHostMessage::encode (type, payload));
}

bool PluginHostCoordinator::sendMessageAndWaitForAnyReply (PluginHostMessageType sendType,
                                                           const juce::MemoryBlock& payload,
                                                           PluginHostMessage& reply,
                                                           int timeoutMs)
{
    if (! connected)
        return false;

    {
        const juce::ScopedLock lock (replyLock);
        replyReceived = false;
    }

    if (! sendMessage (sendType, payload))
        return false;

    if (! replyEvent.wait (timeoutMs))
        return false;

    const juce::ScopedLock lock (replyLock);
    if (! replyReceived)
        return false;

    reply = pendingReply;
    return true;
}

bool PluginHostCoordinator::sendMessageAndWaitForReply (PluginHostMessageType sendType,
                                                        const juce::MemoryBlock& payload,
                                                        PluginHostMessageType expectedReply,
                                                        PluginHostMessage& reply,
                                                        int timeoutMs)
{
    if (! connected)
        return false;

    {
        const juce::ScopedLock lock (replyLock);
        expectedReplyType = expectedReply;
        replyReceived = false;
    }

    if (! sendMessage (sendType, payload))
        return false;

    if (! replyEvent.wait (timeoutMs))
        return false;

    const juce::ScopedLock lock (replyLock);
    if (! replyReceived || pendingReply.type != expectedReply)
        return false;

    reply = pendingReply;
    return true;
}

void PluginHostCoordinator::handleConnectionLost()
{
    connected = false;
    crashed = true;
    sharedMemory.requestShutdown();

    if (ownerInstance != nullptr)
        ownerInstance->notifyBridgeCrashed();
}

void PluginHostCoordinator::handleMessageFromWorker (const juce::MemoryBlock& message)
{
    PluginHostMessage decoded;
    if (! PluginHostMessage::decode (message, decoded))
        return;

    const juce::ScopedLock lock (replyLock);
    pendingReply = decoded;
    replyReceived = true;
    expectedReplyType = decoded.type;
    replyEvent.signal();
}

} // namespace skeletonhive
