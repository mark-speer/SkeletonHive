#include "PluginWindow.h"
#include "NativePluginEditor.h"
#include "SandboxEmbeddedEditor.h"
#include "UI/Instruments/DrumRackEditor.h"
#include "UI/Instruments/SamplerEditor.h"
#include "UI/Instruments/SynthEditor.h"
#include "UI/Effects/EqualiserEditor.h"
#include "UI/Effects/CompressorEditor.h"
#include "UI/Effects/DelayReverbEditor.h"
#include "UI/Effects/SaturationEditor.h"
#include "UI/Effects/MultibandDynamicsEditor.h"
#include "UI/Effects/NamEditor.h"
#include "Engine/DrumRackHelpers.h"
#include "Engine/Effects/NativeCustomPlugins.h"
#include "Engine/Effects/SaturationPlugin.h"
#include "Engine/Effects/MultibandDynamicsPlugin.h"
#include "Engine/Effects/NamPlugin.h"
#include "Engine/EngineHelpers.h"
#include "Engine/PluginHost/SandboxedPluginInstance.h"
#include "Engine/NativePluginCatalog.h"

namespace skeletonhive
{

namespace
{
class SandboxFallbackPanel : public juce::Component
{
public:
    SandboxFallbackPanel (PluginWindow& windowIn,
                          te::ExternalPlugin& externalPlugin,
                          const SandboxEditorResult& initialResult)
        : ownerWindow (windowIn),
          external (externalPlugin)
    {
        addAndMakeVisible (messageLabel);
        messageLabel.setJustificationType (juce::Justification::centred);
        messageLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));

        addAndMakeVisible (openButton);
        openButton.setButtonText ("Open Editor");
        openButton.onClick = [this] { requestEditor(); };

        showResult (initialResult);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (16);
        openButton.setBounds (area.removeFromBottom (28).withSizeKeepingCentre (180, 28));
        messageLabel.setBounds (area);
    }

private:
    void requestEditor()
    {
        if (auto* sandboxed = SandboxedPluginInstance::fromExternalPlugin (external))
        {
            SandboxEditorResult result;
            if (sandboxed->requestBridgeEditor (result))
            {
                if (result.nativeHandle != nullptr)
                {
                    ownerWindow.setSandboxEmbeddedEditor (external);
                    return;
                }

                showResult (result);
                return;
            }

            messageLabel.setText ("Sandbox editor failed to open:\n" + result.error,
                                  juce::dontSendNotification);
            return;
        }

        messageLabel.setText ("Sandbox plugin instance is unavailable.",
                              juce::dontSendNotification);
    }

    void showResult (const SandboxEditorResult& result)
    {
        if (result.usesBridgeWindow())
        {
            messageLabel.setText ("This plugin is running in a separate sandbox process.\n"
                                  "Its editor should appear in a dedicated bridge window.\n"
                                  "Check the taskbar for a second SkeletonHive window if you do not see it.",
                                  juce::dontSendNotification);
            return;
        }

        if (result.success && result.nativeHandle != nullptr)
        {
            messageLabel.setText ("Sandbox editor opened.",
                                  juce::dontSendNotification);
            return;
        }

        if (result.error.isNotEmpty())
        {
            messageLabel.setText (result.error, juce::dontSendNotification);
            return;
        }

        messageLabel.setText ("Plugin sandbox editor is not embedded.\n"
                              "Click Open Editor to reconnect.",
                              juce::dontSendNotification);
    }

    PluginWindow& ownerWindow;
    te::ExternalPlugin& external;
    juce::Label messageLabel;
    juce::TextButton openButton;
};
} // namespace

#if JUCE_LINUX
constexpr bool shouldAddPluginWindowToDesktop = false;
#else
constexpr bool shouldAddPluginWindowToDesktop = true;
#endif

PluginWindow::PluginWindow (te::Plugin& plug, bool createEditorNow)
    : DocumentWindow (plug.getName(), juce::Colours::black, DocumentWindow::closeButton, shouldAddPluginWindowToDesktop),
      plugin (plug),
      windowState (*plug.windowState)
{
    getConstrainer()->setMinimumOnscreenAmounts (0x10000, 50, 30, 50);
    setResizeLimits (100, 50, 4000, 4000);

    // Sandboxed plugins install their own content after construction — calling
    // recreateEditor() first would hit ExternalPlugin::createEditor with a
    // hasEditor/createEditor mismatch (Debug assert) for no benefit.
    if (createEditorNow)
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
    clearBridgeEditorInvalidationHandler();

    if (auto* external = dynamic_cast<te::ExternalPlugin*> (&plugin))
        if (auto* sandboxed = SandboxedPluginInstance::fromExternalPlugin (*external))
            sandboxed->closeEditorInBridge();

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

        if (EngineHelpers::isSandboxedExternalPlugin (plugin))
        {
            auto w = std::make_unique<PluginWindow> (plugin, false);

           #if JUCE_WINDOWS
            w->setSandboxEmbeddedEditor (*externalPlugin);
           #else
            w->setSandboxPlaceholder (*externalPlugin);
           #endif

            w->show();
            return w;
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

    // FourOsc is allowed inside racks (unlike SamplerEditor) — simpler parameter surface.
    if (newEditor == nullptr)
        if (auto* fourOsc = dynamic_cast<te::FourOscPlugin*> (&plugin))
            newEditor = SynthEditor::create (*fourOsc);

    if (newEditor == nullptr)
        if (auto* eq = dynamic_cast<te::EqualiserPlugin*> (&plugin))
            newEditor = EqualiserEditor::create (*eq);

    if (newEditor == nullptr)
        if (auto* comp = dynamic_cast<te::CompressorPlugin*> (&plugin))
            newEditor = CompressorEditor::create (*comp);

    if (newEditor == nullptr)
        if (auto* delay = dynamic_cast<te::DelayPlugin*> (&plugin))
            newEditor = DelayEditor::create (*delay);

    if (newEditor == nullptr)
        if (auto* reverb = dynamic_cast<te::ReverbPlugin*> (&plugin))
            newEditor = ReverbEditor::create (*reverb);

    if (newEditor == nullptr)
        if (auto* saturation = dynamic_cast<SaturationPlugin*> (&plugin))
            newEditor = SaturationEditor::create (*saturation);

    if (newEditor == nullptr)
        if (auto* multiband = dynamic_cast<MultibandDynamicsPlugin*> (&plugin))
            newEditor = MultibandDynamicsEditor::create (*multiband);

    if (newEditor == nullptr)
        if (auto* nam = dynamic_cast<NamPlugin*> (&plugin))
            newEditor = NamEditor::create (*nam);

    if (newEditor == nullptr && NativePluginCatalog::isNativePlugin (plugin))
        newEditor = NativePluginEditor::create (plugin);

    setEditor (std::move (newEditor));
    resizeToFitEditorContent();
}

void PluginWindow::resizeToFitSandboxContent (int contentW, int contentH)
{
    if (contentW <= 0 || contentH <= 0)
        return;

    const auto border = getBorderThickness();
    const int titleBar = (int) getTitleBarHeight();
    const int targetW = contentW + border.getLeftAndRight();
    const int targetH = contentH + border.getTopAndBottom() + titleBar;

    if (targetW != getWidth() || targetH != getHeight())
        setSize (targetW, targetH);
}

void PluginWindow::setSandboxPlaceholder (te::ExternalPlugin& externalPlugin)
{
    SandboxEditorResult result;

    if (auto* sandboxed = SandboxedPluginInstance::fromExternalPlugin (externalPlugin))
        sandboxed->requestBridgeEditor (result);

    replaceSandboxContentWithFallback (externalPlugin, result);
}

void PluginWindow::setSandboxEmbeddedEditor (te::ExternalPlugin& externalPlugin)
{
    SandboxEditorResult result;

    auto* sandboxed = SandboxedPluginInstance::fromExternalPlugin (externalPlugin);

    if (sandboxed != nullptr)
        sandboxed->requestBridgeEditor (result);

    if (result.success && result.nativeHandle != nullptr)
    {
        auto* embedded = new SandboxEmbeddedEditor (result.nativeHandle, result.width, result.height);
        setContentOwned (embedded, true);
        setResizable (true, false);
        resizeToFitSandboxContent (embedded->getPreferredWidth(), embedded->getPreferredHeight());

        if (sandboxed != nullptr)
        {
            sandboxed->setBridgeEditorInvalidationHandler (
                [safeWindow = juce::Component::SafePointer<PluginWindow> (this)]
                {
                    if (safeWindow == nullptr)
                        return;

                    if (auto* external = dynamic_cast<te::ExternalPlugin*> (&safeWindow->plugin))
                        safeWindow->replaceSandboxContentWithFallback (*external);
                });
        }

        return;
    }

    replaceSandboxContentWithFallback (externalPlugin, result);
}

void PluginWindow::replaceSandboxContentWithFallback (te::ExternalPlugin& externalPlugin,
                                                      const SandboxEditorResult& result)
{
    if (auto* embedded = dynamic_cast<SandboxEmbeddedEditor*> (getContentComponent()))
        embedded->detach();

    clearBridgeEditorInvalidationHandler();

    SandboxEditorResult panelResult = result;
    if (! panelResult.success && panelResult.error.isEmpty())
        panelResult.error = "Plugin sandbox editor was reset. Click Open Editor to reconnect.";

    setContentOwned (new SandboxFallbackPanel (*this, externalPlugin, panelResult), true);
    setResizable (true, false);
    setSize (460, 190);
}

void PluginWindow::clearBridgeEditorInvalidationHandler()
{
    if (auto* external = dynamic_cast<te::ExternalPlugin*> (&plugin))
        if (auto* sandboxed = SandboxedPluginInstance::fromExternalPlugin (*external))
            sandboxed->setBridgeEditorInvalidationHandler ({});
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
