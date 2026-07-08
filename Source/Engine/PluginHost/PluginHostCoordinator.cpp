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

bool PluginHostCoordinator::launchAndConnect (const juce::File& executable)
{
    if (! sharedMemory.create (sessionId))
        return false;

    if (! launchWorkerProcess (executable, PluginHostConstants::workerUniqueId, 10000))
        return false;

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
