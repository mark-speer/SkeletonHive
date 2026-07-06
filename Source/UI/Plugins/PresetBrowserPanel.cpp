#include "PresetBrowserPanel.h"
#include "Engine/EngineHelpers.h"

namespace skeletonhive
{

class PresetBrowserPanel::PresetListModel : public juce::ListBoxModel
{
public:
    explicit PresetListModel (PresetBrowserPanel& owner) : panel (owner) {}

    int getNumRows() override { return panel.presets.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool rowSelected) override
    {
        if (! juce::isPositiveAndBelow (row, panel.presets.size()))
            return;

        const auto& preset = panel.presets.getReference (row);

        if (rowSelected)
            g.fillAll (juce::Colours::white.withAlpha (0.15f));

        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.setFont (juce::FontOptions ((float) height * 0.65f));
        g.drawText (preset.name + "  ·  " + preset.category,
                    6, 0, width - 12, height, juce::Justification::centredLeft, true);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        juce::ignoreUnused (e);
        panel.selectedRow = row;
        panel.presetList.selectRow (row);
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
    {
        panel.selectedRow = row;
        panel.loadSelectedPreset();
    }

    PresetBrowserPanel& panel;
};

PresetBrowserPanel::PresetBrowserPanel (te::Plugin& pluginRef)
    : plugin (pluginRef),
      pluginIdentifier (EngineHelpers::getPluginDescription (plugin).createIdentifierString())
{
    titleLabel.setText ("Presets — " + plugin.getName(), juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));

    categoryFilter.addItem ("All categories", 1);
    categoryFilter.setSelectedId (1, juce::dontSendNotification);
    categoryFilter.onChange = [this] { refreshPresets(); };

    listModel = std::make_unique<PresetListModel> (*this);
    presetList.setModel (listModel.get());
    presetList.setRowHeight (22);

    loadButton.onClick = [this] { loadSelectedPreset(); };
    saveButton.onClick = [this] { savePresetAs(); };
    deleteButton.onClick = [this] { deleteSelectedPreset(); };
    slotAButton.onClick = [this]
    {
        if (activeSlotA)
            captureSlot (true);
        else
            applySlot (true);
    };
    slotBButton.onClick = [this]
    {
        if (! activeSlotA)
            captureSlot (false);
        else
            applySlot (false);
    };
    copyAButton.onClick = [this] { copyAToB(); };

    addAndMakeVisible (titleLabel);
    addAndMakeVisible (categoryFilter);
    addAndMakeVisible (presetList);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (deleteButton);
    addAndMakeVisible (slotAButton);
    addAndMakeVisible (slotBButton);
    addAndMakeVisible (copyAButton);
    addAndMakeVisible (abStatusLabel);

    refreshCategories();
    refreshPresets();
    updateAbLabels();

    setSize (360, 320);
}

void PresetBrowserPanel::resized()
{
    auto r = getLocalBounds().reduced (10);
    titleLabel.setBounds (r.removeFromTop (20));
    r.removeFromTop (6);
    categoryFilter.setBounds (r.removeFromTop (24));
    r.removeFromTop (6);

    auto abRow = r.removeFromBottom (26);
    copyAButton.setBounds (abRow.removeFromRight (88));
    abRow.removeFromRight (6);
    slotBButton.setBounds (abRow.removeFromRight (28));
    abRow.removeFromRight (4);
    slotAButton.setBounds (abRow.removeFromRight (28));
    abRow.removeFromRight (8);
    abStatusLabel.setBounds (abRow);

    auto buttons = r.removeFromBottom (28);
    deleteButton.setBounds (buttons.removeFromRight (64));
    buttons.removeFromRight (6);
    saveButton.setBounds (buttons.removeFromRight (80));
    buttons.removeFromRight (6);
    loadButton.setBounds (buttons.removeFromRight (64));

    r.removeFromBottom (6);
    presetList.setBounds (r);
}

void PresetBrowserPanel::refreshCategories()
{
    const int previousId = categoryFilter.getSelectedId();
    categoryFilter.clear (juce::dontSendNotification);
    categoryFilter.addItem ("All categories", 1);

    int id = 2;
    for (const auto& category : PluginPresetManager::listCategories (pluginIdentifier))
        categoryFilter.addItem (category, id++);

    categoryFilter.setSelectedId (juce::jmax (1, previousId), juce::dontSendNotification);
}

void PresetBrowserPanel::refreshPresets()
{
    const auto category = categoryFilter.getSelectedId() > 1 ? categoryFilter.getText() : juce::String {};
    presets = PluginPresetManager::listPresets (pluginIdentifier, category);
    selectedRow = -1;
    presetList.updateContent();
    presetList.repaint();
}

void PresetBrowserPanel::loadSelectedPreset()
{
    if (! juce::isPositiveAndBelow (selectedRow, presets.size()))
        return;

    if (PluginPresetManager::loadPreset (plugin, presets.getReference (selectedRow).file))
    {
        if (onPluginChanged)
            onPluginChanged();
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Load Preset",
                                                "Could not load the selected preset.");
    }
}

void PresetBrowserPanel::savePresetAs()
{
    auto w = std::make_shared<juce::AlertWindow> ("Save Preset", "Enter preset name:", juce::AlertWindow::QuestionIcon);
    w->addTextEditor ("name", plugin.getName());
    w->addTextEditor ("category", "User");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, this] (int result)
    {
        if (result != 1)
            return;

        const auto name = w->getTextEditorContents ("name").trim();
        const auto category = w->getTextEditorContents ("category").trim();

        if (PluginPresetManager::saveNamedPreset (plugin, name, category))
        {
            refreshCategories();
            refreshPresets();
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                    "Save Preset",
                                                    "Could not save the preset.");
        }
    }));
}

void PresetBrowserPanel::deleteSelectedPreset()
{
    if (! juce::isPositiveAndBelow (selectedRow, presets.size()))
        return;

    const auto file = presets.getReference (selectedRow).file;
    const auto presetName = presets.getReference (selectedRow).name;

    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon,
                                        "Delete Preset",
                                        "Delete preset \"" + presetName + "\"?",
                                        "Delete", "Cancel", nullptr,
                                        juce::ModalCallbackFunction::create ([this, file] (int button)
    {
        if (button != 1)
            return;

        if (PluginPresetManager::deletePreset (file))
        {
            refreshCategories();
            refreshPresets();
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                    "Delete Preset",
                                                    "Could not delete the preset.");
        }
    }));
}

void PresetBrowserPanel::captureSlot (bool slotA)
{
    if (slotA)
    {
        stateA = PluginPresetManager::capturePluginState (plugin);
        hasStateA = stateA.isValid();
        activeSlotA = true;
    }
    else
    {
        stateB = PluginPresetManager::capturePluginState (plugin);
        hasStateB = stateB.isValid();
        activeSlotA = false;
    }

    updateAbLabels();
}

void PresetBrowserPanel::applySlot (bool slotA)
{
    const auto& state = slotA ? stateA : stateB;
    const bool hasState = slotA ? hasStateA : hasStateB;

    if (! hasState || ! state.isValid())
        return;

    if (PluginPresetManager::applyPluginState (plugin, state))
    {
        activeSlotA = slotA;

        if (onPluginChanged)
            onPluginChanged();

        updateAbLabels();
    }
}

void PresetBrowserPanel::copyAToB()
{
    if (! hasStateA)
        return;

    stateB = stateA.createCopy();
    hasStateB = true;
    updateAbLabels();
}

void PresetBrowserPanel::updateAbLabels()
{
    slotAButton.setColour (juce::TextButton::buttonColourId,
                           activeSlotA ? juce::Colours::steelblue : juce::Colours::darkgrey);
    slotBButton.setColour (juce::TextButton::buttonColourId,
                           ! activeSlotA ? juce::Colours::steelblue : juce::Colours::darkgrey);

    juce::String status = "A/B: ";
    status << (hasStateA ? "A saved" : "A empty");
    status << " · ";
    status << (hasStateB ? "B saved" : "B empty");
    status << " · active " << (activeSlotA ? "A" : "B");
    abStatusLabel.setText (status, juce::dontSendNotification);
}

void PresetBrowserPanel::showForPlugin (te::Plugin& pluginToShow, juce::Component* target,
                                        std::function<void()> onChanged)
{
    auto panel = std::make_unique<PresetBrowserPanel> (pluginToShow);
    panel->onPluginChanged = std::move (onChanged);

    juce::CallOutBox::launchAsynchronously (std::move (panel),
                                            target != nullptr ? target->localAreaToGlobal (target->getScreenBounds())
                                                              : juce::Rectangle<int> (200, 200, 1, 1),
                                            nullptr);
}

} // namespace skeletonhive
