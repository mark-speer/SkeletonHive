#include "PluginPickerDialog.h"
#include "Engine/EngineHelpers.h"

namespace skeletonhive
{

class PluginPickerDialog::PluginListModel : public juce::ListBoxModel
{
public:
    explicit PluginListModel (PluginPickerDialog& owner) : dialog (owner) {}

    int getNumRows() override { return (int) dialog.getFilteredPlugins().size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool rowSelected) override
    {
        const auto plugins = dialog.getFilteredPlugins();
        if (! juce::isPositiveAndBelow (row, plugins.size()))
            return;

        const auto& desc = plugins[(size_t) row];

        if (rowSelected)
            g.fillAll (juce::Colours::white.withAlpha (0.15f));

        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.setFont (juce::FontOptions ((float) height * 0.65f));

        juce::String line = desc.name;
        if (desc.manufacturerName.isNotEmpty())
            line << "  ·  " << desc.manufacturerName;

        g.drawText (line, 6, 0, width - 12, height, juce::Justification::centredLeft, true);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        const auto plugins = dialog.getFilteredPlugins();
        if (! juce::isPositiveAndBelow (row, plugins.size()))
            return;

        dialog.selectedPlugin = plugins[(size_t) row];

        if (e.getNumberOfClicks() >= 2)
            dialog.chooseSelected();
    }

    PluginPickerDialog& dialog;
};

PluginPickerDialog::PluginPickerDialog (PluginScanner& scanner,
                                        te::Engine& engine,
                                        PluginStateManager& stateManager,
                                        PluginPickerFilter filter,
                                        const juce::String& title)
    : pluginScanner (scanner),
      engineRef (engine),
      pluginStateManager (stateManager),
      pickerFilter (filter)
{
    titleLabel.setText (title, juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));

    searchBox.setTextToShowWhenEmpty ("Search plugins...", juce::Colours::grey);
    searchBox.onTextChange = [this] { rebuildList(); };

    listModel = std::make_unique<PluginListModel> (*this);
    pluginList.setModel (listModel.get());
    pluginList.setRowHeight (24);

    okButton.onClick = [this] { chooseSelected(); };
    cancelButton.onClick = [this]
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (0);
    };

    addAndMakeVisible (titleLabel);
    addAndMakeVisible (searchBox);
    addAndMakeVisible (pluginList);
    addAndMakeVisible (okButton);
    addAndMakeVisible (cancelButton);

    setSize (420, 360);
    rebuildList();
}

void PluginPickerDialog::resized()
{
    auto r = getLocalBounds().reduced (12);
    titleLabel.setBounds (r.removeFromTop (22));
    r.removeFromTop (8);
    searchBox.setBounds (r.removeFromTop (26));
    r.removeFromTop (8);

    auto buttons = r.removeFromBottom (28);
    cancelButton.setBounds (buttons.removeFromRight (80));
    buttons.removeFromRight (8);
    okButton.setBounds (buttons.removeFromRight (80));

    pluginList.setBounds (r);
}

juce::Array<juce::PluginDescription> PluginPickerDialog::getFilteredPlugins() const
{
    const auto search = searchBox.getText().trim().toLowerCase();
    juce::Array<juce::PluginDescription> result;

    for (const auto& desc : pluginScanner.getKnownPluginList().getTypes())
    {
        if (pickerFilter == PluginPickerFilter::instrumentsOnly && ! desc.isInstrument)
            continue;
        if (pickerFilter == PluginPickerFilter::effectsOnly && desc.isInstrument)
            continue;

        if (search.isNotEmpty())
        {
            const auto hay = (desc.name + " " + desc.manufacturerName + " " + desc.category).toLowerCase();
            if (! hay.contains (search))
                continue;
        }

        result.add (desc);
    }

    return result;
}

void PluginPickerDialog::rebuildList()
{
    pluginList.updateContent();
    pluginList.repaint();
}

void PluginPickerDialog::chooseSelected()
{
    if (! selectedPlugin.name.isNotEmpty())
        return;

    if (onPluginChosen)
        onPluginChosen (selectedPlugin);

    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState (1);
}

void PluginPickerDialog::show (juce::Component* centreAround,
                               PluginScanner& scanner,
                               te::Engine& engine,
                               PluginStateManager& stateManager,
                               PluginPickerFilter filter,
                               const juce::String& title,
                               std::function<void (const juce::PluginDescription&)> onChosen)
{
    auto* content = new PluginPickerDialog (scanner, engine, stateManager, filter, title);
    content->onPluginChosen = std::move (onChosen);

    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = title;
    opts.content.setOwned (content);
    opts.componentToCentreAround = centreAround;
    opts.useNativeTitleBar = true;
    opts.resizable = false;
    opts.launchAsync();
}

} // namespace skeletonhive
