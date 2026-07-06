#pragma once

#include "Engine/PluginScanner.h"
#include "Engine/PluginStateManager.h"

namespace skeletonhive
{

enum class PluginPickerFilter
{
    all,
    instrumentsOnly,
    effectsOnly
};

/** Modal plugin list for hot-swap, default chains, etc. */
class PluginPickerDialog : public juce::Component
{
public:
    PluginPickerDialog (PluginScanner& scanner,
                        te::Engine& engine,
                        PluginStateManager& stateManager,
                        PluginPickerFilter filter,
                        const juce::String& title);

    std::function<void (const juce::PluginDescription&)> onPluginChosen;

    void resized() override;

    static void show (juce::Component* centreAround,
                      PluginScanner& scanner,
                      te::Engine& engine,
                      PluginStateManager& stateManager,
                      PluginPickerFilter filter,
                      const juce::String& title,
                      std::function<void (const juce::PluginDescription&)> onChosen);

private:
    class PluginListModel;

    void rebuildList();
    juce::Array<juce::PluginDescription> getFilteredPlugins() const;
    void chooseSelected();

    PluginScanner& pluginScanner;
    te::Engine& engineRef;
    PluginStateManager& pluginStateManager;
    PluginPickerFilter pickerFilter;

    juce::Label titleLabel;
    juce::TextEditor searchBox;
    juce::ListBox pluginList;
    juce::TextButton okButton { "OK" };
    juce::TextButton cancelButton { "Cancel" };
    std::unique_ptr<PluginListModel> listModel;
    juce::PluginDescription selectedPlugin;
};

} // namespace skeletonhive
