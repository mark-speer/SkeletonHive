#include "PluginBrowser.h"
#include "Engine/EngineHelpers.h"
#include "Engine/PluginDragManager.h"
#include "Engine/TrackPluginChainModel.h"

namespace arrange
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
        }
    };

    statusLabel.setText ("Ready", juce::dontSendNotification);

    addAndMakeVisible (searchBox);
    addAndMakeVisible (categoryFilter);
    addAndMakeVisible (vendorFilter);
    addAndMakeVisible (pluginList);
    addAndMakeVisible (scanButton);
    addAndMakeVisible (insertButton);
    addAndMakeVisible (statusLabel);

    refreshList();
}

void PluginBrowser::resized()
{
    auto r = getLocalBounds().reduced (4);
    auto top = r.removeFromTop (26);
    scanButton.setBounds (top.removeFromRight (52).reduced (1));
    insertButton.setBounds (top.removeFromRight (64).reduced (1));

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

void PluginBrowser::finishScan (int numFound)
{
    stopTimer();
    scanButton.setEnabled (true);
    refreshList();

    const int total = pluginScanner.getKnownPluginList().getNumTypes();
    statusLabel.setText (juce::String (numFound) + " new, " + juce::String (total) + " total",
                         juce::dontSendNotification);
}

void PluginBrowser::mouseDrag (const juce::MouseEvent& e)
{
    if (selectedPlugin.name.isEmpty() || e.getDistanceFromDragStart() < 6)
        return;

    if (auto* container = findParentComponentOfClass<juce::DragAndDropContainer>())
    {
        PluginDragPayload payload;
        payload.kind = PluginDragPayload::Kind::browserInsert;
        payload.pluginIdentifier = selectedPlugin.createIdentifierString();
        container->startDragging (payload.encode(), this, juce::ScaledImage(), true, nullptr, &e.source);
    }
}

void PluginBrowser::scanPlugins()
{
    if (pluginScanner.isScanning())
        return;

    scanButton.setEnabled (false);
    statusLabel.setText ("Scanning...", juce::dontSendNotification);
    startTimer (100);

    const auto safeThis = juce::Component::SafePointer<PluginBrowser> (this);

    if (! pluginScanner.scanDefaultLocations ([safeThis] (int numFound)
    {
        if (safeThis != nullptr)
            safeThis->finishScan (numFound);
    }))
    {
        stopTimer();
        scanButton.setEnabled (true);
        statusLabel.setText ("Scan already in progress", juce::dontSendNotification);
    }
}

} // namespace arrange
