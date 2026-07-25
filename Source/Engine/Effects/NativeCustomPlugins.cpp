#include "NativeCustomPlugins.h"

#include "SaturationPlugin.h"
#include "MultibandDynamicsPlugin.h"
#include "NamPlugin.h"

namespace skeletonhive
{

void registerNativeCustomPlugins (te::Engine& engine)
{
    engine.getPluginManager().createBuiltInType<SaturationPlugin>();
    engine.getPluginManager().createBuiltInType<MultibandDynamicsPlugin>();
    engine.getPluginManager().createBuiltInType<NamPlugin>();
}

bool isSaturationPlugin (const te::Plugin& plugin)
{
    return dynamic_cast<const SaturationPlugin*> (&plugin) != nullptr;
}

bool isMultibandDynamicsPlugin (const te::Plugin& plugin)
{
    return dynamic_cast<const MultibandDynamicsPlugin*> (&plugin) != nullptr;
}

bool isNamPlugin (const te::Plugin& plugin)
{
    return dynamic_cast<const NamPlugin*> (&plugin) != nullptr;
}

} // namespace skeletonhive
