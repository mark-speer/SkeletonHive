#pragma once

#include "GrooveEngine.h"

namespace skeletonhive
{

/** Project-wide groove template pool: built-ins plus user templates persisted per project. */
class GroovePoolManager : public juce::ChangeBroadcaster
{
public:
    GroovePoolManager();

    void loadForProject (const juce::File& projectFile);
    void saveForProject() const;

    juce::Array<GrooveTemplate> getAllTemplates() const;
    juce::Array<GrooveTemplate> getUserTemplates() const;

    void setSelectedGrooveId (const juce::String& id);
    juce::String getSelectedGrooveId() const { return selectedGrooveId; }
    const GrooveTemplate* getSelectedGroove() const;

    const GrooveTemplate* findById (const juce::String& id) const;
    int indexOfTemplate (const juce::String& id) const;

    GrooveTemplate addTemplate (const juce::String& name);
    bool removeTemplate (const juce::String& id);
    bool renameTemplate (const juce::String& id, const juce::String& newName);
    GrooveTemplate duplicateTemplate (const juce::String& id);

    int getRandomTemplateIndex() const { return GrooveEngine::randomGrooveIndex; }

private:
    static juce::File sidecarFileForProject (const juce::File& projectFile);
    static GrooveTemplate templateFromValueTree (const juce::ValueTree& tree);
    static juce::ValueTree valueTreeFromTemplate (const GrooveTemplate& t);

    juce::File projectFile;
    juce::Array<GrooveTemplate> userTemplates;
    juce::String selectedGrooveId { "builtin.random" };
};

} // namespace skeletonhive
