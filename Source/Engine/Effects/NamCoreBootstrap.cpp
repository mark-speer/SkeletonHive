#include "NamCoreBootstrap.h"

extern "C" void nam_ensure_builtin_parsers_registered();

namespace skeletonhive
{

void ensureNamParsersRegistered()
{
    nam_ensure_builtin_parsers_registered();
}

} // namespace skeletonhive
