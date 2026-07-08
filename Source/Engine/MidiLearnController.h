#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

/** Thin wrapper around TE MIDI learn / parameter mapping.
    For fader-bank, transport, and session-grid surface bindings see ControlSurfaceManager. */
class MidiLearnController : public juce::ChangeListener
{
public:
    explicit MidiLearnController (te::Engine& engine);

    bool isActive() const;
    void setActive (bool shouldBeActive);

    /** Arm learn mode for a specific automatable parameter. */
    void learnParameter (te::Edit& edit, te::AutomatableParameter& parameter);

    void removeMappingForParameter (te::Edit& edit, te::AutomatableParameter& parameter);
    bool isParameterMapped (te::Edit& edit, te::AutomatableParameter& parameter) const;

    juce::String getStatusText (te::Edit& edit) const;

    std::function<void()> onStatusChanged;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    te::Engine& engineRef;
    te::Edit* activeEdit = nullptr;
    te::AutomatableParameter::Ptr pendingParameter;
    std::unique_ptr<te::MidiLearnState::Listener> learnListener;
};

} // namespace skeletonhive
