#pragma once

#include "EngineHelpers.h"
#include "UI/Plugins/PluginWindow.h"

namespace arrange
{

class ExtendedUIBehaviour : public te::UIBehaviour
{
public:
    std::unique_ptr<juce::Component> createPluginWindow (te::PluginWindowState& pws) override
    {
        if (auto* ws = dynamic_cast<te::Plugin::WindowState*> (&pws))
            return PluginWindow::create (ws->plugin);

        return {};
    }

    void recreatePluginWindowContentAsync (te::Plugin& p) override
    {
        if (auto* w = dynamic_cast<PluginWindow*> (p.windowState->pluginWindow.get()))
            return w->recreateEditorAsync();

        te::UIBehaviour::recreatePluginWindowContentAsync (p);
    }
};

} // namespace arrange
