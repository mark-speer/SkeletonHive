#pragma once

#include "Engine/AppSettings.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

struct PluginHostManager
{
    static void installCreatePluginInstanceHook (te::Engine& engine, AppSettings& settings);
};

} // namespace skeletonhive
