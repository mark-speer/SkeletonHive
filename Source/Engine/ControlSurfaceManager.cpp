#include "ControlSurfaceManager.h"

#include "EngineHelpers.h"

namespace skeletonhive
{

namespace
{
constexpr int anyChannel = -1;
constexpr int tracksPerBank = 8;

int clampMidiChannel (int channel)
{
    return juce::jlimit (0, 15, channel);
}
} // namespace

class ControlSurfaceManager::MidiInputTap : public juce::MidiInputCallback
{
public:
    explicit MidiInputTap (ControlSurfaceManager& ownerIn) : owner (ownerIn) {}

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message) override
    {
        if (message.isController())
            owner.processController (message.getChannel() - 1, message.getControllerNumber(),
                                     message.getControllerValue());
    }

private:
    ControlSurfaceManager& owner;
};

ControlSurfaceManager::ControlSurfaceManager (te::Edit& e, EditViewState& viewState,
                                              TransportController& transport, SessionManager& session)
    : edit (e),
      editViewState (viewState),
      transportController (transport),
      sessionManager (session)
{
    controlState = editViewState.state.getOrCreateChildWithName (IDs::CONTROLSTATE, nullptr);
    faderBankOffsetValue.referTo (controlState, IDs::faderBankOffset, &edit.getUndoManager(), 0);
    faderBankOffset = faderBankOffsetValue.get();

    midiKeyDispatcher->listeners.add (this);
    sessionManager.addChangeListener (this);

    midiInputTap = std::make_unique<MidiInputTap> (*this);

    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        if (edit.engine.getDeviceManager().deviceManager.isMidiInputDeviceEnabled (device.identifier))
        {
            edit.engine.getDeviceManager().deviceManager.addMidiInputCallback (device.identifier, midiInputTap.get());
            registeredMidiInputIds.add (device.identifier);
        }
    }

    startTimerHz (15);
}

ControlSurfaceManager::~ControlSurfaceManager()
{
    stopTimer();

    for (const auto& id : registeredMidiInputIds)
        edit.engine.getDeviceManager().deviceManager.removeMidiInputCallback (id, midiInputTap.get());

    registeredMidiInputIds.clear();
    sessionManager.removeChangeListener (this);
    midiKeyDispatcher->listeners.remove (this);
}

void ControlSurfaceManager::setFaderBankOffset (int bank)
{
    faderBankOffset = juce::jmax (0, bank);
    faderBankOffsetValue = faderBankOffset;
}

juce::Array<ControlSurfaceBinding> ControlSurfaceManager::getBindings() const
{
    juce::Array<ControlSurfaceBinding> bindings;

    for (int i = 0; i < controlState.getNumChildren(); ++i)
    {
        const auto child = controlState.getChild (i);
        if (child.hasType (IDs::CONTROLSURFACEBINDING))
            bindings.add (bindingFromTree (child));
    }

    return bindings;
}

juce::Array<ControlSurfaceFeedbackBinding> ControlSurfaceManager::getFeedbackBindings() const
{
    juce::Array<ControlSurfaceFeedbackBinding> bindings;

    for (int i = 0; i < controlState.getNumChildren(); ++i)
    {
        const auto child = controlState.getChild (i);
        if (child.hasType (IDs::CONTROLFEEDBACK))
            bindings.add (feedbackFromTree (child));
    }

    return bindings;
}

ControlSurfaceBinding ControlSurfaceManager::bindingFromTree (const juce::ValueTree& tree) const
{
    ControlSurfaceBinding binding;
    binding.triggerType = static_cast<ControlSurfaceTrigger> (juce::jlimit (0, 1, (int) tree.getProperty (IDs::triggerType, 0)));
    binding.channel = (int) tree.getProperty (IDs::channel, anyChannel);
    binding.number = (int) tree.getProperty (IDs::number, 0);
    binding.target = static_cast<ControlSurfaceTarget> (juce::jlimit (0, (int) ControlSurfaceTarget::bankSelect,
                                                                        (int) tree.getProperty (IDs::targetType, 0)));
    binding.bankOffset = (int) tree.getProperty (IDs::bankOffset, 0);
    binding.relativeIndex = (int) tree.getProperty (IDs::relativeIndex, 0);
    binding.trackId = te::EditItemID::fromRawID ((juce::uint64) (juce::int64) tree.getProperty (IDs::trackId));
    binding.sceneIndex = (int) tree.getProperty (IDs::sceneIndex, 0);
    return binding;
}

ControlSurfaceFeedbackBinding ControlSurfaceManager::feedbackFromTree (const juce::ValueTree& tree) const
{
    ControlSurfaceFeedbackBinding binding;
    binding.trackId = te::EditItemID::fromRawID ((juce::uint64) (juce::int64) tree.getProperty (IDs::trackId));
    binding.sceneIndex = (int) tree.getProperty (IDs::sceneIndex, 0);
    binding.channel = juce::jlimit (1, 16, (int) tree.getProperty (IDs::channel, 1));
    binding.noteNumber = juce::jlimit (0, 127, (int) tree.getProperty (IDs::noteNumber, 36));
    return binding;
}

void ControlSurfaceManager::addBinding (const ControlSurfaceBinding& binding)
{
    juce::ValueTree tree (IDs::CONTROLSURFACEBINDING);
    tree.setProperty (IDs::triggerType, (int) binding.triggerType, nullptr);
    tree.setProperty (IDs::channel, binding.channel, nullptr);
    tree.setProperty (IDs::number, binding.number, nullptr);
    tree.setProperty (IDs::targetType, (int) binding.target, nullptr);
    tree.setProperty (IDs::bankOffset, binding.bankOffset, nullptr);
    tree.setProperty (IDs::relativeIndex, binding.relativeIndex, nullptr);
    tree.setProperty (IDs::trackId, (juce::int64) binding.trackId.getRawID(), nullptr);
    tree.setProperty (IDs::sceneIndex, binding.sceneIndex, nullptr);
    controlState.appendChild (tree, &edit.getUndoManager());
}

void ControlSurfaceManager::removeBinding (int index)
{
    int bindingIndex = 0;

    for (int i = 0; i < controlState.getNumChildren(); ++i)
    {
        const auto child = controlState.getChild (i);
        if (! child.hasType (IDs::CONTROLSURFACEBINDING))
            continue;

        if (bindingIndex++ == index)
        {
            controlState.removeChild (i, &edit.getUndoManager());
            break;
        }
    }
}

void ControlSurfaceManager::clearBindings()
{
    for (int i = controlState.getNumChildren(); --i >= 0;)
        if (controlState.getChild (i).hasType (IDs::CONTROLSURFACEBINDING))
            controlState.removeChild (i, &edit.getUndoManager());
}

void ControlSurfaceManager::addFeedbackBinding (const ControlSurfaceFeedbackBinding& binding)
{
    removeFeedbackBindingsForSlot (binding.trackId, binding.sceneIndex);

    juce::ValueTree tree (IDs::CONTROLFEEDBACK);
    tree.setProperty (IDs::trackId, (juce::int64) binding.trackId.getRawID(), nullptr);
    tree.setProperty (IDs::sceneIndex, binding.sceneIndex, nullptr);
    tree.setProperty (IDs::channel, binding.channel, nullptr);
    tree.setProperty (IDs::noteNumber, binding.noteNumber, nullptr);
    controlState.appendChild (tree, &edit.getUndoManager());
}

void ControlSurfaceManager::removeFeedbackBindingsForSlot (te::EditItemID trackId, int sceneIndex)
{
    for (int i = controlState.getNumChildren(); --i >= 0;)
    {
        const auto child = controlState.getChild (i);
        if (! child.hasType (IDs::CONTROLFEEDBACK))
            continue;

        if ((juce::int64) child.getProperty (IDs::trackId) == (juce::int64) trackId.getRawID()
            && (int) child.getProperty (IDs::sceneIndex) == sceneIndex)
            controlState.removeChild (i, &edit.getUndoManager());
    }
}

void ControlSurfaceManager::cancelLearn()
{
    learnActive = false;
    learnTarget.reset();

    if (onStatusChanged)
        onStatusChanged();
}

void ControlSurfaceManager::armLearn (ControlSurfaceTarget target, int bankOffsetIn,
                                      int relativeIndexIn, te::EditItemID trackId, int sceneIndex)
{
    edit.engine.getMidiLearnState().setActive (false);

    learnActive = true;
    learnTarget = LearnTarget {};
    learnTarget->target = target;
    learnTarget->bankOffset = bankOffsetIn;
    learnTarget->relativeIndex = relativeIndexIn;
    learnTarget->trackId = trackId;
    learnTarget->sceneIndex = sceneIndex;

    if (onStatusChanged)
        onStatusChanged();
}

void ControlSurfaceManager::armFeedbackLearn (te::EditItemID trackId, int sceneIndex)
{
    edit.engine.getMidiLearnState().setActive (false);

    learnActive = true;
    learnTarget = LearnTarget {};
    learnTarget->feedback = true;
    learnTarget->trackId = trackId;
    learnTarget->sceneIndex = sceneIndex;

    if (onStatusChanged)
        onStatusChanged();
}

juce::String ControlSurfaceManager::getStatusText() const
{
    if (! learnActive || ! learnTarget.has_value())
        return {};

    if (learnTarget->feedback)
        return "Control Surface: move a pad/note for slot feedback";

    return "Control Surface: move a controller for "
           + juce::String (learnTarget->relativeIndex >= 0 ? "bank control" : "transport/scene");
}

juce::Array<te::AudioTrack*> ControlSurfaceManager::getBankableTracks() const
{
    juce::Array<te::AudioTrack*> tracks;

    for (auto* track : te::getAudioTracks (edit))
    {
        if (track == nullptr || track->isFolderTrack() || EngineHelpers::isReturnTrack (*track))
            continue;

        tracks.add (track);
    }

    return tracks;
}

te::AudioTrack* ControlSurfaceManager::resolveBankTrack (int bankOffsetIn, int relativeIndexIn) const
{
    const auto tracks = getBankableTracks();
    const int index = bankOffsetIn * tracksPerBank + relativeIndexIn;

    if (juce::isPositiveAndBelow (index, tracks.size()))
        return tracks[index];

    return nullptr;
}

bool ControlSurfaceManager::matchesBinding (const ControlSurfaceBinding& binding,
                                            ControlSurfaceTrigger trigger, int channel, int number) const
{
    if (binding.triggerType != trigger)
        return false;

    if (binding.channel != anyChannel && binding.channel != channel)
        return false;

    return binding.number == number;
}

void ControlSurfaceManager::dispatchBinding (const ControlSurfaceBinding& binding, int value)
{
    switch (binding.target)
    {
        case ControlSurfaceTarget::bankSelect:
        {
            setFaderBankOffset (juce::jlimit (0, 127, value) / tracksPerBank);
            break;
        }

        case ControlSurfaceTarget::trackVolume:
        {
            if (auto* track = resolveBankTrack (binding.bankOffset >= 0 ? binding.bankOffset : faderBankOffset,
                                                binding.relativeIndex))
            {
                if (auto vol = track->getVolumePlugin())
                {
                    const float normalised = juce::jlimit (0.0f, 1.0f, value / 127.0f);
                    const float db = juce::Decibels::gainToDecibels (normalised, -100.0f);
                    vol->setVolumeDb (juce::jlimit (-100.0f, 6.0f, db));
                }
            }
            break;
        }

        case ControlSurfaceTarget::trackPan:
        {
            if (auto* track = resolveBankTrack (binding.bankOffset >= 0 ? binding.bankOffset : faderBankOffset,
                                                binding.relativeIndex))
            {
                if (auto vol = track->getVolumePlugin())
                {
                    const float pan = (value / 127.0f) * 2.0f - 1.0f;
                    vol->setPan (juce::jlimit (-1.0f, 1.0f, pan));
                }
            }
            break;
        }

        case ControlSurfaceTarget::trackMute:
        {
            if (auto* track = resolveBankTrack (binding.bankOffset >= 0 ? binding.bankOffset : faderBankOffset,
                                                binding.relativeIndex))
                track->setMute (value > 63);
            break;
        }

        case ControlSurfaceTarget::trackSolo:
        {
            if (auto* track = resolveBankTrack (binding.bankOffset >= 0 ? binding.bankOffset : faderBankOffset,
                                                binding.relativeIndex))
                track->setSolo (value > 63);
            break;
        }

        case ControlSurfaceTarget::transportPlay:
            if (value > 0) transportController.play();
            break;

        case ControlSurfaceTarget::transportStop:
            if (value > 0) transportController.stop();
            break;

        case ControlSurfaceTarget::transportRecord:
            if (value > 0) transportController.record();
            break;

        case ControlSurfaceTarget::transportTogglePlay:
            if (value > 0) transportController.togglePlay();
            break;

        case ControlSurfaceTarget::launchScene:
            if (value > 0)
                sessionManager.launchScene (binding.sceneIndex);
            break;

        case ControlSurfaceTarget::toggleSlot:
            if (value > 0)
                sessionManager.toggleSlot (binding.trackId, binding.sceneIndex);
            break;

        default:
            break;
    }
}

void ControlSurfaceManager::processController (int channel, int controllerNumber, int value)
{
    if (learnActive && learnTarget.has_value() && ! learnTarget->feedback && value > 0)
    {
        ControlSurfaceBinding binding;
        binding.triggerType = ControlSurfaceTrigger::cc;
        binding.channel = clampMidiChannel (channel);
        binding.number = controllerNumber;
        binding.target = learnTarget->target;
        binding.bankOffset = learnTarget->bankOffset;
        binding.relativeIndex = learnTarget->relativeIndex;
        binding.trackId = learnTarget->trackId;
        binding.sceneIndex = learnTarget->sceneIndex;
        addBinding (binding);
        cancelLearn();
        return;
    }

    for (const auto& binding : getBindings())
    {
        if (binding.triggerType != ControlSurfaceTrigger::cc)
            continue;

        if (! matchesBinding (binding, ControlSurfaceTrigger::cc, clampMidiChannel (channel), controllerNumber))
            continue;

        dispatchBinding (binding, value);
    }
}

void ControlSurfaceManager::midiKeyStateChanged (te::AudioTrack*, const juce::Array<int>& notesOn,
                                                 const juce::Array<int>& vels, const juce::Array<int>& notesOff)
{
    juce::ignoreUnused (notesOff, vels);

    if (learnActive && learnTarget.has_value() && ! notesOn.isEmpty())
    {
        const int note = notesOn.getFirst();

        if (learnTarget->feedback)
        {
            ControlSurfaceFeedbackBinding feedback;
            feedback.trackId = learnTarget->trackId;
            feedback.sceneIndex = learnTarget->sceneIndex;
            feedback.channel = 1;
            feedback.noteNumber = note;
            addFeedbackBinding (feedback);
        }
        else
        {
            ControlSurfaceBinding binding;
            binding.triggerType = ControlSurfaceTrigger::noteOn;
            binding.channel = anyChannel;
            binding.number = note;
            binding.target = learnTarget->target;
            binding.bankOffset = learnTarget->bankOffset;
            binding.relativeIndex = learnTarget->relativeIndex;
            binding.trackId = learnTarget->trackId;
            binding.sceneIndex = learnTarget->sceneIndex;
            addBinding (binding);
        }

        cancelLearn();
        return;
    }

    for (auto note : notesOn)
    {
        for (const auto& binding : getBindings())
        {
            if (binding.triggerType != ControlSurfaceTrigger::noteOn)
                continue;

            if (! matchesBinding (binding, ControlSurfaceTrigger::noteOn, anyChannel, note))
                continue;

            dispatchBinding (binding, 127);
        }
    }
}

void ControlSurfaceManager::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshFeedback();
}

void ControlSurfaceManager::timerCallback()
{
    refreshFeedback();
}

void ControlSurfaceManager::updateFeedbackOutput()
{
    const auto devices = juce::MidiOutput::getAvailableDevices();

    if (devices.isEmpty())
    {
        feedbackOutput.reset();
        feedbackDeviceId.clear();
        return;
    }

    if (feedbackDeviceId.isEmpty())
        feedbackDeviceId = devices.getFirst().identifier;

    if (feedbackOutput == nullptr || feedbackOutput->getDeviceInfo().identifier != feedbackDeviceId)
        feedbackOutput = juce::MidiOutput::openDevice (feedbackDeviceId);
}

void ControlSurfaceManager::sendFeedbackNote (int channel, int note, bool on)
{
    updateFeedbackOutput();

    if (feedbackOutput == nullptr)
        return;

    const int clampedChannel = juce::jlimit (1, 16, channel);
    const int clampedNote = juce::jlimit (0, 127, note);
    const int hashKey = (clampedChannel << 8) | clampedNote;
    const bool lastState = lastFeedbackState.contains (hashKey) ? lastFeedbackState[hashKey] : false;

    if (lastState == on)
        return;

    lastFeedbackState.set (hashKey, on);
    feedbackOutput->sendMessageNow (on ? juce::MidiMessage::noteOn (clampedChannel, clampedNote, (juce::uint8) 127)
                                      : juce::MidiMessage::noteOff (clampedChannel, clampedNote));
}

void ControlSurfaceManager::refreshFeedback()
{
    for (const auto& feedback : getFeedbackBindings())
    {
        const bool playing = sessionManager.isSlotPlaying (feedback.trackId, feedback.sceneIndex);
        sendFeedbackNote (feedback.channel, feedback.noteNumber, playing);
    }
}

void ControlSurfaceManager::installGenericFaderBankScript()
{
    if (! getBindings().isEmpty())
        return;

    for (int i = 0; i < tracksPerBank; ++i)
    {
        ControlSurfaceBinding volume;
        volume.triggerType = ControlSurfaceTrigger::cc;
        volume.channel = 0;
        volume.number = i + 1;
        volume.target = ControlSurfaceTarget::trackVolume;
        volume.relativeIndex = i;
        addBinding (volume);

        ControlSurfaceBinding pan;
        pan.triggerType = ControlSurfaceTrigger::cc;
        pan.channel = 0;
        pan.number = i + 9;
        pan.target = ControlSurfaceTarget::trackPan;
        pan.relativeIndex = i;
        addBinding (pan);
    }

    ControlSurfaceBinding play;
    play.triggerType = ControlSurfaceTrigger::noteOn;
    play.number = 36;
    play.target = ControlSurfaceTarget::transportTogglePlay;
    addBinding (play);

    ControlSurfaceBinding stop;
    stop.triggerType = ControlSurfaceTrigger::noteOn;
    stop.number = 37;
    stop.target = ControlSurfaceTarget::transportStop;
    addBinding (stop);
}

} // namespace skeletonhive
