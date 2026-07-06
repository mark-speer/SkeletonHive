#include "MidiLearnController.h"
#include "Engine/EngineHelpers.h"

namespace skeletonhive
{

namespace
{
class LearnStateListener : public te::MidiLearnState::Listener
{
public:
    explicit LearnStateListener (MidiLearnController& owner, te::MidiLearnState& state)
        : te::MidiLearnState::Listener (state), controller (owner) {}

    void midiLearnStatusChanged (bool) override
    {
        if (controller.onStatusChanged)
            controller.onStatusChanged();
    }

    MidiLearnController& controller;
};
} // namespace

MidiLearnController::MidiLearnController (te::Engine& engine)
    : engineRef (engine),
      learnListener (std::make_unique<LearnStateListener> (*this, engineRef.getMidiLearnState()))
{
}

bool MidiLearnController::isActive() const
{
    return engineRef.getMidiLearnState().isActive();
}

void MidiLearnController::setActive (bool shouldBeActive)
{
    engineRef.getMidiLearnState().setActive (shouldBeActive);

    if (! shouldBeActive)
    {
        pendingParameter = nullptr;
        activeEdit = nullptr;

        if (auto* edit = engineRef.getUIBehaviour().getLastFocusedEdit())
            edit->getParameterChangeHandler().setParameterLearnActive (false);
    }

    if (onStatusChanged)
        onStatusChanged();
}

void MidiLearnController::learnParameter (te::Edit& edit, te::AutomatableParameter& parameter)
{
    EngineHelpers::startParameterMidiLearn (edit, parameter);
    activeEdit = &edit;
    pendingParameter = &parameter;

    if (onStatusChanged)
        onStatusChanged();
}

void MidiLearnController::removeMappingForParameter (te::Edit& edit, te::AutomatableParameter& parameter)
{
    EngineHelpers::removeParameterMidiMapping (edit, parameter);
}

bool MidiLearnController::isParameterMapped (te::Edit& edit, te::AutomatableParameter& parameter) const
{
    return EngineHelpers::isParameterMidiMapped (edit, parameter);
}

juce::String MidiLearnController::getStatusText (te::Edit& edit) const
{
    if (! isActive())
        return {};

    if (pendingParameter != nullptr)
        return "MIDI Learn: move a controller for \"" + pendingParameter->getFullName() + "\"";

    auto& mappings = edit.getParameterControlMappings();
    const int row = mappings.getRowBeingListenedTo();

    if (row >= 0)
    {
        const auto text = mappings.getTextForRow (row);
        return "MIDI Learn: " + text.first + " -> " + text.second;
    }

    return "MIDI Learn active — adjust a control, then move a MIDI controller";
}

void MidiLearnController::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (onStatusChanged)
        onStatusChanged();
}

} // namespace skeletonhive
