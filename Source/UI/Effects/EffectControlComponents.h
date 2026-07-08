#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

void configureAutomatableSlider (juce::Slider& slider);
void configureAutomatableRotary (juce::Slider& slider);

/** Horizontal slider row bound to a TE automatable parameter. */
class AutomatableSliderRow : public juce::Component,
                             private te::AutomatableParameter::Listener
{
public:
    AutomatableSliderRow (te::AutomatableParameter& param, std::function<bool()> isUpdating);
    ~AutomatableSliderRow() override;

    void resized() override;
    static int preferredHeight() { return 44; }

private:
    void curveHasChanged (te::AutomatableParameter&) override {}
    void currentValueChanged (te::AutomatableParameter&) override;

    te::AutomatableParameter::Ptr parameter;
    std::function<bool()> updatingCheck;
    juce::Label label;
    juce::Slider slider;
};

/** Rotary knob bound to a TE automatable parameter. */
class AutomatableRotaryRow : public juce::Component,
                             private te::AutomatableParameter::Listener
{
public:
    AutomatableRotaryRow (const juce::String& name, te::AutomatableParameter& param,
                          std::function<bool()> isUpdating);
    ~AutomatableRotaryRow() override;

    void resized() override;
    static int preferredSize() { return 72; }

private:
    void curveHasChanged (te::AutomatableParameter&) override {}
    void currentValueChanged (te::AutomatableParameter&) override;

    te::AutomatableParameter::Ptr parameter;
    std::function<bool()> updatingCheck;
    juce::Label label;
    juce::Slider slider;
};

/** Toggle bound to a juce::CachedValue<bool>. */
class BoolToggleRow : public juce::Component
{
public:
    BoolToggleRow (const juce::String& name, juce::CachedValue<bool>& value,
                   std::function<bool()> isUpdating);

    void refresh();
    void resized() override;
    static int preferredHeight() { return 28; }

private:
    juce::CachedValue<bool>& cachedValue;
    std::function<bool()> updatingCheck;
    juce::ToggleButton toggle;
};

/** Scrollable vertical stack for effect control rows. */
class EffectEditorScrollPanel : public juce::Component
{
public:
    explicit EffectEditorScrollPanel (int contentWidth = 400);

    template <typename RowType, typename... Args>
    RowType& addRow (Args&&... args)
    {
        auto row = std::make_unique<RowType> (std::forward<Args> (args)...);
        auto& ref = *row;
        content.addAndMakeVisible (row.release());
        relayout();
        return ref;
    }

    void clearRows();
    void relayout();
    void resized() override;

    juce::Component& getContent() { return content; }

private:
    juce::Viewport viewport;
    juce::Component content;
    int panelWidth;
};

} // namespace skeletonhive
