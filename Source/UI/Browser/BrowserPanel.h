#pragma once

#include "SampleBrowserTab.h"
#include "PlacesBrowserTab.h"
#include "ClipsBrowserTab.h"
#include "GroovesBrowserTab.h"
#include "UI/Plugins/PluginBrowser.h"
#include "Engine/ContentLibraryManager.h"
#include "Engine/ClipLibraryManager.h"
#include "Engine/GroovePoolManager.h"
#include "Engine/PreviewPlayer.h"
#include "Engine/WaveformCache.h"
#include "Engine/PluginScanner.h"
#include "Engine/PluginStateManager.h"
#include "UI/Arrangement/EditViewState.h"

namespace skeletonhive
{

class BrowserTabbedComponent : public juce::TabbedComponent
{
public:
    using juce::TabbedComponent::TabbedComponent;

    std::function<void()> onTabChanged;

    void currentTabChanged (int, const juce::String&) override
    {
        if (onTabChanged)
            onTabChanged();
    }
};

class BrowserPanel : public juce::Component
{
public:
    BrowserPanel (ContentLibraryManager& library,
                  ClipLibraryManager& clipLibrary,
                  GroovePoolManager& groovePool,
                  PreviewPlayer& preview,
                  WaveformCache& waveforms,
                  PluginScanner& pluginScanner,
                  PluginStateManager& pluginStateManager,
                  te::Engine& engine,
                  te::Edit& edit,
                  te::SelectionManager& selectionManager);

    void refresh();
    void stopPreview();
    void resized() override;

    void showPluginsTab();
    void showGroovesTab();

    PluginBrowser* getPluginBrowser() const { return pluginsTab.get(); }

    static constexpr int preferredWidth = 280;

private:
    static constexpr int pluginsTabIndex = 4;
    static constexpr int groovesTabIndex = 3;

    ContentLibraryManager& contentLibrary;
    PreviewPlayer& previewPlayer;
    BrowserTabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    std::unique_ptr<PlacesBrowserTab> placesTab;
    std::unique_ptr<SampleBrowserTab> samplesTab;
    std::unique_ptr<ClipsBrowserTab> clipsTab;
    std::unique_ptr<GroovesBrowserTab> groovesTab;
    std::unique_ptr<PluginBrowser> pluginsTab;
};

} // namespace skeletonhive
