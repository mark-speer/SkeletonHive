#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

/** Registers SkeletonHive custom TE plugins with the engine plugin manager. */
void registerNativeCustomPlugins (te::Engine& engine);

bool isSaturationPlugin (const te::Plugin& plugin);
bool isMultibandDynamicsPlugin (const te::Plugin& plugin);
bool isNamPlugin (const te::Plugin& plugin);

} // namespace skeletonhive
