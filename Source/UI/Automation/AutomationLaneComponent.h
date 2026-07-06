#pragma once

#include "TracktionCommon.h"
#include "UI/Arrangement/EditViewState.h"

namespace skeletonhive
{

enum class AutomationMode { read, touch, latch };

class AutomationLaneComponent : public juce::Component
{
public:
    AutomationLaneComponent (te::AutomatableParameter& param, te::Edit& edit, EditViewState& viewState);

    void setAutomationMode (AutomationMode mode);
    AutomationMode getAutomationMode() const { return automationMode; }

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

private:
    te::AutomatableParameter& parameter;
    te::Edit& edit;
    EditViewState& editViewState;
    AutomationMode automationMode = AutomationMode::read;
    bool isRecordingAutomation = false;

    float yToValue (int y) const;
    int valueToY (float value) const;
    te::TimePosition xToTime (int x) const;
};

} // namespace skeletonhive
