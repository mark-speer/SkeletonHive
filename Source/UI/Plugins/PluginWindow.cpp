#include "PluginWindow.h"
#include "NativePluginEditor.h"
#include "UI/Instruments/DrumRackEditor.h"
#include "UI/Instruments/SamplerEditor.h"
#include "UI/Instruments/SynthEditor.h"
#include "UI/Effects/EqualiserEditor.h"
#include "UI/Effects/CompressorEditor.h"
#include "UI/Effects/DelayReverbEditor.h"
#include "UI/Effects/SaturationEditor.h"
#include "UI/Effects/MultibandDynamicsEditor.h"
#include "Engine/DrumRackHelpers.h"
#include "Engine/Effects/NativeCustomPlugins.h"
#include "Engine/Effects/SaturationPlugin.h"
#include "Engine/Effects/MultibandDynamicsPlugin.h"
#include "Engine/EngineHelpers.h"
#include "Engine/PluginHost/SandboxedPluginInstance.h"
#include "Engine/NativePluginCatalog.h"

namespace skeletonhive
{

namespace
{
class SandboxPlaceholderPanel : public juce::Component
{
public:
    SandboxPlaceholderPanel (te::ExternalPlugin& externalPlugin)
        : external (externalPlugin)
    {
        addAndMakeVisible (messageLabel);
        messageLabel.setJustificationType (juce::Justification::centred);
        messageLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));

        addAndMakeVisible (openButton);
        openButton.setButtonText ("Open Bridge Editor");
        openButton.onClick = [this] { requestEditor(); };

        requestEditor();
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
            juce::String error;
            if (sandboxed->requestBridgeEditor (error))
            {
                messageLabel.setText ("This plugin is running in a separate sandbox process.\n"
                                      "Its editor should appear in a dedicated bridge window.\n"
                                      "Check the taskbar for a second SkeletonHive window if you do not see it.",
                                      juce::dontSendNotification);
                return;
            }

            messageLabel.setText ("Sandbox editor failed to open:\n" + error,
                                  juce::dontSendNotification);
            return;
        }

        messageLabel.setText ("Sandbox plugin instance is unavailable.",
                              juce::dontSendNotification);
    }

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
            auto w = std::make_unique<PluginWindow> (plugin);
            w->setSandboxPlaceholder (*externalPlugin);
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

    if (newEditor == nullptr && NativePluginCatalog::isNativePlugin (plugin))
        newEditor = NativePluginEditor::create (plugin);

    setEditor (std::move (newEditor));
    resizeToFitEditorContent();
}

void PluginWindow::setSandboxPlaceholder (te::ExternalPlugin& externalPlugin)
{
    setContentOwned (new SandboxPlaceholderPanel (externalPlugin), true);
    setResizable (true, false);
    setSize (460, 190);
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
