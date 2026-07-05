#pragma once

#include "TracktionCommon.h"

namespace arrange
{

class ExtendedEngineBehaviour : public te::EngineBehaviour
{
public:
    bool canScanPluginsOutOfProcess() override { return true; }
};

} // namespace arrange
