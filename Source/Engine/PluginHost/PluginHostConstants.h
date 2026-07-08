#pragma once

namespace skeletonhive
{

struct PluginHostConstants
{
    static constexpr const char* workerUniqueId = "SkeletonHivePluginHost";
    static constexpr int maxChannels = 2;
    static constexpr int maxBlockSize = 512;
    static constexpr int maxMidiEvents = 32;
    static constexpr int processSpinLimit = 20000;
};

} // namespace skeletonhive
