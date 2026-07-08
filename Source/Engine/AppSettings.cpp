#include "AppSettings.h"

namespace skeletonhive
{

namespace
{
constexpr int defaultAutosaveSeconds = 60;

juce::PropertiesFile::Options makeOptions()
{
    juce::PropertiesFile::Options opts;
    opts.applicationName = "SkeletonHive";
    opts.filenameSuffix = ".settings";
    opts.osxLibrarySubFolder = "Application Support";
    opts.commonToAllUsers = false;
    opts.storageFormat = juce::PropertiesFile::storeAsXML;
    opts.millisecondsBeforeSaving = 500;
    return opts;
}
} // namespace

AppSettings::AppSettings()
    : storageFile (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("SkeletonHive")
                       .getChildFile ("app.settings")),
      properties (storageFile, makeOptions())
{
}

ThemeChoice AppSettings::getTheme() const
{
    return properties.getValue ("theme", "dark") == "light" ? ThemeChoice::light : ThemeChoice::dark;
}

void AppSettings::setTheme (ThemeChoice theme)
{
    properties.setValue ("theme", theme == ThemeChoice::light ? "light" : "dark");
    saveIfNeeded();
    sendChangeMessage();
}

int AppSettings::getAutosaveIntervalSeconds() const
{
    return properties.getIntValue ("autosaveSeconds", defaultAutosaveSeconds);
}

void AppSettings::setAutosaveIntervalSeconds (int seconds)
{
    properties.setValue ("autosaveSeconds", juce::jmax (10, seconds));
    saveIfNeeded();
}

juce::File AppSettings::getDefaultProjectFolder() const
{
    const auto stored = properties.getValue ("defaultProjectFolder");

    if (stored.isNotEmpty())
    {
        const juce::File f (stored);
        if (f.isDirectory())
            return f;
    }

    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("SkeletonHive");
}

void AppSettings::setDefaultProjectFolder (const juce::File& folder)
{
    if (folder.isDirectory())
    {
        properties.setValue ("defaultProjectFolder", folder.getFullPathName());
        saveIfNeeded();
    }
}

juce::Array<juce::File> AppSettings::getSampleLibraryPaths() const
{
    juce::Array<juce::File> paths;
    const auto tokens = juce::StringArray::fromTokens (properties.getValue ("sampleLibraryPaths"), "|", "");

    for (const auto& token : tokens)
    {
        const juce::File f (token.trim());
        if (f.isDirectory())
            paths.add (f);
    }

    return paths;
}

void AppSettings::setSampleLibraryPaths (const juce::Array<juce::File>& paths)
{
    juce::StringArray stored;

    for (const auto& path : paths)
    {
        if (path.isDirectory())
            stored.add (path.getFullPathName());
    }

    properties.setValue ("sampleLibraryPaths", stored.joinIntoString ("|"));
    saveIfNeeded();
    sendChangeMessage();
}

void AppSettings::ensureDefaultSampleLibraryPaths()
{
    if (! getSampleLibraryPaths().isEmpty())
        return;

    juce::Array<juce::File> defaults;
    const auto music = juce::File::getSpecialLocation (juce::File::userMusicDirectory);

    if (music.isDirectory())
        defaults.add (music);

    const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    if (docs.isDirectory())
        defaults.add (docs);

    if (! defaults.isEmpty())
        setSampleLibraryPaths (defaults);
}

namespace
{
juce::String defaultChainProperty (DefaultChainKind kind)
{
    return kind == DefaultChainKind::audioTrack ? "defaultAudioChain" : "defaultMidiChain";
}
} // namespace

juce::StringArray AppSettings::getDefaultDeviceChain (DefaultChainKind kind) const
{
    return juce::StringArray::fromTokens (properties.getValue (defaultChainProperty (kind)), "|", "");
}

void AppSettings::setDefaultDeviceChain (DefaultChainKind kind, const juce::StringArray& pluginIdentifiers)
{
    juce::StringArray stored;

    for (const auto& id : pluginIdentifiers)
    {
        if (id.trim().isNotEmpty())
            stored.add (id.trim());
    }

    properties.setValue (defaultChainProperty (kind), stored.joinIntoString ("|"));
    saveIfNeeded();
    sendChangeMessage();
}

void AppSettings::saveKeyMappings (const juce::ApplicationCommandManager& commandManager)
{
    if (auto* mappings = commandManager.getKeyMappings())
    {
        if (auto xml = mappings->createXml (true))
        {
            properties.setValue ("keyMappings", xml->toString());
            saveIfNeeded();
        }
    }
}

void AppSettings::loadKeyMappings (juce::ApplicationCommandManager& commandManager)
{
    if (auto* mappings = commandManager.getKeyMappings())
    {
        if (auto xml = juce::parseXML (properties.getValue ("keyMappings")))
            mappings->restoreFromXml (*xml);
    }
}

void AppSettings::saveIfNeeded()
{
    properties.saveIfNeeded();
}

bool AppSettings::isPluginSandboxEnabled() const
{
    return properties.getBoolValue ("pluginSandboxEnabled", true);
}

void AppSettings::setPluginSandboxEnabled (bool enabled)
{
    properties.setValue ("pluginSandboxEnabled", enabled);
    saveIfNeeded();
    sendChangeMessage();
}

} // namespace skeletonhive
