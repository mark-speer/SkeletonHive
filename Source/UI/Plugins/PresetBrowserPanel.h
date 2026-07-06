#pragma once

#include "Engine/PluginPresetManager.h"

namespace skeletonhive
{

/** Per-device preset browser with session A/B compare. */
class PresetBrowserPanel : public juce::Component
{
public:
    PresetBrowserPanel (te::Plugin& pluginRef);

    std::function<void()> onPluginChanged;

    void resized() override;

    static void showForPlugin (te::Plugin& plugin, juce::Component* target,
                               std::function<void()> onChanged = {});

private:
    class PresetListModel;

    void refreshCategories();
    void refreshPresets();
    void loadSelectedPreset();
    void savePresetAs();
    void deleteSelectedPreset();
    void captureSlot (bool slotA);
    void applySlot (bool slotA);
    void copyAToB();
    void updateAbLabels();

    te::Plugin& plugin;
    juce::String pluginIdentifier;

    juce::Label titleLabel;
    juce::ComboBox categoryFilter;
    juce::ListBox presetList;
    juce::TextButton loadButton { "Load" };
    juce::TextButton saveButton { "Save As..." };
    juce::TextButton deleteButton { "Delete" };
    juce::TextButton slotAButton { "A" };
    juce::TextButton slotBButton { "B" };
    juce::TextButton copyAButton { "Copy A→B" };
    juce::Label abStatusLabel;

    std::unique_ptr<PresetListModel> listModel;
    juce::Array<PluginPresetEntry> presets;
    int selectedRow = -1;

    juce::ValueTree stateA { "PresetSlotA" };
    juce::ValueTree stateB { "PresetSlotB" };
    bool hasStateA = false;
    bool hasStateB = false;
    bool activeSlotA = true;
};

} // namespace skeletonhive
