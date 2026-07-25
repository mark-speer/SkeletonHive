#pragma once

namespace skeletonhive
{

struct PluginHostConstants
{
    static constexpr const char* workerUniqueId = "SkeletonHivePluginHost";
    static constexpr int maxChannels = 2;
    static constexpr int maxBlockSize = 512;
    static constexpr int maxMidiEvents = 32;

    /** How long the *worker* busy-spins on the shared-memory sequence counter before
        falling back to a blocking wait on the cross-process wake event.
    */
    static constexpr int spinBeforeBlockMicroseconds = 250;

    /** Total wall-clock budget for a worker-side wait (spin + blocking) before the
        worker loop polls again. Worker audio is not the host device callback, so a
        few milliseconds of blocking here is acceptable.
    */
    static constexpr int chunkStallTimeoutMicroseconds = 5000;

    /** Host-side realtime budget for one transport chunk round trip. The host audio
        callback MUST NOT block on OS waits — only a short busy-spin is allowed.
        If the worker hasn't answered in this window, that chunk falls back to dry
        passthrough so a slow/starting sandbox can never stall the whole device.
    */
    static constexpr int hostRealtimeRoundTripBudgetMicroseconds = 750;

    /** Number of consecutive soft-stalled chunks that escalates a stall into a hard
        "plugin is hung" decision, triggering watchdog auto-recovery.
    */
    static constexpr int maxConsecutiveStalledChunks = 12;

    /** Independent of the consecutive-stall counter: if this much wall-clock time has
        elapsed since the last successful round trip, escalate to auto-recovery even if
        stalls weren't perfectly consecutive.
    */
    static constexpr int hangRecoveryTimeoutMs = 1500;

    /** Cap on automatic worker kill/restart attempts before giving up and permanently
        marking the slot crashed, to avoid restart-thrashing on a plugin that is
        fundamentally broken rather than transiently slow.
    */
    static constexpr int maxAutoRestartAttempts = 3;
};

} // namespace skeletonhive
