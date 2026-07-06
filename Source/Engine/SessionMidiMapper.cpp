#include "SessionMidiMapper.h"

namespace skeletonhive
{

namespace
{
constexpr int anyChannel = -1;
}

SessionMidiMapper::SessionMidiMapper (te::Edit& e, EditViewState& viewState, SessionManager& session)
    : edit (e),
      editViewState (viewState),
      sessionManager (session)
{
    midiKeyDispatcher->listeners.add (this);
}

SessionMidiMapper::~SessionMidiMapper()
{
    midiKeyDispatcher->listeners.remove (this);
}

juce::Array<SessionMidiMapping> SessionMidiMapper::getMappings() const
{
    juce::Array<SessionMidiMapping> mappings;

    for (int i = 0; i < editViewState.sessionState.getNumChildren(); ++i)
    {
        const auto child = editViewState.sessionState.getChild (i);
        if (child.hasType (IDs::SESSIONMIDIMAPPING))
            mappings.add (mappingFromTree (child));
    }

    return mappings;
}

SessionMidiMapping SessionMidiMapper::mappingFromTree (const juce::ValueTree& tree) const
{
    SessionMidiMapping mapping;
    mapping.triggerType = static_cast<SessionMidiTrigger> (juce::jlimit (0, 1, (int) tree.getProperty (IDs::triggerType, 0)));
    mapping.channel = (int) tree.getProperty (IDs::channel, anyChannel);
    mapping.number = (int) tree.getProperty (IDs::number, 0);
    mapping.action = static_cast<SessionMidiAction> (juce::jlimit (0, 2, (int) tree.getProperty (IDs::action, 0)));
    mapping.trackId = te::EditItemID::fromRawID ((juce::uint64) (juce::int64) tree.getProperty (IDs::trackId));
    mapping.sceneIndex = (int) tree.getProperty (IDs::sceneIndex, 0);
    return mapping;
}

bool SessionMidiMapper::hasMappingForSlot (te::EditItemID trackId, int sceneIndex) const
{
    for (const auto& mapping : getMappings())
    {
        if (mapping.trackId == trackId && mapping.sceneIndex == sceneIndex)
            return true;
    }

    return false;
}

void SessionMidiMapper::removeMappingsForSlot (te::EditItemID trackId, int sceneIndex)
{
    for (int i = editViewState.sessionState.getNumChildren(); --i >= 0;)
    {
        const auto child = editViewState.sessionState.getChild (i);
        if (! child.hasType (IDs::SESSIONMIDIMAPPING))
            continue;

        if ((juce::int64) child.getProperty (IDs::trackId) == (juce::int64) trackId.getRawID()
            && (int) child.getProperty (IDs::sceneIndex) == sceneIndex)
            editViewState.sessionState.removeChild (i, &edit.getUndoManager());
    }

    if (onStatusChanged)
        onStatusChanged();
}

juce::ValueTree SessionMidiMapper::findMappingTree (te::EditItemID trackId, int sceneIndex,
                                                    SessionMidiAction action) const
{
    for (int i = 0; i < editViewState.sessionState.getNumChildren(); ++i)
    {
        const auto child = editViewState.sessionState.getChild (i);
        if (! child.hasType (IDs::SESSIONMIDIMAPPING))
            continue;

        if ((juce::int64) child.getProperty (IDs::trackId) == (juce::int64) trackId.getRawID()
            && (int) child.getProperty (IDs::sceneIndex) == sceneIndex
            && (int) child.getProperty (IDs::action) == (int) action)
            return child;
    }

    return {};
}

void SessionMidiMapper::addMapping (const SessionMidiMapping& mapping)
{
    removeMappingsForSlot (mapping.trackId, mapping.sceneIndex);

    juce::ValueTree node (IDs::SESSIONMIDIMAPPING);
    node.setProperty (IDs::triggerType, (int) mapping.triggerType, &edit.getUndoManager());
    node.setProperty (IDs::channel, mapping.channel, &edit.getUndoManager());
    node.setProperty (IDs::number, mapping.number, &edit.getUndoManager());
    node.setProperty (IDs::action, (int) mapping.action, &edit.getUndoManager());
    node.setProperty (IDs::trackId, (juce::int64) mapping.trackId.getRawID(), &edit.getUndoManager());
    node.setProperty (IDs::sceneIndex, mapping.sceneIndex, &edit.getUndoManager());
    editViewState.sessionState.appendChild (node, &edit.getUndoManager());
}

void SessionMidiMapper::armLearn (te::EditItemID trackId, int sceneIndex, SessionMidiAction action)
{
    edit.engine.getMidiLearnState().setActive (false);
    learnTarget = LearnTarget { trackId, sceneIndex, action };
    learnActive = true;

    if (onStatusChanged)
        onStatusChanged();
}

void SessionMidiMapper::cancelLearn()
{
    learnTarget.reset();
    learnActive = false;

    if (onStatusChanged)
        onStatusChanged();
}

juce::String SessionMidiMapper::getStatusText() const
{
    if (! learnActive || ! learnTarget.has_value())
        return {};

    const juce::String actionName = learnTarget->action == SessionMidiAction::launchSlot ? "Launch"
                                 : learnTarget->action == SessionMidiAction::stopSlot ? "Stop"
                                 : "Toggle";

    return "Session MIDI Learn: move a note or CC for slot " + actionName;
}

bool SessionMidiMapper::matchesMapping (const SessionMidiMapping& mapping, SessionMidiTrigger trigger,
                                        int channel, int number) const
{
    if (mapping.triggerType != trigger || mapping.number != number)
        return false;

    return mapping.channel == anyChannel || mapping.channel == channel;
}

void SessionMidiMapper::dispatchMapping (const SessionMidiMapping& mapping)
{
    switch (mapping.action)
    {
        case SessionMidiAction::toggleSlot:
            sessionManager.toggleSlot (mapping.trackId, mapping.sceneIndex);
            break;
        case SessionMidiAction::launchSlot:
            sessionManager.launchSlot (mapping.trackId, mapping.sceneIndex);
            break;
        case SessionMidiAction::stopSlot:
            sessionManager.stopSlot (mapping.trackId, mapping.sceneIndex);
            break;
        default:
            break;
    }
}

void SessionMidiMapper::processMidiMessage (const juce::MidiMessage& message)
{
    if (message.isController() && message.getControllerValue() > 0)
        handleIncomingController (message.getChannel() - 1, message.getControllerNumber(),
                                  message.getControllerValue());
}

void SessionMidiMapper::handleIncomingController (int channel, int controllerNumber, int value)
{
    if (learnActive && learnTarget.has_value() && value > 0)
    {
        SessionMidiMapping mapping;
        mapping.triggerType = SessionMidiTrigger::cc;
        mapping.channel = channel;
        mapping.number = controllerNumber;
        mapping.action = learnTarget->action;
        mapping.trackId = learnTarget->trackId;
        mapping.sceneIndex = learnTarget->sceneIndex;
        addMapping (mapping);
        cancelLearn();
        return;
    }

    if (value <= 0)
        return;

    for (const auto& mapping : getMappings())
    {
        if (mapping.triggerType != SessionMidiTrigger::cc)
            continue;

        if (! matchesMapping (mapping, SessionMidiTrigger::cc, channel, controllerNumber))
            continue;

        const int mapIndex = mapping.number + (mapping.channel + 1) * 128;
        if (mapIndex >= 0 && mapIndex < lastTriggeredCcValues.size()
            && lastTriggeredCcValues.getReference (mapIndex) > 0)
            continue;

        while (lastTriggeredCcValues.size() <= mapIndex)
            lastTriggeredCcValues.add (0);

        lastTriggeredCcValues.set (mapIndex, value);
        dispatchMapping (mapping);
    }
}

void SessionMidiMapper::midiKeyStateChanged (te::AudioTrack*, const juce::Array<int>& notesOn,
                                             const juce::Array<int>& vels, const juce::Array<int>& notesOff)
{
    juce::ignoreUnused (vels, notesOff);

    if (learnActive && learnTarget.has_value() && ! notesOn.isEmpty())
    {
        SessionMidiMapping mapping;
        mapping.triggerType = SessionMidiTrigger::noteOn;
        mapping.channel = anyChannel;
        mapping.number = notesOn.getFirst();
        mapping.action = learnTarget->action;
        mapping.trackId = learnTarget->trackId;
        mapping.sceneIndex = learnTarget->sceneIndex;
        addMapping (mapping);
        cancelLearn();
        return;
    }

    for (auto note : notesOn)
    {
        for (const auto& mapping : getMappings())
        {
            if (mapping.triggerType != SessionMidiTrigger::noteOn)
                continue;

            if (! matchesMapping (mapping, SessionMidiTrigger::noteOn, anyChannel, note))
                continue;

            dispatchMapping (mapping);
        }
    }

    for (auto& ccValue : lastTriggeredCcValues)
        ccValue = 0;

    juce::ignoreUnused (edit);
}

} // namespace skeletonhive
