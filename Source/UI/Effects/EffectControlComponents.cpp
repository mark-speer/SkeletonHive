#include "EffectControlComponents.h"

namespace skeletonhive
{

void configureAutomatableSlider (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 18);
    slider.setRange (0.0, 1.0, 0.001);
}

void configureAutomatableRotary (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
    slider.setRange (0.0, 1.0, 0.001);
}

AutomatableSliderRow::AutomatableSliderRow (te::AutomatableParameter& param, std::function<bool()> isUpdating)
    : parameter (&param), updatingCheck (std::move (isUpdating))
{
    label.setText (param.getParameterName(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (label);

    configureAutomatableSlider (slider);
    slider.setValue (param.getCurrentNormalisedValue(), juce::dontSendNotification);
    slider.textFromValueFunction = [this] (double value)
    {
        if (parameter != nullptr)
            return parameter->valueToString (parameter->valueRange.convertFrom0to1 ((float) value));

        return juce::String {};
    };
    slider.onValueChange = [this]
    {
        if (parameter == nullptr || (updatingCheck != nullptr && updatingCheck()))
            return;

        parameter->setNormalisedParameter ((float) slider.getValue(), juce::sendNotification);
    };
    addAndMakeVisible (slider);

    parameter->addListener (this);
}

AutomatableSliderRow::~AutomatableSliderRow()
{
    if (parameter != nullptr)
        parameter->removeListener (this);
}

void AutomatableSliderRow::resized()
{
    auto r = getLocalBounds().reduced (2);
    label.setBounds (r.removeFromTop (18));
    slider.setBounds (r);
}

void AutomatableSliderRow::currentValueChanged (te::AutomatableParameter&)
{
    if (parameter != nullptr)
        slider.setValue (parameter->getCurrentNormalisedValue(), juce::dontSendNotification);
}

AutomatableRotaryRow::AutomatableRotaryRow (const juce::String& name, te::AutomatableParameter& param,
                                            std::function<bool()> isUpdating)
    : parameter (&param), updatingCheck (std::move (isUpdating))
{
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (label);

    configureAutomatableRotary (slider);
    slider.setValue (param.getCurrentNormalisedValue(), juce::dontSendNotification);
    slider.textFromValueFunction = [this] (double value)
    {
        if (parameter != nullptr)
            return parameter->valueToString (parameter->valueRange.convertFrom0to1 ((float) value));

        return juce::String {};
    };
    slider.onValueChange = [this]
    {
        if (parameter == nullptr || (updatingCheck != nullptr && updatingCheck()))
            return;

        parameter->setNormalisedParameter ((float) slider.getValue(), juce::sendNotification);
    };
    addAndMakeVisible (slider);

    parameter->addListener (this);
}

AutomatableRotaryRow::~AutomatableRotaryRow()
{
    if (parameter != nullptr)
        parameter->removeListener (this);
}

void AutomatableRotaryRow::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r.reduced (2));
}

void AutomatableRotaryRow::currentValueChanged (te::AutomatableParameter&)
{
    if (parameter != nullptr)
        slider.setValue (parameter->getCurrentNormalisedValue(), juce::dontSendNotification);
}

BoolToggleRow::BoolToggleRow (const juce::String& name, juce::CachedValue<bool>& value,
                              std::function<bool()> isUpdating)
    : cachedValue (value), updatingCheck (std::move (isUpdating))
{
    toggle.setButtonText (name);
    toggle.setToggleState (value.get(), juce::dontSendNotification);
    toggle.onClick = [this]
    {
        if (updatingCheck != nullptr && updatingCheck())
            return;

        cachedValue = toggle.getToggleState();
    };
    addAndMakeVisible (toggle);
}

void BoolToggleRow::refresh()
{
    toggle.setToggleState (cachedValue.get(), juce::dontSendNotification);
}

void BoolToggleRow::resized()
{
    toggle.setBounds (getLocalBounds().reduced (2));
}

EffectEditorScrollPanel::EffectEditorScrollPanel (int contentWidth)
    : panelWidth (contentWidth)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
}

void EffectEditorScrollPanel::clearRows()
{
    content.removeAllChildren();
    content.setSize (panelWidth, 0);
}

void EffectEditorScrollPanel::relayout()
{
    int y = 0;

    for (int i = 0; i < content.getNumChildComponents(); ++i)
    {
        auto* row = content.getChildComponent (i);
        const int h = dynamic_cast<AutomatableSliderRow*> (row) != nullptr ? AutomatableSliderRow::preferredHeight()
                     : dynamic_cast<BoolToggleRow*> (row) != nullptr ? BoolToggleRow::preferredHeight()
                     : row->getHeight() > 0 ? row->getHeight()
                     : AutomatableSliderRow::preferredHeight();
        row->setBounds (0, y, panelWidth, h);
        y += h;
    }

    content.setSize (panelWidth, juce::jmax (y, 1));
}

void EffectEditorScrollPanel::resized()
{
    viewport.setBounds (getLocalBounds());
}

} // namespace skeletonhive
