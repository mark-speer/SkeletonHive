#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

/** Stable command IDs for ApplicationCommandManager. */
namespace AppCommandIDs
{
enum
{
    play = 0x5000,
    stop,
    record,
    undo,
    redo,
    saveProject,
    saveProjectAs,
    exportProject,
    showPreferences,
    toggleMidiLearn,

    toggleGrid,
    duplicateClips,
    groupClips,
    ungroupClips,
    toggleRipple,
    deleteTimelineSelection,
    addMarker,
    prevMarker,
    nextMarker,
    toggleTakeLanes,

    consolidateClips,

    pluginCopy,
    pluginPaste,
    pluginDuplicate,
    pluginDelete,

    pianoDeleteNotes,
    pianoSelectAll,
    pianoDuplicateNotes,
    pianoQuantize,
    pianoHumanize,
    pianoToggleFold,
    pianoToggleScaleSnap,
    pianoToggleDrawTool,
    pianoToggleStepTool,
    pianoEscape,
    pianoNudgeLeft,
    pianoNudgeRight,
    pianoNudgeUp,
    pianoNudgeDown,
    pianoStepRest,

    applyGrooveToClips,
    toggleDetailDevices,
    toggleDetailClip,
    toggleMainView,
    toggleRecordToArrangement,
    captureSessionToArrangement,
    duplicateSessionLoopToArrangement,
    togglePerformancePanel,
};
} // namespace AppCommandIDs

/** Registers command metadata and default key mappings. */
class AppCommands
{
public:
    static void registerAllCommands (juce::ApplicationCommandManager& manager);
    static void registerDefaultKeyMappings (juce::KeyPressMappingSet& mappings);
    static juce::String getCommandName (int commandID);
};

} // namespace skeletonhive
