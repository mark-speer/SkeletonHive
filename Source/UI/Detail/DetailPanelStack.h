#pragma once

#include "UI/Plugins/PluginTrayComponent.h"
#include "UI/Arrangement/ClipInspectorPanel.h"

namespace skeletonhive
{

enum class DetailView
{
    devices,
    clip
};

/** Live-style bottom detail area: toggles Devices (plugin tray) vs Clip inspector. */
class DetailPanelStack : public juce::Component
{
public:
    DetailPanelStack (std::unique_ptr<PluginTrayComponent> tray,
                      std::unique_ptr<ClipInspectorPanel> inspector);

    PluginTrayComponent* getPluginTray() const { return pluginTray.get(); }
    ClipInspectorPanel* getClipInspector() const { return clipInspector.get(); }

    void setActiveView (DetailView view);
    DetailView getActiveView() const { return activeView; }

    void autoSelectView (bool hasAudioClipSelection);
    void updateClipSelection (const juce::Array<te::Clip*>& clips);

    int getPreferredHeight() const;

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    void refreshVisibility();

    std::unique_ptr<PluginTrayComponent> pluginTray;
    std::unique_ptr<ClipInspectorPanel> clipInspector;

    juce::TextButton devicesTab { "Devices" };
    juce::TextButton clipTab { "Clip" };

    DetailView activeView = DetailView::devices;
    bool userPinnedView = false;
    bool hasAudioClips = false;

    static constexpr int tabBarHeight = 26;
    static constexpr int devicesContentHeight = 148;
};

} // namespace skeletonhive
