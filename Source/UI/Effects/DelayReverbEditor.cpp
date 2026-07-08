#include "DelayReverbEditor.h"

namespace skeletonhive
{

namespace
{

void addAutomatableRows (EffectEditorScrollPanel& panel, te::Plugin& plugin, std::function<bool()> isUpdating)
{
    for (auto* param : plugin.getAutomatableParameters())
    {
        if (param != nullptr)
            panel.addRow<AutomatableSliderRow> (*param, isUpdating);
    }
}

class ReverbSliderRow : public juce::Component
{
public:
    ReverbSliderRow (const juce::String& name, std::function<float()> getter,
                     std::function<void (float)> setter, float minValue, float maxValue)
        : getValue (std::move (getter)), setValue (std::move (setter))
    {
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (label);

        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 18);
        slider.setRange (minValue, maxValue, 0.01);
        slider.setValue (getValue(), juce::dontSendNotification);
        slider.onValueChange = [this]
        {
            if (setValue != nullptr)
                setValue ((float) slider.getValue());
        };
        addAndMakeVisible (slider);
    }

    void refresh()
    {
        slider.setValue (getValue(), juce::dontSendNotification);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (2);
        label.setBounds (r.removeFromTop (18));
        slider.setBounds (r);
    }

    static int preferredHeight() { return 44; }

private:
    std::function<float()> getValue;
    std::function<void (float)> setValue;
    juce::Label label;
    juce::Slider slider;
};

} // namespace

std::unique_ptr<te::Plugin::EditorComponent> DelayEditor::create (te::DelayPlugin& delayPlugin)
{
    return std::unique_ptr<te::Plugin::EditorComponent> (new DelayEditor (delayPlugin));
}

DelayEditor::DelayEditor (te::DelayPlugin& delayPlugin)
    : delay (delayPlugin),
      controls (420)
{
    delay.state.addListener (this);

    titleLabel.setText ("Delay", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);
    addAndMakeVisible (controls);

    auto isUpdating = [this] { return updatingFromModel; };
    addAutomatableRows (controls, delay, isUpdating);

    setSize (460, 280);
}

DelayEditor::~DelayEditor()
{
    delay.state.removeListener (this);
}

void DelayEditor::resized()
{
    auto r = getLocalBounds().reduced (8);
    titleLabel.setBounds (r.removeFromTop (24));
    controls.setBounds (r);
}

std::unique_ptr<te::Plugin::EditorComponent> ReverbEditor::create (te::ReverbPlugin& reverbPlugin)
{
    return std::unique_ptr<te::Plugin::EditorComponent> (new ReverbEditor (reverbPlugin));
}

ReverbEditor::ReverbEditor (te::ReverbPlugin& reverbPlugin)
    : reverb (reverbPlugin),
      controls (420)
{
    reverb.state.addListener (this);

    titleLabel.setText ("Reverb", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);
    addAndMakeVisible (controls);

    auto& panel = controls;
    panel.addRow<ReverbSliderRow> ("Room Size", [this] { return reverb.getRoomSize(); },
                                   [this] (float v) { reverb.setRoomSize (v); }, 0.0f, 1.0f);
    panel.addRow<ReverbSliderRow> ("Damp", [this] { return reverb.getDamp(); },
                                   [this] (float v) { reverb.setDamp (v); }, 0.0f, 1.0f);
    panel.addRow<ReverbSliderRow> ("Wet", [this] { return reverb.getWet(); },
                                   [this] (float v) { reverb.setWet (v); }, 0.0f, 1.0f);
    panel.addRow<ReverbSliderRow> ("Dry", [this] { return reverb.getDry(); },
                                   [this] (float v) { reverb.setDry (v); }, 0.0f, 1.0f);
    panel.addRow<ReverbSliderRow> ("Width", [this] { return reverb.getWidth(); },
                                   [this] (float v) { reverb.setWidth (v); }, 0.0f, 1.0f);
    panel.addRow<ReverbSliderRow> ("Mode", [this] { return reverb.getMode(); },
                                   [this] (float v) { reverb.setMode (v); }, 0.0f, 1.0f);

    setSize (460, 340);
}

ReverbEditor::~ReverbEditor()
{
    reverb.state.removeListener (this);
}

void ReverbEditor::resized()
{
    auto r = getLocalBounds().reduced (8);
    titleLabel.setBounds (r.removeFromTop (24));
    controls.setBounds (r);
}

} // namespace skeletonhive
