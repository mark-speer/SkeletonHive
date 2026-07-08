#pragma once

#include "PluginHostConstants.h"
#include "PluginHostProtocol.h"
#include "PluginHostSharedMemory.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

class SandboxedPluginInstance;

/** Host-side bridge process connection for one sandboxed plugin instance. */
class PluginHostCoordinator : public juce::ChildProcessCoordinator
{
public:
    PluginHostCoordinator();
    ~PluginHostCoordinator() override;

    bool launchAndConnect (const juce::File& executable);
    bool sendMessage (PluginHostMessageType type, const juce::MemoryBlock& payload = {});
    bool sendMessageAndWaitForReply (PluginHostMessageType sendType,
                                     const juce::MemoryBlock& payload,
                                     PluginHostMessageType expectedReply,
                                     PluginHostMessage& reply,
                                     int timeoutMs = 15000);

    bool sendMessageAndWaitForAnyReply (PluginHostMessageType sendType,
                                        const juce::MemoryBlock& payload,
                                        PluginHostMessage& reply,
                                        int timeoutMs = 15000);

    PluginHostSharedMemory& getSharedMemory() { return sharedMemory; }
    const juce::String& getSessionId() const { return sessionId; }

    bool isConnected() const { return connected; }
    bool hasCrashed() const { return crashed; }

    void setInstance (SandboxedPluginInstance* instance) { ownerInstance = instance; }

private:
    void handleConnectionLost() override;
    void handleMessageFromWorker (const juce::MemoryBlock& message) override;

    juce::CriticalSection replyLock;
    juce::WaitableEvent replyEvent;
    PluginHostMessage pendingReply;
    PluginHostMessageType expectedReplyType = PluginHostMessageType::ping;
    bool replyReceived = false;

    SandboxedPluginInstance* ownerInstance = nullptr;
    PluginHostSharedMemory sharedMemory;
    juce::String sessionId;
    bool connected = false;
    bool crashed = false;
};

} // namespace skeletonhive
