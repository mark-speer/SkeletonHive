#include "AutomationLaneComponent.h"
#include "UI/Arrangement/EditViewState.h"

namespace arrange
{

AutomationLaneComponent::AutomationLaneComponent (te::AutomatableParameter& param, te::Edit& e,
                                                  EditViewState& vs)
    : parameter (param), edit (e), editViewState (vs)
{
}

void AutomationLaneComponent::setAutomationMode (AutomationMode mode)
{
    automationMode = mode;
    isRecordingAutomation = (mode == AutomationMode::touch || mode == AutomationMode::latch);
}

void AutomationLaneComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll (juce::Colour (0xff16213e));

    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawText (parameter.getParameterName(), bounds.removeFromTop (16), juce::Justification::centredLeft, true);

    auto& curve = parameter.getCurve();
    {
        juce::Path path;
        const int width = getWidth();
        const int height = getHeight() - 16;
        bool started = false;
        const float defaultValue = parameter.getCurrentBaseValue();

        for (int x = 0; x < width; ++x)
        {
            const auto time = xToTime (x);
            const float value = curve.getValueAt (time, defaultValue);
            const float y = (float) (16 + valueToY (value));

            if (! started)
            {
                path.startNewSubPath ((float) x, y);
                started = true;
            }
            else
            {
                path.lineTo ((float) x, y);
            }
        }

        g.setColour (juce::Colour (0xffe9c46a));
        g.strokePath (path, juce::PathStrokeType (1.5f));
    }
}

void AutomationLaneComponent::mouseDown (const juce::MouseEvent& e)
{
    if (automationMode == AutomationMode::read)
        return;

    const auto time = xToTime (e.x);
    const float value = yToValue (e.y);

    auto& curve = parameter.getCurve();
    curve.addPoint (time, value, 0.0f, &edit.getUndoManager());
    if (isRecordingAutomation && edit.getTransport().isPlaying())
        parameter.setParameter (value, juce::sendNotification);
    repaint();
}

void AutomationLaneComponent::mouseDrag (const juce::MouseEvent& e)
{
    mouseDown (e);
}

float AutomationLaneComponent::yToValue (int y) const
{
    const int height = juce::jmax (1, getHeight() - 16);
    return 1.0f - juce::jlimit (0.0f, 1.0f, (float) (y - 16) / (float) height);
}

int AutomationLaneComponent::valueToY (float value) const
{
    const int height = juce::jmax (1, getHeight() - 16);
    return (int) ((1.0f - value) * (float) height);
}

te::TimePosition AutomationLaneComponent::xToTime (int x) const
{
    return editViewState.xToTime (x);
}

} // namespace arrange
