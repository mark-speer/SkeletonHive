#include "PluginBrowser.h"
#include "UI/Arrangement/TrackComponents.h"
#include "Engine/EngineHelpers.h"

namespace arrange
{

PluginBrowser::RootItem::RootItem (PluginBrowser& owner)
    : browser (owner)
{
}

void PluginBrowser::RootItem::itemOpennessChanged (bool isOpen)
{
    if (isOpen)
    {
        clearSubItems();
        for (const auto& desc : browser.pluginScanner.getKnownPluginList().getTypes())
            addSubItem (new PluginBrowser::PluginTreeItem (browser, desc));
    }
}

PluginBrowser::PluginTreeItem::PluginTreeItem (PluginBrowser& o, juce::PluginDescription d)
    : browser (o), desc (std::move (d))
{
}

void PluginBrowser::PluginTreeItem::paintItem (juce::Graphics& g, int width, int height)
{
    auto& lf = juce::LookAndFeel::getDefaultLookAndFeel();

    if (isSelected())
        g.fillAll (lf.findColour (juce::TextEditor::highlightColourId));

    g.setColour (lf.findColour (juce::Label::textColourId));
    g.setFont (juce::FontOptions ((float) height * 0.7f));

    auto text = desc.name;
    if (desc.manufacturerName.isNotEmpty())
        text << " - " << desc.manufacturerName;

    g.drawText (text, 4, 0, width - 8, height, juce::Justification::centredLeft, true);
}

void PluginBrowser::PluginTreeItem::itemClicked (const juce::MouseEvent& e)
{
    browser.selectedPlugin = desc;
    setSelected (true, true);

    if (e.getNumberOfClicks() >= 2)
        browser.insertButton.triggerClick();
}

PluginBrowser::PluginBrowser (PluginScanner& scanner, te::Edit& e)
    : pluginScanner (scanner), edit (e)
{
    rootItem = std::make_unique<RootItem> (*this);
    tree.setRootItem (rootItem.get());
    tree.setRootItemVisible (false);
    tree.setDefaultOpenness (true);

    scanButton.onClick = [this] { scanPlugins(); };
    insertButton.onClick = [this]
    {
        if (selectedTrack == nullptr || ! selectedPlugin.name.isNotEmpty())
        {
            statusLabel.setText ("Select a track and a plugin first", juce::dontSendNotification);
            return;
        }

        auto* at = dynamic_cast<te::AudioTrack*> (selectedTrack);
        if (at == nullptr)
        {
            statusLabel.setText ("Selected track can't host plugins", juce::dontSendNotification);
            return;
        }

        if (auto plugin = pluginScanner.createPlugin (selectedPlugin, edit))
        {
            // Insert at the end of the user chain, just before the built-in volume plugin
            int insertIndex = at->pluginList.size();
            for (int i = 0; i < at->pluginList.size(); ++i)
            {
                if (dynamic_cast<te::VolumeAndPanPlugin*> (at->pluginList[i]) != nullptr)
                {
                    insertIndex = i;
                    break;
                }
            }

            EngineHelpers::insertPluginOnTrack (*at, plugin, insertIndex);
        }
    };

    statusLabel.setText ("Ready", juce::dontSendNotification);
    addAndMakeVisible (tree);
    addAndMakeVisible (scanButton);
    addAndMakeVisible (insertButton);
    addAndMakeVisible (statusLabel);

    refreshList();
}

void PluginBrowser::resized()
{
    auto r = getLocalBounds().reduced (4);
    auto top = r.removeFromTop (28);
    scanButton.setBounds (top.removeFromLeft (100).reduced (1));
    insertButton.setBounds (top.removeFromLeft (120).reduced (1));
    statusLabel.setBounds (top);
    tree.setBounds (r);
}

void PluginBrowser::refreshList()
{
    if (rootItem != nullptr)
    {
        rootItem->clearSubItems();
        for (const auto& desc : pluginScanner.getKnownPluginList().getTypes())
            rootItem->addSubItem (new PluginTreeItem (*this, desc));

        rootItem->setOpen (true);
        tree.repaint();
    }
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

    if (numFound < 0)
        statusLabel.setText ("No plugin formats available", juce::dontSendNotification);
    else
        statusLabel.setText (juce::String (numFound) + " new, "
                             + juce::String (total) + " total",
                             juce::dontSendNotification);
}

void PluginBrowser::mouseDrag (const juce::MouseEvent& e)
{
    if (selectedPlugin.name.isEmpty() || e.getDistanceFromDragStart() < 6)
        return;

    if (auto* container = findParentComponentOfClass<juce::DragAndDropContainer>())
    {
        const juce::String payload = juce::String (PluginDragTypes::browserInsert) + ":" + selectedPlugin.createIdentifierString();
        container->startDragging (payload, this, juce::ScaledImage(), true, nullptr, &e.source);
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
        statusLabel.setText ("A scan is already in progress", juce::dontSendNotification);
    }
}

} // namespace arrange
