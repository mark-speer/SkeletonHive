#include "AppCommands.h"

namespace skeletonhive
{

void AppCommands::registerAllCommands (juce::ApplicationCommandManager& manager)
{
    auto reg = [&] (juce::CommandID id, const char* name, const char* desc)
    {
        juce::ApplicationCommandInfo info (id);
        info.setInfo (name, desc, "SkeletonHive", 0);
        manager.registerCommand (info);
    };

    reg (AppCommandIDs::play, "Play/Pause", "Toggle playback");
    reg (AppCommandIDs::stop, "Stop", "Stop playback");
    reg (AppCommandIDs::record, "Record", "Toggle recording");
    reg (AppCommandIDs::undo, "Undo", "Undo last edit");
    reg (AppCommandIDs::redo, "Redo", "Redo last undone edit");
    reg (AppCommandIDs::saveProject, "Save Project", "Save the current project");
    reg (AppCommandIDs::saveProjectAs, "Save Project As", "Save the project to a new file");
    reg (AppCommandIDs::exportProject, "Export", "Bounce the project to an audio file");
    reg (AppCommandIDs::showPreferences, "Preferences", "Open application preferences");
    reg (AppCommandIDs::toggleMidiLearn, "MIDI Learn", "Toggle MIDI controller learn mode");

    reg (AppCommandIDs::toggleGrid, "Toggle Grid", "Show or hide the arrangement grid");
    reg (AppCommandIDs::duplicateClips, "Duplicate Clips", "Duplicate selected clips");
    reg (AppCommandIDs::groupClips, "Group Clips", "Group selected clips");
    reg (AppCommandIDs::ungroupClips, "Ungroup Clips", "Remove clip grouping");
    reg (AppCommandIDs::toggleRipple, "Toggle Ripple", "Toggle ripple editing mode");
    reg (AppCommandIDs::deleteTimelineSelection, "Delete Selection", "Delete selected clips");
    reg (AppCommandIDs::addMarker, "Add Marker", "Add an arrangement marker at the playhead");
    reg (AppCommandIDs::prevMarker, "Previous Marker", "Jump to the previous marker");
    reg (AppCommandIDs::nextMarker, "Next Marker", "Jump to the next marker");
    reg (AppCommandIDs::toggleTakeLanes, "Toggle Take Lanes", "Show or hide take lanes for the selected clip");
    reg (AppCommandIDs::consolidateClips, "Consolidate", "Bounce selected clips in place");
    reg (AppCommandIDs::applyGrooveToClips, "Apply Groove", "Apply the selected groove to MIDI clips");
    reg (AppCommandIDs::toggleDetailDevices, "Show Devices", "Show device chain in detail panel");
    reg (AppCommandIDs::toggleDetailClip, "Show Clip", "Show clip inspector in detail panel");
    reg (AppCommandIDs::toggleMainView, "Toggle Session View", "Switch between Arrangement and Session view");
    reg (AppCommandIDs::toggleRecordToArrangement, "Record to Arrangement", "Record session launches into the arrangement timeline");
    reg (AppCommandIDs::captureSessionToArrangement, "Capture and Insert", "Capture playing session clips into the arrangement at the write position");
    reg (AppCommandIDs::duplicateSessionLoopToArrangement, "Commit Loop to Arrangement", "Commit a session clip loop to the arrangement timeline");

    reg (AppCommandIDs::pluginCopy, "Copy Plugin", "Copy the selected plugin");
    reg (AppCommandIDs::pluginPaste, "Paste Plugin", "Paste a copied plugin");
    reg (AppCommandIDs::pluginDuplicate, "Duplicate Plugin", "Duplicate the selected plugin");
    reg (AppCommandIDs::pluginDelete, "Delete Plugin", "Remove the selected plugin");

    reg (AppCommandIDs::pianoDeleteNotes, "Delete Notes", "Delete selected notes");
    reg (AppCommandIDs::pianoSelectAll, "Select All Notes", "Select all notes in the clip");
    reg (AppCommandIDs::pianoDuplicateNotes, "Duplicate Notes", "Duplicate selected notes");
    reg (AppCommandIDs::pianoQuantize, "Quantize Notes", "Quantize selected notes");
    reg (AppCommandIDs::pianoHumanize, "Humanize Notes", "Humanize selected notes");
    reg (AppCommandIDs::pianoToggleFold, "Toggle Fold", "Fold the piano roll to active notes");
    reg (AppCommandIDs::pianoToggleScaleSnap, "Toggle Scale Snap", "Toggle scale-aware note input");
    reg (AppCommandIDs::pianoToggleDrawTool, "Toggle Draw Tool", "Toggle draw tool in piano roll");
    reg (AppCommandIDs::pianoToggleStepTool, "Toggle Step Tool", "Toggle step input in piano roll");
    reg (AppCommandIDs::pianoEscape, "Clear Selection", "Clear piano roll selection");
    reg (AppCommandIDs::pianoNudgeLeft, "Nudge Left", "Nudge notes left by one grid step");
    reg (AppCommandIDs::pianoNudgeRight, "Nudge Right", "Nudge notes right by one grid step");
    reg (AppCommandIDs::pianoNudgeUp, "Nudge Up", "Nudge notes up by one semitone");
    reg (AppCommandIDs::pianoNudgeDown, "Nudge Down", "Nudge notes down by one semitone");
    reg (AppCommandIDs::pianoStepRest, "Step Rest", "Advance step cursor without inserting a note");
}

void AppCommands::registerDefaultKeyMappings (juce::KeyPressMappingSet& mappings)
{
    mappings.clearAllKeyPresses();

    const auto cmd = juce::ModifierKeys::commandModifier;
    const auto ctrl = juce::ModifierKeys::ctrlModifier;
    const auto shift = juce::ModifierKeys::shiftModifier;
    const auto alt = juce::ModifierKeys::altModifier;

    mappings.addKeyPress (AppCommandIDs::play, juce::KeyPress (juce::KeyPress::spaceKey));
    mappings.addKeyPress (AppCommandIDs::record, juce::KeyPress ('r'));

    mappings.addKeyPress (AppCommandIDs::undo, juce::KeyPress ('z', cmd, 0));
    mappings.addKeyPress (AppCommandIDs::undo, juce::KeyPress ('z', ctrl, 0));
    mappings.addKeyPress (AppCommandIDs::redo, juce::KeyPress ('z', cmd | shift, 0));
    mappings.addKeyPress (AppCommandIDs::redo, juce::KeyPress ('z', ctrl | shift, 0));
    mappings.addKeyPress (AppCommandIDs::redo, juce::KeyPress ('y', cmd, 0));
    mappings.addKeyPress (AppCommandIDs::redo, juce::KeyPress ('y', ctrl, 0));

    mappings.addKeyPress (AppCommandIDs::saveProject, juce::KeyPress ('s', cmd, 0));
    mappings.addKeyPress (AppCommandIDs::saveProject, juce::KeyPress ('s', ctrl, 0));
    mappings.addKeyPress (AppCommandIDs::saveProjectAs, juce::KeyPress ('s', cmd | shift, 0));
    mappings.addKeyPress (AppCommandIDs::saveProjectAs, juce::KeyPress ('s', ctrl | shift, 0));
    mappings.addKeyPress (AppCommandIDs::exportProject, juce::KeyPress ('e', cmd | shift, 0));
    mappings.addKeyPress (AppCommandIDs::exportProject, juce::KeyPress ('e', ctrl | shift, 0));

    mappings.addKeyPress (AppCommandIDs::toggleGrid, juce::KeyPress ('4', ctrl, 0));
    mappings.addKeyPress (AppCommandIDs::toggleGrid, juce::KeyPress ('4', cmd, 0));
    mappings.addKeyPress (AppCommandIDs::duplicateClips, juce::KeyPress ('d', ctrl, 0));
    mappings.addKeyPress (AppCommandIDs::duplicateClips, juce::KeyPress ('d', cmd, 0));
    mappings.addKeyPress (AppCommandIDs::groupClips, juce::KeyPress ('g', ctrl, 0));
    mappings.addKeyPress (AppCommandIDs::groupClips, juce::KeyPress ('g', cmd, 0));
    mappings.addKeyPress (AppCommandIDs::ungroupClips, juce::KeyPress ('g', ctrl | shift, 0));
    mappings.addKeyPress (AppCommandIDs::ungroupClips, juce::KeyPress ('g', cmd | shift, 0));
    mappings.addKeyPress (AppCommandIDs::toggleRipple, juce::KeyPress ('r', ctrl, 0));
    mappings.addKeyPress (AppCommandIDs::toggleRipple, juce::KeyPress ('r', cmd, 0));
    mappings.addKeyPress (AppCommandIDs::deleteTimelineSelection, juce::KeyPress (juce::KeyPress::deleteKey));
    mappings.addKeyPress (AppCommandIDs::deleteTimelineSelection, juce::KeyPress (juce::KeyPress::backspaceKey));
    mappings.addKeyPress (AppCommandIDs::addMarker, juce::KeyPress ('m'));
    mappings.addKeyPress (AppCommandIDs::prevMarker, juce::KeyPress (juce::KeyPress::leftKey, alt, 0));
    mappings.addKeyPress (AppCommandIDs::nextMarker, juce::KeyPress (juce::KeyPress::rightKey, alt, 0));
    mappings.addKeyPress (AppCommandIDs::toggleTakeLanes, juce::KeyPress ('t'));
    mappings.addKeyPress (AppCommandIDs::consolidateClips, juce::KeyPress ('j', ctrl, 0));
    mappings.addKeyPress (AppCommandIDs::consolidateClips, juce::KeyPress ('j', cmd, 0));
    mappings.addKeyPress (AppCommandIDs::applyGrooveToClips, juce::KeyPress ('h', shift, 0));
    mappings.addKeyPress (AppCommandIDs::toggleDetailDevices, juce::KeyPress ('d', alt, 0));
    mappings.addKeyPress (AppCommandIDs::toggleDetailClip, juce::KeyPress ('c', alt, 0));
    mappings.addKeyPress (AppCommandIDs::toggleMainView, juce::KeyPress (juce::KeyPress::tabKey));
    mappings.addKeyPress (AppCommandIDs::captureSessionToArrangement, juce::KeyPress ('c', shift, 0));

    mappings.addKeyPress (AppCommandIDs::pluginCopy, juce::KeyPress ('c', ctrl, 0));
    mappings.addKeyPress (AppCommandIDs::pluginCopy, juce::KeyPress ('c', cmd, 0));
    mappings.addKeyPress (AppCommandIDs::pluginPaste, juce::KeyPress ('v', ctrl, 0));
    mappings.addKeyPress (AppCommandIDs::pluginPaste, juce::KeyPress ('v', cmd, 0));
    mappings.addKeyPress (AppCommandIDs::pluginDuplicate, juce::KeyPress ('d', ctrl, 0));
    mappings.addKeyPress (AppCommandIDs::pluginDuplicate, juce::KeyPress ('d', cmd, 0));
    mappings.addKeyPress (AppCommandIDs::pluginDelete, juce::KeyPress (juce::KeyPress::deleteKey));
    mappings.addKeyPress (AppCommandIDs::pluginDelete, juce::KeyPress (juce::KeyPress::backspaceKey));

    mappings.addKeyPress (AppCommandIDs::pianoDeleteNotes, juce::KeyPress (juce::KeyPress::deleteKey));
    mappings.addKeyPress (AppCommandIDs::pianoDeleteNotes, juce::KeyPress (juce::KeyPress::backspaceKey));
    mappings.addKeyPress (AppCommandIDs::pianoSelectAll, juce::KeyPress ('a', ctrl, 0));
    mappings.addKeyPress (AppCommandIDs::pianoSelectAll, juce::KeyPress ('a', cmd, 0));
    mappings.addKeyPress (AppCommandIDs::pianoDuplicateNotes, juce::KeyPress ('d', ctrl, 0));
    mappings.addKeyPress (AppCommandIDs::pianoDuplicateNotes, juce::KeyPress ('d', cmd, 0));
    mappings.addKeyPress (AppCommandIDs::pianoQuantize, juce::KeyPress ('q'));
    mappings.addKeyPress (AppCommandIDs::pianoHumanize, juce::KeyPress ('h'));
    mappings.addKeyPress (AppCommandIDs::pianoToggleFold, juce::KeyPress ('f'));
    mappings.addKeyPress (AppCommandIDs::pianoToggleScaleSnap, juce::KeyPress ('s'));
    mappings.addKeyPress (AppCommandIDs::pianoToggleDrawTool, juce::KeyPress ('d'));
    mappings.addKeyPress (AppCommandIDs::pianoToggleStepTool, juce::KeyPress ('t'));
    mappings.addKeyPress (AppCommandIDs::pianoEscape, juce::KeyPress (juce::KeyPress::escapeKey));
    mappings.addKeyPress (AppCommandIDs::pianoNudgeLeft, juce::KeyPress (juce::KeyPress::leftKey));
    mappings.addKeyPress (AppCommandIDs::pianoNudgeRight, juce::KeyPress (juce::KeyPress::rightKey));
    mappings.addKeyPress (AppCommandIDs::pianoNudgeUp, juce::KeyPress (juce::KeyPress::upKey));
    mappings.addKeyPress (AppCommandIDs::pianoNudgeDown, juce::KeyPress (juce::KeyPress::downKey));
    mappings.addKeyPress (AppCommandIDs::pianoStepRest, juce::KeyPress (juce::KeyPress::spaceKey));
}

juce::String AppCommands::getCommandName (int commandID)
{
    switch (commandID)
    {
        case AppCommandIDs::play: return "Play/Pause";
        case AppCommandIDs::stop: return "Stop";
        case AppCommandIDs::record: return "Record";
        case AppCommandIDs::undo: return "Undo";
        case AppCommandIDs::redo: return "Redo";
        case AppCommandIDs::saveProject: return "Save Project";
        case AppCommandIDs::saveProjectAs: return "Save Project As";
        case AppCommandIDs::exportProject: return "Export";
        case AppCommandIDs::showPreferences: return "Preferences";
        case AppCommandIDs::toggleMidiLearn: return "MIDI Learn";
        case AppCommandIDs::toggleGrid: return "Toggle Grid";
        case AppCommandIDs::duplicateClips: return "Duplicate Clips";
        case AppCommandIDs::groupClips: return "Group Clips";
        case AppCommandIDs::ungroupClips: return "Ungroup Clips";
        case AppCommandIDs::toggleRipple: return "Toggle Ripple";
        case AppCommandIDs::deleteTimelineSelection: return "Delete Selection";
        case AppCommandIDs::addMarker: return "Add Marker";
        case AppCommandIDs::prevMarker: return "Previous Marker";
        case AppCommandIDs::nextMarker: return "Next Marker";
        case AppCommandIDs::toggleTakeLanes: return "Toggle Take Lanes";
        case AppCommandIDs::consolidateClips: return "Consolidate";
        case AppCommandIDs::applyGrooveToClips: return "Apply Groove";
        case AppCommandIDs::toggleDetailDevices: return "Show Devices";
        case AppCommandIDs::toggleDetailClip: return "Show Clip";
        case AppCommandIDs::toggleMainView: return "Toggle Session View";
        case AppCommandIDs::toggleRecordToArrangement: return "Record to Arrangement";
        case AppCommandIDs::captureSessionToArrangement: return "Capture and Insert";
        case AppCommandIDs::duplicateSessionLoopToArrangement: return "Commit Loop to Arrangement";
        case AppCommandIDs::pluginCopy: return "Copy Plugin";
        case AppCommandIDs::pluginPaste: return "Paste Plugin";
        case AppCommandIDs::pluginDuplicate: return "Duplicate Plugin";
        case AppCommandIDs::pluginDelete: return "Delete Plugin";
        case AppCommandIDs::pianoDeleteNotes: return "Delete Notes";
        case AppCommandIDs::pianoSelectAll: return "Select All Notes";
        case AppCommandIDs::pianoDuplicateNotes: return "Duplicate Notes";
        case AppCommandIDs::pianoQuantize: return "Quantize Notes";
        case AppCommandIDs::pianoHumanize: return "Humanize Notes";
        case AppCommandIDs::pianoToggleFold: return "Toggle Fold";
        case AppCommandIDs::pianoToggleScaleSnap: return "Toggle Scale Snap";
        case AppCommandIDs::pianoToggleDrawTool: return "Toggle Draw Tool";
        case AppCommandIDs::pianoToggleStepTool: return "Toggle Step Tool";
        case AppCommandIDs::pianoEscape: return "Clear Selection";
        case AppCommandIDs::pianoNudgeLeft: return "Nudge Left";
        case AppCommandIDs::pianoNudgeRight: return "Nudge Right";
        case AppCommandIDs::pianoNudgeUp: return "Nudge Up";
        case AppCommandIDs::pianoNudgeDown: return "Nudge Down";
        case AppCommandIDs::pianoStepRest: return "Step Rest";
        default: return {};
    }
}

} // namespace skeletonhive
