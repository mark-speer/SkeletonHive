#pragma once

#include "SessionManager.h"
#include "TransportController.h"
#include "UI/Arrangement/EditViewState.h"

namespace skeletonhive
{

enum class ControlSurfaceTrigger
{
    cc = 0,
    noteOn
};

enum class ControlSurfaceTarget
{
    trackVolume = 0,
    trackPan,
    trackMute,
    trackSolo,
    transportPlay,
    transportStop,
    transportRecord,
    transportTogglePlay,
    launchScene,
    toggleSlot,
    bankSelect
};

struct ControlSurfaceBinding
{
    ControlSurfaceTrigger triggerType = ControlSurfaceTrigger::cc;
    int channel = 0;
    int number = 0;
    ControlSurfaceTarget target = ControlSurfaceTarget::trackVolume;
    int bankOffset = 0;
    int relativeIndex = 0;
    te::EditItemID trackId;
    int sceneIndex = 0;
};

struct ControlSurfaceFeedbackBinding
{
    te::EditItemID trackId;
    int sceneIndex = 0;
    int channel = 1;
    int noteNumber = 36;
};

/** Generic MIDI control-surface bindings: fader banks, transport, session feedback. */
class ControlSurfaceManager : private te::MidiInputDevice::MidiKeyChangeDispatcher::Listener,
                              private juce::ChangeListener,
                              private juce::Timer
{
public:
    ControlSurfaceManager (te::Edit& edit, EditViewState& viewState,
                           TransportController& transport, SessionManager& sessionManager);
    ~ControlSurfaceManager() override;

    int getFaderBankOffset() const { return faderBankOffset; }
    void setFaderBankOffset (int bank);

    juce::Array<ControlSurfaceBinding> getBindings() const;
    juce::Array<ControlSurfaceFeedbackBinding> getFeedbackBindings() const;

    void addBinding (const ControlSurfaceBinding& binding);
    void removeBinding (int index);
    void clearBindings();

    void addFeedbackBinding (const ControlSurfaceFeedbackBinding& binding);
    void removeFeedbackBindingsForSlot (te::EditItemID trackId, int sceneIndex);

    bool isLearnActive() const { return learnActive; }
    void cancelLearn();
    void armLearn (ControlSurfaceTarget target, int bankOffset = 0, int relativeIndex = 0,
                   te::EditItemID trackId = {}, int sceneIndex = 0);
    void armFeedbackLearn (te::EditItemID trackId, int sceneIndex);

    juce::String getStatusText() const;

    void processController (int channel, int controllerNumber, int value);
    void refreshFeedback();

    /** Installs CC 1–8 / 9–16 volume/pan bindings for the current fader bank (channel 1). */
    void installGenericFaderBankScript();

    std::function<void()> onStatusChanged;

private:
    struct LearnTarget
    {
        bool feedback = false;
        ControlSurfaceTarget target = ControlSurfaceTarget::trackVolume;
        int bankOffset = 0;
        int relativeIndex = 0;
        te::EditItemID trackId;
        int sceneIndex = 0;
    };

    void midiKeyStateChanged (te::AudioTrack*, const juce::Array<int>& notesOn,
                              const juce::Array<int>& vels, const juce::Array<int>& notesOff) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    ControlSurfaceBinding bindingFromTree (const juce::ValueTree& tree) const;
    ControlSurfaceFeedbackBinding feedbackFromTree (const juce::ValueTree& tree) const;
    juce::ValueTree findBindingTree (const ControlSurfaceBinding& binding) const;

    bool matchesBinding (const ControlSurfaceBinding& binding, ControlSurfaceTrigger trigger,
                         int channel, int number) const;
    void dispatchBinding (const ControlSurfaceBinding& binding, int value);
    te::AudioTrack* resolveBankTrack (int bankOffset, int relativeIndex) const;
    juce::Array<te::AudioTrack*> getBankableTracks() const;

    void sendFeedbackNote (int channel, int note, bool on);
    void updateFeedbackOutput();

    te::Edit& edit;
    EditViewState& editViewState;
    TransportController& transportController;
    SessionManager& sessionManager;

    juce::ValueTree controlState;
    juce::CachedValue<int> faderBankOffsetValue;
    int faderBankOffset = 0;

    juce::SharedResourcePointer<te::MidiInputDevice::MidiKeyChangeDispatcher> midiKeyDispatcher;
    std::optional<LearnTarget> learnTarget;
    bool learnActive = false;

    std::unique_ptr<juce::MidiOutput> feedbackOutput;
    juce::String feedbackDeviceId;
    juce::HashMap<int, bool> lastFeedbackState;

    class MidiInputTap;
    std::unique_ptr<MidiInputTap> midiInputTap;
    juce::StringArray registeredMidiInputIds;
};

} // namespace skeletonhive
