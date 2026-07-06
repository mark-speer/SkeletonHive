#include "GroovePoolManager.h"

namespace skeletonhive
{

namespace
{
constexpr const char* groovesRootTag = "SkeletonHiveGrooves";
constexpr const char* grooveTag = "GROOVE";

juce::String makeUserTemplateId()
{
    return "user." + juce::Uuid().toString();
}

const juce::Array<GrooveTemplate>& builtInTemplates()
{
    static const juce::Array<GrooveTemplate> templates = GrooveEngine::defaultTemplates();
    return templates;
}
} // namespace

GroovePoolManager::GroovePoolManager() = default;

juce::File GroovePoolManager::sidecarFileForProject (const juce::File& project)
{
    if (project == juce::File())
        return {};

    return project.getParentDirectory()
                  .getChildFile (".skeletonhive")
                  .getChildFile (project.getFileNameWithoutExtension() + ".grooves.xml");
}

void GroovePoolManager::loadForProject (const juce::File& newProjectFile)
{
    projectFile = newProjectFile;
    userTemplates.clear();

    const auto sidecar = sidecarFileForProject (projectFile);

    if (sidecar.existsAsFile())
    {
        const auto xml = juce::parseXML (sidecar);

        if (xml != nullptr)
        {
            const auto root = juce::ValueTree::fromXml (*xml);

            if (root.isValid() && root.hasType (groovesRootTag))
            {
                for (int i = 0; i < root.getNumChildren(); ++i)
                {
                    const auto child = root.getChild (i);

                    if (child.hasType (grooveTag))
                        userTemplates.add (templateFromValueTree (child));
                }
            }
        }
    }

    sendChangeMessage();
}

void GroovePoolManager::saveForProject() const
{
    const auto sidecar = sidecarFileForProject (projectFile);

    if (sidecar == juce::File())
        return;

    sidecar.getParentDirectory().createDirectory();

    juce::ValueTree root (groovesRootTag);

    for (const auto& t : userTemplates)
        root.appendChild (valueTreeFromTemplate (t), nullptr);

    if (auto xml = root.createXml())
        xml->writeTo (sidecar);
}

juce::Array<GrooveTemplate> GroovePoolManager::getAllTemplates() const
{
    auto all = builtInTemplates();

    for (const auto& t : userTemplates)
        all.add (t);

    return all;
}

juce::Array<GrooveTemplate> GroovePoolManager::getUserTemplates() const
{
    return userTemplates;
}

const GrooveTemplate* GroovePoolManager::findById (const juce::String& id) const
{
    for (const auto& t : userTemplates)
        if (t.id == id)
            return &t;

    for (const auto& t : builtInTemplates())
        if (t.id == id)
            return &t;

    return nullptr;
}

int GroovePoolManager::indexOfTemplate (const juce::String& id) const
{
    const auto all = getAllTemplates();

    for (int i = 0; i < all.size(); ++i)
        if (all.getReference (i).id == id)
            return i;

    return -1;
}

void GroovePoolManager::setSelectedGrooveId (const juce::String& id)
{
    if (findById (id) != nullptr)
        selectedGrooveId = id;
}

const GrooveTemplate* GroovePoolManager::getSelectedGroove() const
{
    return findById (selectedGrooveId);
}

GrooveTemplate GroovePoolManager::addTemplate (const juce::String& name)
{
    GrooveTemplate t;
    t.id = makeUserTemplateId();
    t.name = name;
    userTemplates.add (t);
    saveForProject();
    sendChangeMessage();
    return t;
}

bool GroovePoolManager::removeTemplate (const juce::String& id)
{
    for (int i = 0; i < userTemplates.size(); ++i)
    {
        if (userTemplates.getReference (i).id == id)
        {
            userTemplates.remove (i);
            saveForProject();
            sendChangeMessage();
            return true;
        }
    }

    return false;
}

bool GroovePoolManager::renameTemplate (const juce::String& id, const juce::String& newName)
{
    for (auto& t : userTemplates)
    {
        if (t.id == id)
        {
            t.name = newName;
            saveForProject();
            sendChangeMessage();
            return true;
        }
    }

    return false;
}

GrooveTemplate GroovePoolManager::duplicateTemplate (const juce::String& id)
{
    if (const auto* source = findById (id))
    {
        GrooveTemplate copy = *source;
        copy.id = makeUserTemplateId();
        copy.name = source->name + " Copy";
        copy.isBuiltIn = false;
        userTemplates.add (copy);
        saveForProject();
        sendChangeMessage();
        return copy;
    }

    return {};
}

GrooveTemplate GroovePoolManager::templateFromValueTree (const juce::ValueTree& tree)
{
    GrooveTemplate t;
    t.id = tree.getProperty ("id").toString();
    t.name = tree.getProperty ("name").toString();
    t.isRandom = (bool) tree.getProperty ("isRandom", false);

    if (auto* timingProp = tree.getPropertyPointer ("timing"))
    {
        juce::StringArray parts;
        parts.addTokens (timingProp->toString(), ",", "");
        for (int i = 0; i < 16 && i < parts.size(); ++i)
            t.timing[(size_t) i] = parts[i].getDoubleValue();
    }

    if (auto* velocityProp = tree.getPropertyPointer ("velocity"))
    {
        juce::StringArray parts;
        parts.addTokens (velocityProp->toString(), ",", "");
        for (int i = 0; i < 16 && i < parts.size(); ++i)
            t.velocity[(size_t) i] = parts[i].getIntValue();
    }

    return t;
}

juce::ValueTree GroovePoolManager::valueTreeFromTemplate (const GrooveTemplate& t)
{
    juce::ValueTree tree (grooveTag);
    tree.setProperty ("id", t.id, nullptr);
    tree.setProperty ("name", t.name, nullptr);
    tree.setProperty ("isRandom", t.isRandom, nullptr);

    juce::String timingStr, velocityStr;

    for (int i = 0; i < 16; ++i)
    {
        if (i > 0)
        {
            timingStr << ",";
            velocityStr << ",";
        }

        timingStr << juce::String (t.timing[(size_t) i], 4);
        velocityStr << t.velocity[(size_t) i];
    }

    tree.setProperty ("timing", timingStr, nullptr);
    tree.setProperty ("velocity", velocityStr, nullptr);
    return tree;
}

} // namespace skeletonhive
