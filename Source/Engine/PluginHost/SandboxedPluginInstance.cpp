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
    closeEditorInBridge();
}

bool SandboxedPluginInstance::initialiseBridge (double sampleRate, int samplesPerBlock, juce::String& errorMessage)
{
    coordinator = std::make_unique<PluginHostCoordinator>();
    coordinator->setInstance (this);

    const auto executable = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    juce::String launchError;

    const auto launchResult = juce::MessageManager::callSync ([&]
    {
        return coordinator->launchAndConnect (executable, launchError);
    });

    if (! launchResult.has_value() || ! *launchResult)
    {
        errorMessage = launchError.isNotEmpty() ? launchError
                                                  : juce::String ("Failed to launch plugin sandbox process.");
        loading = false;
        crashed = true;
        coordinator.reset();
        return false;
    }

    PluginHostMessage reply;
    const auto loadPayload = PluginHostMessage::encodeLoadPlugin (description,
                                                                  coordinator->getSessionId(),
                                                                  sampleRate,
                                                                  samplesPerBlock);

    if (! coordinator->sendMessageAndWaitForAnyReply (PluginHostMessageType::loadPlugin,
                                                      loadPayload,
                                                      reply,
                                                      60000))
    {
        errorMessage = "Plugin sandbox failed to respond.";
        loading = false;
        crashed = coordinator->hasCrashed();
        coordinator->sendMessage (PluginHostMessageType::shutdown);
        coordinator.reset();
        DBG ("Sandbox load timeout: " + description.name);
        return false;
    }

    if (reply.type == PluginHostMessageType::pluginLoadFailed)
    {
        errorMessage = PluginHostMessage::decodeFailure (reply.payload);
        if (errorMessage.isEmpty())
            errorMessage = "Plugin sandbox failed to load the plugin.";
        loading = false;
        coordinator->sendMessage (PluginHostMessageType::shutdown);
        coordinator.reset();
        DBG ("Sandbox load failed for " + description.name + ": " + errorMessage);
        return false;
    }

    if (reply.type != PluginHostMessageType::pluginLoaded)
    {
        errorMessage = "Unexpected plugin sandbox response.";
        loading = false;
        coordinator->sendMessage (PluginHostMessageType::shutdown);
        coordinator.reset();
        DBG ("Sandbox unexpected response for " + description.name);
        return false;
    }

    juce::StringArray paramNames;
    if (! PluginHostMessage::decodePluginLoaded (reply.payload, pluginName, numInputChannels, numOutputChannels, paramNames))
    {
        errorMessage = "Invalid plugin sandbox load response.";
        loading = false;
        coordinator->sendMessage (PluginHostMessageType::shutdown);
        coordinator.reset();
        return false;
    }

    juce::ignoreUnused (paramNames);
    setPlayConfigDetails (numInputChannels, numOutputChannels, sampleRate, samplesPerBlock);
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    processBuffer.setSize (juce::jmax (numInputChannels, numOutputChannels), samplesPerBlock);
    loading = false;
    loaded = true;

    prepareToPlay (sampleRate, samplesPerBlock);

    if (! prepared)
    {
        errorMessage = "Plugin sandbox failed to prepare.";
        loaded = false;
        coordinator->sendMessage (PluginHostMessageType::shutdown);
        coordinator.reset();
        return false;
    }

    return true;
}

void SandboxedPluginInstance::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    processBuffer.setSize (juce::jmax (numInputChannels, numOutputChannels), samplesPerBlock);

    if (coordinator == nullptr || ! loaded || crashed)
        return;

    PluginHostMessage reply;
    prepared = coordinator->sendMessageAndWaitForReply (PluginHostMessageType::prepare,
                                                        PluginHostMessage::encodePrepare (sampleRate, samplesPerBlock),
                                                        PluginHostMessageType::prepared,
                                                        reply,
                                                        10000);
}

void SandboxedPluginInstance::releaseResources()
{
    prepared = false;
}

void SandboxedPluginInstance::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    if (! loaded || crashed || coordinator == nullptr || ! prepared)
    {
        buffer.clear();
        return;
    }

    auto& sharedMemory = coordinator->getSharedMemory();
    sharedMemory.writeInput (buffer);

    if (! sharedMemory.waitForOutput())
    {
        if (coordinator->hasCrashed())
            notifyBridgeCrashed();
        buffer.clear();
        return;
    }

    sharedMemory.readOutput (buffer);
}

void SandboxedPluginInstance::getStateInformation (juce::MemoryBlock& destData)
{
    destData.reset();

    if (coordinator == nullptr || ! loaded || crashed)
        return;

    PluginHostMessage reply;
    if (coordinator->sendMessageAndWaitForReply (PluginHostMessageType::getState,
                                                 {},
                                                 PluginHostMessageType::stateBlob,
                                                 reply,
                                                 5000))
        destData = reply.payload;
}

void SandboxedPluginInstance::setStateInformation (const void* data, int sizeInBytes)
{
    if (coordinator == nullptr || ! loaded || crashed)
        return;

    juce::MemoryBlock payload (data, (size_t) sizeInBytes);
    coordinator->sendMessage (PluginHostMessageType::setState, payload);
}

void SandboxedPluginInstance::notifyBridgeCrashed()
{
    crashed = true;
    loaded = false;
    prepared = false;
}

void SandboxedPluginInstance::openEditorInBridge()
{
    juce::String ignored;
    requestBridgeEditor (ignored);
}

bool SandboxedPluginInstance::requestBridgeEditor (juce::String& errorMessage)
{
    errorMessage.clear();

    if (coordinator == nullptr || ! loaded || crashed)
    {
        errorMessage = "Plugin sandbox is not ready.";
        return false;
    }

    PluginHostMessage reply;

    if (! coordinator->sendMessageAndWaitForAnyReply (PluginHostMessageType::openEditor,
                                                      {},
                                                      reply,
                                                      60000))
    {
        errorMessage = "Plugin sandbox failed to respond while opening the editor.";
        return false;
    }

    if (reply.type == PluginHostMessageType::editorOpened)
        return true;

    if (reply.type == PluginHostMessageType::editorOpenFailed)
        errorMessage = PluginHostMessage::decodeFailure (reply.payload);

    if (errorMessage.isEmpty())
        errorMessage = "Failed to open plugin editor in sandbox.";

    DBG ("Sandbox editor open failed for " + description.name + ": " + errorMessage);
    return false;
}

void SandboxedPluginInstance::closeEditorInBridge()
{
    if (coordinator != nullptr)
        coordinator->sendMessage (PluginHostMessageType::closeEditor);
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
