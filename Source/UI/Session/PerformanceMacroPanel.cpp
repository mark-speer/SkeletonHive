#include "PerformanceMacroPanel.h"
#include "Engine/EngineHelpers.h"

namespace skeletonhive
{

namespace
{
class MacroKnobComponent : public juce::Component,
                           private te::AutomatableParameter::Listener
{
public:
    MacroKnobComponent (te::Edit& e, te::AutomatableParameter& param)
        : edit (e), parameter (&param)
    {
        label.setText (param.getParameterName(), juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (10.0f));
        addAndMakeVisible (label);

        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange (0.0, 1.0, 0.001);
        slider.setValue (param.getCurrentValue(), juce::dontSendNotification);
        slider.onValueChange = [this]
        {
            if (parameter != nullptr)
                parameter->setParameter ((float) slider.getValue(), juce::sendNotification);
        };
        addAndMakeVisible (slider);

        parameter->addListener (this);
    }

    ~MacroKnobComponent() override
    {
        if (parameter != nullptr)
            parameter->removeListener (this);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (2);
        label.setBounds (r.removeFromBottom (14));
        slider.setBounds (r);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu() || parameter == nullptr)
            return;

        juce::PopupMenu menu;
        menu.addItem (1, "MIDI Learn...");
        if (EngineHelpers::isParameterMidiMapped (edit, *parameter))
            menu.addItem (2, "Remove MIDI Mapping");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [this, paramPtr = te::AutomatableParameter::Ptr (parameter)] (int result)
        {
            if (result == 1)
                EngineHelpers::startParameterMidiLearn (edit, *paramPtr);
            else if (result == 2)
                EngineHelpers::removeParameterMidiMapping (edit, *paramPtr);
        });
    }

private:
    void curveHasChanged (te::AutomatableParameter&) override {}
    void currentValueChanged (te::AutomatableParameter&) override
    {
        if (parameter != nullptr)
            slider.setValue (parameter->getCurrentValue(), juce::dontSendNotification);
    }

    te::Edit& edit;
    te::AutomatableParameter::Ptr parameter;
    juce::Label label;
    juce::Slider slider;
};

class RackMacroSection : public juce::Component
{
public:
    RackMacroSection (te::Edit& edit, te::RackInstance& rack)
    {
        title.setText (rack.getName(), juce::dontSendNotification);
        title.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
        title.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (title);

        for (auto macro : rack.type->getMacroParameters())
        {
            auto* knob = knobs.add (new MacroKnobComponent (edit, *macro));
            addAndMakeVisible (knob);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (4);
        title.setBounds (r.removeFromTop (18));

        const int knobSize = 72;
        const int columns = juce::jmax (1, r.getWidth() / knobSize);
        int x = r.getX();
        int y = r.getY();
        int col = 0;

        for (auto* knob : knobs)
        {
            knob->setBounds (x, y, knobSize, knobSize + 14);
            knob->setVisible (true);

            if (++col >= columns)
            {
                col = 0;
                x = r.getX();
                y += knobSize + 18;
            }
            else
            {
                x += knobSize;
            }
        }

        setSize (r.getWidth(), y + knobSize + 18 - r.getY());
    }

    int getPreferredHeight() const
    {
        const int knobSize = 72;
        const int columns = juce::jmax (1, getWidth() / knobSize);
        const int rows = (knobs.size() + columns - 1) / columns;
        return 22 + rows * (knobSize + 18);
    }

    juce::OwnedArray<MacroKnobComponent> knobs;
    juce::Label title;
};
} // namespace

PerformanceMacroPanel::PerformanceMacroPanel (te::Edit& e, EditViewState& viewState)
    : edit (e), editViewState (viewState)
{
    emptyLabel.setJustificationType (juce::Justification::centred);
    emptyLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.45f));
    addAndMakeVisible (emptyLabel);

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
}

void PerformanceMacroPanel::setFocusedTrack (te::EditItemID trackId)
{
    if (focusedTrackId == trackId)
        return;

    focusedTrackId = trackId;
    rebuild();
}

void PerformanceMacroPanel::rebuild()
{
    content.removeAllChildren();

    te::AudioTrack* audioTrack = nullptr;

    for (auto track : te::getAllTracks (edit))
    {
        if (track->itemID == focusedTrackId)
        {
            audioTrack = dynamic_cast<te::AudioTrack*> (track);
            break;
        }
    }

    bool hasMacros = false;
    int y = 0;

    if (audioTrack != nullptr)
    {
        for (auto plugin : audioTrack->pluginList)
        {
            if (auto* rack = dynamic_cast<te::RackInstance*> (plugin))
            {
                if (rack->type->getMacroParameters().isEmpty())
                    continue;

                auto* section = new RackMacroSection (edit, *rack);
                section->setBounds (0, y, juce::jmax (320, getWidth()), section->getPreferredHeight());
                content.addAndMakeVisible (section);
                y += section->getHeight();
                hasMacros = true;
            }
        }
    }

    content.setSize (juce::jmax (320, getWidth()), juce::jmax (y, 1));
    emptyLabel.setVisible (! hasMacros || focusedTrackId.isInvalid());
    viewport.setVisible (hasMacros && ! focusedTrackId.isInvalid());
}

void PerformanceMacroPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff151b28));
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawLine (0.0f, 0.0f, (float) getWidth(), 0.0f, 1.0f);
}

void PerformanceMacroPanel::resized()
{
    auto r = getLocalBounds();
    emptyLabel.setBounds (r);
    viewport.setBounds (r);
    rebuild();
}

} // namespace skeletonhive
