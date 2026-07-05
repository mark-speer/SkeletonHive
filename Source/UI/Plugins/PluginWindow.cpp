#include "PluginWindow.h"

namespace arrange
{

#if JUCE_LINUX
constexpr bool shouldAddPluginWindowToDesktop = false;
#else
constexpr bool shouldAddPluginWindowToDesktop = true;
#endif

PluginWindow::PluginWindow (te::Plugin& plug)
    : DocumentWindow (plug.getName(), juce::Colours::black, DocumentWindow::closeButton, shouldAddPluginWindowToDesktop),
      plugin (plug),
      windowState (*plug.windowState)
{
    getConstrainer()->setMinimumOnscreenAmounts (0x10000, 50, 30, 50);
    setResizeLimits (100, 50, 4000, 4000);
    recreateEditor();
    setBoundsConstrained (getLocalBounds() + plugin.windowState->choosePositionForPluginWindow());

   #if JUCE_LINUX
    setAlwaysOnTop (true);
    addToDesktop();
   #endif

    updateStoredBounds = true;
}

PluginWindow::~PluginWindow()
{
    updateStoredBounds = false;
    plugin.edit.flushPluginStateIfNeeded (plugin);
    setEditor (nullptr);
}

void PluginWindow::show()
{
    setVisible (true);
    toFront (false);
    setBoundsConstrained (getBounds());
}

std::unique_ptr<juce::Component> PluginWindow::create (te::Plugin& plugin)
{
    if (auto* externalPlugin = dynamic_cast<te::ExternalPlugin*> (&plugin))
        if (externalPlugin->getAudioPluginInstance() == nullptr)
            return nullptr;

    auto w = std::make_unique<PluginWindow> (plugin);
    if (w->getContentComponent() == nullptr)
        return nullptr;

    w->show();
    return w;
}

void PluginWindow::recreateEditorAsync()
{
    setEditor (nullptr);
    juce::Timer::callAfterDelay (50, [this, sp = juce::Component::SafePointer<PluginWindow> (this)]
    {
        if (sp != nullptr)
            recreateEditor();
    });
}

void PluginWindow::moved()
{
    if (updateStoredBounds)
    {
        plugin.windowState->lastWindowBounds = getBounds();
        plugin.edit.pluginChanged (plugin);
    }
}

void PluginWindow::closeButtonPressed()
{
    plugin.windowState->closeWindowExplicitly();
}

void PluginWindow::recreateEditor()
{
    setEditor (nullptr);
    setEditor (plugin.createEditor());
}

void PluginWindow::setEditor (std::unique_ptr<te::Plugin::EditorComponent> newEditor)
{
    setConstrainer (nullptr);
    editor = std::move (newEditor);

    if (editor != nullptr)
    {
        setContentNonOwned (editor.get(), true);
        setResizable (editor->allowWindowResizing(), false);
        if (editor->allowWindowResizing())
            setConstrainer (editor->getBoundsConstrainer());
    }
    else
    {
        setContentNonOwned (nullptr, true);
    }
}

} // namespace arrange
