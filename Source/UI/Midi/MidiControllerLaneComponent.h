#pragma once

#include "MidiLaneViewport.h"

namespace skeletonhive
{

/** Editable lane for one MidiList controller type (CC number, pitch bend, or aftertouch). */
class MidiControllerLaneComponent : public juce::Component
{
public:
    MidiControllerLaneComponent (te::MidiClip& clip, MidiLaneViewport& viewport, int controllerType);

    void setControllerType (int newType);
    int getControllerType() const { return controllerType; }

    void sequenceChanged() { repaint(); }

private:
    struct ValueRange
    {
        int min = 0;
        int max = 127;
        int centre = 0;
        bool bipolar = false;
    };

    juce::Rectangle<int> getCurveArea() const;
    ValueRange getValueRange() const;

    juce::Array<te::MidiControllerEvent*> getEventsForType() const;
    te::MidiControllerEvent* eventAtPosition (juce::Point<int> pos) const;
    int hitTestPoint (juce::Point<int> pos) const;

    double clipRelativeBeat (const te::MidiControllerEvent& event) const;
    te::BeatPosition sequenceBeat (double clipRelativeBeat) const;

    int yToValue (int y) const;
    int valueToY (int value) const;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;

    te::MidiClip& clip;
    MidiLaneViewport& viewport;
    int controllerType;

    te::MidiControllerEvent* draggedEvent = nullptr;
    te::MidiControllerEvent* hoveredEvent = nullptr;
    bool pencilMode = false;

    static constexpr int pointHitRadius = 6;
    static constexpr int pitchWheelMax = 16383;
    static constexpr int pitchWheelCentre = 8192;
};

} // namespace skeletonhive
