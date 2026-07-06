#pragma once

#include <optional>

#include "SessionManager.h"
#include "UI/Arrangement/EditViewState.h"

namespace skeletonhive
{

struct SessionMidiMapping
{
    SessionMidiTrigger triggerType = SessionMidiTrigger::noteOn;
    int channel = -1;
    int number = 0;
    SessionMidiAction action = SessionMidiAction::toggleSlot;
    te::EditItemID trackId;
    int sceneIndex = 0;
};

/** Discrete MIDI note/CC mappings to session slot launch actions. */
class SessionMidiMapper : private te::MidiInputDevice::MidiKeyChangeDispatcher::Listener
{
public:
    SessionMidiMapper (te::Edit& edit, EditViewState& viewState, SessionManager& sessionManager);
    ~SessionMidiMapper() override;

    juce::Array<SessionMidiMapping> getMappings() const;
    bool hasMappingForSlot (te::EditItemID trackId, int sceneIndex) const;
    void removeMappingsForSlot (te::EditItemID trackId, int sceneIndex);

    bool isLearnActive() const { return learnActive; }
    void cancelLearn();

    void armLearn (te::EditItemID trackId, int sceneIndex, SessionMidiAction action);

    juce::String getStatusText() const;

    void processMidiMessage (const juce::MidiMessage& message);

    std::function<void()> onStatusChanged;

private:
    struct LearnTarget
    {
        te::EditItemID trackId;
        int sceneIndex = 0;
        SessionMidiAction action = SessionMidiAction::toggleSlot;
    };

    void midiKeyStateChanged (te::AudioTrack*, const juce::Array<int>& notesOn,
                              const juce::Array<int>& vels, const juce::Array<int>& notesOff) override;

    void handleIncomingController (int channel, int controllerNumber, int value);
    void dispatchMapping (const SessionMidiMapping& mapping);
    void addMapping (const SessionMidiMapping& mapping);
    juce::ValueTree findMappingTree (te::EditItemID trackId, int sceneIndex, SessionMidiAction action) const;
    SessionMidiMapping mappingFromTree (const juce::ValueTree& tree) const;
    bool matchesMapping (const SessionMidiMapping& mapping, SessionMidiTrigger trigger, int channel, int number) const;

    te::Edit& edit;
    EditViewState& editViewState;
    SessionManager& sessionManager;

    juce::SharedResourcePointer<te::MidiInputDevice::MidiKeyChangeDispatcher> midiKeyDispatcher;
    std::optional<LearnTarget> learnTarget;
    bool learnActive = false;
    juce::Array<int> lastTriggeredCcValues;
};

} // namespace skeletonhive
