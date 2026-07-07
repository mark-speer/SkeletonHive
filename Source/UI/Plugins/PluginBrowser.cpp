#include "PluginBrowser.h"
#include "Engine/EngineHelpers.h"
#include "Engine/PluginDragManager.h"
#include "Engine/TrackPluginChainModel.h"

namespace skeletonhive
{

class PluginBrowser::PluginListModel : public juce::ListBoxModel
{
public:
    explicit PluginListModel (PluginBrowser& owner) : browser (owner) {}

    int getNumRows() override { return (int) browser.getFilteredPlugins().size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        const auto plugins = browser.getFilteredPlugins();
        if (! juce::isPositiveAndBelow (row, plugins.size()))
            return;

        const auto& desc = plugins[(size_t) row];
        if (selected)
            g.fillAll (juce::Colours::white.withAlpha (0.15f));

        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.setFont (juce::FontOptions ((float) height * 0.65f));

        juce::String line = desc.name;
        if (desc.manufacturerName.isNotEmpty())
            line << "  ·  " << desc.manufacturerName;

        g.drawText (line, 6, 0, width - 12, height, juce::Justification::centredLeft, true);

        if (desc.isInstrument)
        {
            g.setColour (juce::Colour (0xff5a189a).withAlpha (0.8f));
            g.fillRoundedRectangle ((float) width - 56.0f, 4.0f, 48.0f, (float) height - 8.0f, 3.0f);
            g.setColour (juce::Colours::white);
            g.drawText ("INST", width - 56, 0, 48, height, juce::Justification::centred, false);
        }
    }

    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        const auto plugins = browser.getFilteredPlugins();
        if (! juce::isPositiveAndBelow (row, plugins.size()))
            return;

        browser.selectedPlugin = plugins[(size_t) row];

        if (e.getNumberOfClicks() >= 2)
            browser.insertButton.triggerClick();
    }

    juce::var getDragSourceDescription (const juce::SparseSet<int>& rowsToDescribe) override
    {
        if (rowsToDescribe.size() != 1)
            return {};

        const int row = rowsToDescribe[0];
        const auto plugins = browser.getFilteredPlugins();
        if (! juce::isPositiveAndBelow (row, plugins.size()))
            return {};

        browser.selectedPlugin = plugins[(size_t) row];

        PluginDragPayload payload;
        payload.kind = PluginDragPayload::Kind::browserInsert;
        payload.pluginIdentifier = browser.selectedPlugin.createIdentifierString();
        return payload.encode();
    }

    PluginBrowser& browser;
};

PluginBrowser::PluginBrowser (PluginScanner& scanner, te::Edit& e, PluginStateManager& sm)
    : pluginScanner (scanner), edit (e), pluginStateManager (sm)
{
    searchBox.setTextToShowWhenEmpty ("Search plugins...", juce::Colours::grey);
    searchBox.addListener (this);

    categoryFilter.addItem ("All", (int) PluginBrowserFilter::All + 1);
    categoryFilter.addItem ("Instruments", (int) PluginBrowserFilter::Instruments + 1);
    categoryFilter.addItem ("Effects", (int) PluginBrowserFilter::Effects + 1);
    categoryFilter.addItem ("Favorites", (int) PluginBrowserFilter::Favorites + 1);
    categoryFilter.addItem ("Recent", (int) PluginBrowserFilter::Recent + 1);
    categoryFilter.setSelectedId ((int) PluginBrowserFilter::All + 1, juce::dontSendNotification);
    categoryFilter.addListener (this);

    vendorFilter.addItem ("All vendors", 1);
    vendorFilter.setSelectedId (1, juce::dontSendNotification);
    vendorFilter.addListener (this);

    listModel = std::make_unique<PluginListModel> (*this);
    pluginList.setModel (listModel.get());
    pluginList.setRowHeight (24);

    scanButton.onClick = [this] { scanPlugins(); };
    failedButton.onClick = [this] { showFailedPluginsDialog(); };
    insertButton.onClick = [this]
    {
        if (selectedTrack == nullptr || ! selectedPlugin.name.isNotEmpty())
        {
            statusLabel.setText ("Select a track and plugin", juce::dontSendNotification);
            return;
        }

        if (auto* at = dynamic_cast<te::AudioTrack*> (selectedTrack))
        {
            if (auto plugin = pluginScanner.createPlugin (selectedPlugin, edit))
            {
                TrackPluginChainModel model (*at);
                const int index = model.resolveInsertIndex (model.getUserChainSize(),
                                                            EngineHelpers::isInstrumentDescription (selectedPlugin),
                                                            nullptr);
                if (index >= 0)
                {
                    EngineHelpers::insertPluginOnTrack (*at, plugin, index);
                    pluginStateManager.recordRecentUse (selectedPlugin.createIdentifierString());
                }
            }
            else
            {
                EngineHelpers::showPluginInsertFailureAlert (this, selectedPlugin);
            }
        }
    };

    statusLabel.setText ("Ready", juce::dontSendNotification);

    addAndMakeVisible (searchBox);
    addAndMakeVisible (categoryFilter);
    addAndMakeVisible (vendorFilter);
    addAndMakeVisible (pluginList);
    addAndMakeVisible (scanButton);
    addAndMakeVisible (failedButton);
    addAndMakeVisible (insertButton);
    addAndMakeVisible (statusLabel);

    refreshList();
    updateFailedButton();
}

void PluginBrowser::resized()
{
    auto r = getLocalBounds().reduced (4);
    auto top = r.removeFromTop (26);
    insertButton.setBounds (top.removeFromRight (64).reduced (1));
    failedButton.setBounds (top.removeFromRight (58).reduced (1));
    scanButton.setBounds (top.removeFromRight (52).reduced (1));

    auto filters = r.removeFromTop (26);
    categoryFilter.setBounds (filters.removeFromLeft (110).reduced (1));
    vendorFilter.setBounds (filters.removeFromLeft (120).reduced (1));
    searchBox.setBounds (filters.reduced (1));

    statusLabel.setBounds (r.removeFromBottom (18));
    pluginList.setBounds (r);
}

void PluginBrowser::refreshList()
{
    juce::StringArray vendors;
    for (const auto& desc : pluginScanner.getKnownPluginList().getTypes())
        if (desc.manufacturerName.isNotEmpty() && ! vendors.contains (desc.manufacturerName))
            vendors.add (desc.manufacturerName);

    vendors.sort (true);
    vendorFilter.clear (juce::dontSendNotification);
    vendorFilter.addItem ("All vendors", 1);
    int id = 2;
    for (const auto& v : vendors)
        vendorFilter.addItem (v, id++);

    vendorFilter.setSelectedId (1, juce::dontSendNotification);
    rebuildListBox();
}

juce::Array<juce::PluginDescription> PluginBrowser::getFilteredPlugins() const
{
    const auto filter = static_cast<PluginBrowserFilter> (categoryFilter.getSelectedId() - 1);
    const auto search = searchBox.getText().trim().toLowerCase();
    const auto vendor = vendorFilter.getSelectedId() > 1 ? vendorFilter.getText() : juce::String {};

    juce::Array<juce::PluginDescription> result;

    auto addIfMatch = [&] (const juce::PluginDescription& desc)
    {
        if (filter == PluginBrowserFilter::Instruments && ! desc.isInstrument)
            return;
        if (filter == PluginBrowserFilter::Effects && desc.isInstrument)
            return;
        if (filter == PluginBrowserFilter::Favorites
            && ! pluginStateManager.isFavorite (desc.createIdentifierString()))
            return;
        if (filter == PluginBrowserFilter::Recent
            && ! pluginStateManager.getRecentlyUsed().contains (desc.createIdentifierString()))
            return;

        if (vendor.isNotEmpty() && desc.manufacturerName != vendor)
            return;

        if (search.isNotEmpty())
        {
            const auto hay = (desc.name + " " + desc.manufacturerName + " " + desc.category).toLowerCase();
            if (! hay.contains (search))
                return;
        }

        result.add (desc);
    };

    if (filter == PluginBrowserFilter::Recent)
    {
        for (const auto& id : pluginStateManager.getRecentlyUsed())
        {
            const auto desc = EngineHelpers::lookupKnownPlugin (edit.engine, id);
            if (desc.name.isNotEmpty())
                addIfMatch (desc);
        }
        return result;
    }

    for (const auto& desc : pluginScanner.getKnownPluginList().getTypes())
        addIfMatch (desc);

    return result;
}

void PluginBrowser::rebuildListBox()
{
    pluginList.updateContent();
    pluginList.repaint();
}

void PluginBrowser::textEditorTextChanged (juce::TextEditor&)
{
    rebuildListBox();
}

void PluginBrowser::comboBoxChanged (juce::ComboBox*)
{
    rebuildListBox();
}

void PluginBrowser::timerCallback()
{
    if (! pluginScanner.isScanning())
        return;

    const auto progress = juce::roundToInt (pluginScanner.getScanProgress() * 100.0f);
    statusLabel.setText (pluginScanner.getCurrentScanTarget() + " (" + juce::String (progress) + "%)",
                         juce::dontSendNotification);
}

void PluginBrowser::finishScan (const PluginScanReport& report)
{
    stopTimer();
    scanButton.setEnabled (true);
    refreshList();
    updateFailedButton();

    juce::String status = juce::String (report.newPluginsFound) + " new, "
                        + juce::String (report.totalPlugins) + " total";

    if (report.skippedBlacklisted > 0)
        status << ", " + juce::String (report.skippedBlacklisted) + " blacklisted";

    if (report.newlyFailed > 0)
        status << ", " + juce::String (report.newlyFailed) + " failed";

    statusLabel.setText (status, juce::dontSendNotification);
}

void PluginBrowser::updateFailedButton()
{
    const int count = pluginScanner.getBlacklistedFiles().size();
    failedButton.setButtonText (count > 0 ? "Failed (" + juce::String (count) + ")" : "Failed");
    failedButton.setEnabled (count > 0 || ! pluginScanner.isScanning());
}

void PluginBrowser::showFailedPluginsDialog()
{
    const auto blacklisted = pluginScanner.getBlacklistedFiles();

    if (blacklisted.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                "Failed Plugins",
                                                "No plugins are currently blacklisted.",
                                                "OK",
                                                this);
        return;
    }

    juce::PopupMenu menu;
    menu.addSectionHeader ("Failed / blacklisted plugins");

    for (int i = 0; i < blacklisted.size(); ++i)
    {
        const auto file = blacklisted[i];
        const auto label = juce::File (file).getFileName().isNotEmpty() ? juce::File (file).getFileName() : file;
        menu.addItem (1000 + i, "Retry: " + label);
    }

    menu.addSeparator();
    menu.addItem (1, "Retry All Failed Plugins");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&failedButton),
                        [this, blacklisted] (int result)
    {
        if (result == 0)
            return;

        if (result == 1)
        {
            failedButton.setEnabled (false);
            statusLabel.setText ("Retrying failed plugins...", juce::dontSendNotification);
            startTimer (100);

            const auto safeThis = juce::Component::SafePointer<PluginBrowser> (this);

            pluginScanner.rescanFailedPlugins ([safeThis] (const PluginScanReport& report)
            {
                if (safeThis != nullptr)
                    safeThis->finishScan (report);
            });
            return;
        }

        const int index = result - 1000;

        if (! juce::isPositiveAndBelow (index, blacklisted.size()))
            return;

        const auto file = blacklisted[index];
        failedButton.setEnabled (false);
        statusLabel.setText ("Retrying " + juce::File (file).getFileName() + "...", juce::dontSendNotification);
        startTimer (100);

        const auto safeThis = juce::Component::SafePointer<PluginBrowser> (this);

        pluginScanner.rescanBlacklistedFile (file, [safeThis] (const PluginScanReport& report)
        {
            if (safeThis != nullptr)
                safeThis->finishScan (report);
        });
    });
}

void PluginBrowser::scanPlugins()
{
    if (pluginScanner.isScanning())
        return;

    scanButton.setEnabled (false);
    statusLabel.setText ("Scanning...", juce::dontSendNotification);
    startTimer (100);

    const auto safeThis = juce::Component::SafePointer<PluginBrowser> (this);

    if (! pluginScanner.scanDefaultLocations ([safeThis] (const PluginScanReport& report)
    {
        if (safeThis != nullptr)
            safeThis->finishScan (report);
    }))
    {
        stopTimer();
        scanButton.setEnabled (true);
        statusLabel.setText ("Scan already in progress", juce::dontSendNotification);
    }
}

} // namespace skeletonhive
