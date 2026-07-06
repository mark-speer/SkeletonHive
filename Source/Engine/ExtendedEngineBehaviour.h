#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

class ExtendedEngineBehaviour : public te::EngineBehaviour
{
public:
    bool canScanPluginsOutOfProcess() override { return true; }
};

} // namespace skeletonhive
