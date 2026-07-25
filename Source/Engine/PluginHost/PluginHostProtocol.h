#pragma once

#include "TracktionCommon.h"
#include "PluginHostConstants.h"

namespace skeletonhive
{

enum class PluginHostMessageType : uint32_t
{
    ping = 1,
    pong,
    loadPlugin,
    pluginLoaded,
    pluginLoadFailed,
    prepare,
    prepared,
    setParameter,
    getState,
    setState,
    stateBlob,
    openEditor,
    closeEditor,
    editorOpened,
    editorOpenFailed,
    editorClosed,
    shutdown
};

struct PluginHostMessage
{
    PluginHostMessageType type = PluginHostMessageType::ping;
    juce::MemoryBlock payload;

    static juce::MemoryBlock encode (PluginHostMessageType messageType, const juce::MemoryBlock& payloadIn = {});
    static bool decode (const juce::MemoryBlock& block, PluginHostMessage& out);

    static juce::MemoryBlock encodeLoadPlugin (const juce::PluginDescription& desc,
                                               const juce::String& sharedMemoryName,
                                               double sampleRate,
                                               int blockSize);
    static bool decodeLoadPlugin (const juce::MemoryBlock& payload,
                                  juce::PluginDescription& desc,
                                  juce::String& sharedMemoryName,
                                  double& sampleRate,
                                  int& blockSize);

    static juce::MemoryBlock encodePrepare (double sampleRate, int blockSize);
    static bool decodePrepare (const juce::MemoryBlock& payload, double& sampleRate, int& blockSize);

    static juce::MemoryBlock encodeSetParameter (int index, float value);
    static bool decodeSetParameter (const juce::MemoryBlock& payload, int& index, float& value);

    static juce::MemoryBlock encodePluginLoaded (const juce::String& pluginName,
                                                 int numInputChannels,
                                                 int numOutputChannels,
                                                 const juce::StringArray& paramNames);
    static bool decodePluginLoaded (const juce::MemoryBlock& payload,
                                    juce::String& pluginName,
                                    int& numInputChannels,
                                    int& numOutputChannels,
                                    juce::StringArray& paramNames);

    static juce::MemoryBlock encodeFailure (const juce::String& error);
    static juce::String decodeFailure (const juce::MemoryBlock& payload);

    static juce::MemoryBlock encodeEditorOpened (intptr_t nativeHandle, int width, int height);
    static bool decodeEditorOpened (const juce::MemoryBlock& payload,
                                    intptr_t& nativeHandle,
                                    int& width,
                                    int& height);
};

} // namespace skeletonhive
