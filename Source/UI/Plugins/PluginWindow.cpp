#include "PluginWindow.h"
#include "NativePluginEditor.h"
#include "UI/Instruments/DrumRackEditor.h"
#include "UI/Instruments/SamplerEditor.h"
#include "Engine/DrumRackHelpers.h"
#include "Engine/EngineHelpers.h"
#include "Engine/NativePluginCatalog.h"

namespace skeletonhive
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

std::unique_ptr<juce::Component> PluginWindow::create (te::Plugin& plugin, juce::Component* alertParent)
{
    if (auto* externalPlugin = dynamic_cast<te::ExternalPlugin*> (&plugin))
    {
        if (externalPlugin->isInitialisingAsync())
            return nullptr;

        if (externalPlugin->getAudioPluginInstance() == nullptr)
        {
            EngineHelpers::showPluginLoadFailureAlert (alertParent, plugin);
            return nullptr;
        }
    }

    auto w = std::make_unique<PluginWindow> (plugin);

    if (w->getContentComponent() == nullptr)
    {
        EngineHelpers::showPluginLoadFailureAlert (alertParent, plugin);
        return nullptr;
    }

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

void PluginWindow::resized()
{
    DocumentWindow::resized();
    resizeToFitEditorContent();
}

void PluginWindow::childBoundsChanged (juce::Component* child)
{
    if (editor != nullptr && (child == editor.get() || editor->isParentOf (child)))
        resizeToFitEditorContent();

    DocumentWindow::childBoundsChanged (child);
}

void PluginWindow::resizeToFitEditorContent()
{
    if (editor == nullptr)
        return;

    const int contentW = editor->getWidth();
    const int contentH = editor->getHeight();

    if (contentW <= 0 || contentH <= 0)
        return;

    const auto border = getBorderThickness();
    const int titleBar = (int) getTitleBarHeight();
    const int targetW = contentW + border.getLeftAndRight();
    const int targetH = contentH + border.getTopAndBottom() + titleBar;

    if (targetW != getWidth() || targetH != getHeight())
        setSize (targetW, targetH);
}

void PluginWindow::recreateEditor()
{
    setEditor (nullptr);

    auto newEditor = plugin.createEditor();

    if (newEditor == nullptr)
        if (auto* rack = dynamic_cast<te::RackInstance*> (&plugin))
            if (DrumRackHelpers::isDrumRack (*rack))
                newEditor = DrumRackEditor::create (*rack);

    if (newEditor == nullptr)
        if (auto* sampler = dynamic_cast<te::SamplerPlugin*> (&plugin))
            if (! plugin.isInRack())
                newEditor = SamplerEditor::create (*sampler);

    if (newEditor == nullptr && NativePluginCatalog::isNativePlugin (plugin))
        newEditor = NativePluginEditor::create (plugin);

    setEditor (std::move (newEditor));
    resizeToFitEditorContent();
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

} // namespace skeletonhive
