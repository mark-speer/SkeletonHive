#include "NativeCustomPlugins.h"

#include "SaturationPlugin.h"
#include "MultibandDynamicsPlugin.h"

namespace skeletonhive
{

void registerNativeCustomPlugins (te::Engine& engine)
{
    engine.getPluginManager().createBuiltInType<SaturationPlugin>();
    engine.getPluginManager().createBuiltInType<MultibandDynamicsPlugin>();
}

bool isSaturationPlugin (const te::Plugin& plugin)
{
    return dynamic_cast<const SaturationPlugin*> (&plugin) != nullptr;
}

bool isMultibandDynamicsPlugin (const te::Plugin& plugin)
{
    return dynamic_cast<const MultibandDynamicsPlugin*> (&plugin) != nullptr;
}

} // namespace skeletonhive
