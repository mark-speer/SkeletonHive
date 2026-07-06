#include "BrowserPanel.h"
#include "UI/AppLookAndFeel.h"

namespace skeletonhive
{

BrowserPanel::BrowserPanel (ContentLibraryManager& library,
                            ClipLibraryManager& clipLibrary,
                            GroovePoolManager& groovePool,
                            PreviewPlayer& preview,
                            WaveformCache& waveforms,
                            PluginScanner& pluginScanner,
                            PluginStateManager& pluginStateManager,
                            te::Engine& engine,
                            te::Edit& edit,
                            te::SelectionManager& selectionManager)
    : contentLibrary (library),
      previewPlayer (preview)
{
    placesTab = std::make_unique<PlacesBrowserTab> (library);
    samplesTab = std::make_unique<SampleBrowserTab> (library, preview, waveforms, engine);
    clipsTab = std::make_unique<ClipsBrowserTab> (clipLibrary, edit);
    groovesTab = std::make_unique<GroovesBrowserTab> (groovePool, edit, selectionManager);
    pluginsTab = std::make_unique<PluginBrowser> (pluginScanner, edit, pluginStateManager);

    clipsTab->onLibraryChanged = [this]
    {
        refresh();
    };

    placesTab->onPlaceSelected = [this] (const juce::File& root)
    {
        stopPreview();
        samplesTab->setRootFilter (root);
        tabs.setCurrentTabIndex (1);
    };

    placesTab->onShowFavorites = [this]
    {
        stopPreview();
        samplesTab->showFavoritesFilter();
        tabs.setCurrentTabIndex (1);
    };

    tabs.onTabChanged = [this]
    {
        stopPreview();
    };

    tabs.addTab ("Places", juce::Colours::transparentBlack, placesTab.get(), false);
    tabs.addTab ("Samples", juce::Colours::transparentBlack, samplesTab.get(), false);
    tabs.addTab ("Clips", juce::Colours::transparentBlack, clipsTab.get(), false);
    tabs.addTab ("Grooves", juce::Colours::transparentBlack, groovesTab.get(), false);
    tabs.addTab ("Plugins", juce::Colours::transparentBlack, pluginsTab.get(), false);
    addAndMakeVisible (tabs);
}

void BrowserPanel::refresh()
{
    placesTab->refreshPlaces();
    samplesTab->refreshList();
    clipsTab->refreshList();
    groovesTab->refreshList();
    pluginsTab->refreshList();
}

void BrowserPanel::stopPreview()
{
    previewPlayer.stop();
}

void BrowserPanel::showPluginsTab()
{
    tabs.setCurrentTabIndex (pluginsTabIndex);
}

void BrowserPanel::showGroovesTab()
{
    tabs.setCurrentTabIndex (groovesTabIndex);
}

void BrowserPanel::resized()
{
    tabs.setBounds (getLocalBounds());
}

} // namespace skeletonhive
