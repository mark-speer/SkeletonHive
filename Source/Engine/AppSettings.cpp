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

} // namespace skeletonhive
