#include "PluginHostProtocol.h"

namespace skeletonhive
{

namespace
{
void writeString (juce::MemoryOutputStream& stream, const juce::String& text)
{
    stream.writeInt (text.length());
    stream.write (text.toRawUTF8(), (size_t) text.getNumBytesAsUTF8());
}

bool readString (juce::MemoryInputStream& stream, juce::String& text)
{
    const int len = stream.readInt();
    if (len < 0 || len > 65536)
        return false;

    juce::HeapBlock<char> buffer ((size_t) len + 1);
    if (stream.read (buffer.getData(), (size_t) len) != len)
        return false;

    buffer[len] = 0;
    text = juce::String::fromUTF8 (buffer.getData(), len);
    return true;
}
} // namespace

juce::MemoryBlock PluginHostMessage::encode (PluginHostMessageType messageType, const juce::MemoryBlock& payloadIn)
{
    juce::MemoryBlock block;
    juce::MemoryOutputStream stream (block, false);
    stream.writeInt ((int) messageType);
    stream.writeInt ((int) payloadIn.getSize());
    if (payloadIn.getSize() > 0)
        stream.write (payloadIn.getData(), payloadIn.getSize());
    return block;
}

bool PluginHostMessage::decode (const juce::MemoryBlock& block, PluginHostMessage& out)
{
    juce::MemoryInputStream stream (block.getData(), block.getSize(), false);
    const int typeValue = stream.readInt();
    const int payloadSize = stream.readInt();

    if (typeValue < (int) PluginHostMessageType::ping || typeValue > (int) PluginHostMessageType::shutdown)
        return false;

    if (payloadSize < 0 || payloadSize > (int) block.getSize())
        return false;

    out.type = (PluginHostMessageType) typeValue;
    out.payload.setSize ((size_t) payloadSize);
    if (payloadSize > 0 && stream.read (out.payload.getData(), (size_t) payloadSize) != payloadSize)
        return false;

    return true;
}

juce::MemoryBlock PluginHostMessage::encodeLoadPlugin (const juce::PluginDescription& desc, const juce::String& sharedMemoryName)
{
    juce::MemoryBlock payload;
    juce::MemoryOutputStream stream (payload, false);
    if (auto xml = desc.createXml())
        writeString (stream, xml->toString (juce::XmlElement::TextFormat()));
    else
        writeString (stream, {});
    writeString (stream, sharedMemoryName);
    return payload;
}

bool PluginHostMessage::decodeLoadPlugin (const juce::MemoryBlock& payload, juce::PluginDescription& desc, juce::String& sharedMemoryName)
{
    juce::MemoryInputStream stream (payload.getData(), payload.getSize(), false);
    juce::String xmlText;
    if (! readString (stream, xmlText))
        return false;
    if (! readString (stream, sharedMemoryName))
        return false;

    if (auto xml = juce::parseXML (xmlText))
        return desc.loadFromXml (*xml);

    return false;
}

juce::MemoryBlock PluginHostMessage::encodePrepare (double sampleRate, int blockSize)
{
    juce::MemoryBlock payload;
    juce::MemoryOutputStream stream (payload, false);
    stream.writeDouble (sampleRate);
    stream.writeInt (blockSize);
    return payload;
}

bool PluginHostMessage::decodePrepare (const juce::MemoryBlock& payload, double& sampleRate, int& blockSize)
{
    juce::MemoryInputStream stream (payload.getData(), payload.getSize(), false);
    sampleRate = stream.readDouble();
    blockSize = stream.readInt();
    return sampleRate > 0.0 && blockSize > 0;
}

juce::MemoryBlock PluginHostMessage::encodeSetParameter (int index, float value)
{
    juce::MemoryBlock payload;
    juce::MemoryOutputStream stream (payload, false);
    stream.writeInt (index);
    stream.writeFloat (value);
    return payload;
}

bool PluginHostMessage::decodeSetParameter (const juce::MemoryBlock& payload, int& index, float& value)
{
    juce::MemoryInputStream stream (payload.getData(), payload.getSize(), false);
    index = stream.readInt();
    value = stream.readFloat();
    return index >= 0;
}

juce::MemoryBlock PluginHostMessage::encodePluginLoaded (const juce::String& pluginName,
                                                         int numInputChannels,
                                                         int numOutputChannels,
                                                         const juce::StringArray& paramNames)
{
    juce::MemoryBlock payload;
    juce::MemoryOutputStream stream (payload, false);
    writeString (stream, pluginName);
    stream.writeInt (numInputChannels);
    stream.writeInt (numOutputChannels);
    stream.writeInt (paramNames.size());
    for (const auto& name : paramNames)
        writeString (stream, name);
    return payload;
}

bool PluginHostMessage::decodePluginLoaded (const juce::MemoryBlock& payload,
                                            juce::String& pluginName,
                                            int& numInputChannels,
                                            int& numOutputChannels,
                                            juce::StringArray& paramNames)
{
    juce::MemoryInputStream stream (payload.getData(), payload.getSize(), false);
    if (! readString (stream, pluginName))
        return false;

    numInputChannels = stream.readInt();
    numOutputChannels = stream.readInt();
    const int count = stream.readInt();
    if (count < 0 || count > 4096)
        return false;

    paramNames.clear();
    for (int i = 0; i < count; ++i)
    {
        juce::String name;
        if (! readString (stream, name))
            return false;
        paramNames.add (name);
    }

    return true;
}

juce::MemoryBlock PluginHostMessage::encodeFailure (const juce::String& error)
{
    juce::MemoryBlock payload;
    juce::MemoryOutputStream stream (payload, false);
    writeString (stream, error);
    return payload;
}

juce::String PluginHostMessage::decodeFailure (const juce::MemoryBlock& payload)
{
    juce::MemoryInputStream stream (payload.getData(), payload.getSize(), false);
    juce::String error;
    readString (stream, error);
    return error;
}

} // namespace skeletonhive
