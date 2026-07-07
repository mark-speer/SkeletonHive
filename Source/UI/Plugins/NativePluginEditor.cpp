#include "NativePluginEditor.h"
#include "Engine/EngineHelpers.h"
#include "Engine/NativePluginCatalog.h"

namespace skeletonhive
{

namespace
{

class ParameterRow : public juce::Component,
                     private te::AutomatableParameter::Listener
{
public:
    ParameterRow (te::Edit& editRef, te::AutomatableParameter& param)
        : edit (editRef), parameter (&param)
    {
        label.setText (param.getParameterName(), juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (label);

        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 18);
        slider.setRange (0.0, 1.0, 0.001);
        slider.setValue (param.getCurrentNormalisedValue(), juce::dontSendNotification);
        slider.textFromValueFunction = [this] (double value)
        {
            if (parameter != nullptr)
                return parameter->valueToString (parameter->valueRange.convertFrom0to1 ((float) value));

            return juce::String {};
        };
        slider.onValueChange = [this]
        {
            if (parameter != nullptr)
                parameter->setNormalisedParameter ((float) slider.getValue(), juce::sendNotification);
        };
        addAndMakeVisible (slider);

        parameter->addListener (this);
    }

    ~ParameterRow() override
    {
        if (parameter != nullptr)
            parameter->removeListener (this);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (2);
        label.setBounds (r.removeFromTop (18));
        slider.setBounds (r);
    }

    static int preferredHeight() { return 44; }

private:
    void curveHasChanged (te::AutomatableParameter&) override {}
    void currentValueChanged (te::AutomatableParameter&) override
    {
        if (parameter != nullptr)
            slider.setValue (parameter->getCurrentNormalisedValue(), juce::dontSendNotification);
    }

    te::Edit& edit;
    te::AutomatableParameter::Ptr parameter;
    juce::Label label;
    juce::Slider slider;
};

} // namespace

std::unique_ptr<te::Plugin::EditorComponent> NativePluginEditor::create (te::Plugin& plugin)
{
    if (! NativePluginCatalog::isNativePlugin (plugin))
        return {};

    return std::unique_ptr<te::Plugin::EditorComponent> (new NativePluginEditor (plugin));
}

NativePluginEditor::NativePluginEditor (te::Plugin& plug)
    : plugin (plug)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&content, false);
    rebuildContent();
    setSize (420, juce::jlimit (240, 720, content.getHeight() + 8));
}

void NativePluginEditor::rebuildContent()
{
    content.removeAllChildren();

    int y = 0;

    for (auto* param : plugin.getAutomatableParameters())
    {
        if (param == nullptr)
            continue;

        auto* row = new ParameterRow (plugin.edit, *param);
        row->setBounds (0, y, 400, ParameterRow::preferredHeight());
        content.addAndMakeVisible (row);
        y += ParameterRow::preferredHeight();
    }

    if (y == 0)
    {
        auto* label = new juce::Label {};
        label->setText ("No editable parameters exposed for this device.", juce::dontSendNotification);
        label->setJustificationType (juce::Justification::centred);
        label->setBounds (0, 0, 400, 48);
        content.addAndMakeVisible (label);
        y = 48;
    }

    content.setSize (400, y);
}

void NativePluginEditor::resized()
{
    viewport.setBounds (getLocalBounds());
}

} // namespace skeletonhive
