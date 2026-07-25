#include "container.h"
#include "convnet.h"
#include "dsp.h"
#include "lstm.h"
#include "model_config.h"
#include "wavenet/model.h"

// Single definition of ConfigParserRegistry::instance() — must not be inline in
// the header (MSVC can duplicate inline static locals across TUs / static libs).

nam::ConfigParserRegistry& nam::ConfigParserRegistry::instance()
{
    static ConfigParserRegistry inst;
    return inst;
}

extern "C" void nam_ensure_builtin_parsers_registered()
{
    auto& registry = nam::ConfigParserRegistry::instance();

    const auto add = [&registry] (const char* name, nam::ConfigParserFunction parser)
    {
        if (! registry.has (name))
            registry.registerParser (name, std::move (parser));
    };

    add ("WaveNet", nam::wavenet::create_config);
    add ("LSTM", nam::lstm::create_config);
    add ("ConvNet", nam::convnet::create_config);
    add ("Linear", nam::linear::create_config);
    add ("SlimmableContainer", nam::container::create_config);
}

extern "C" int nam_has_builtin_parser (const char* name)
{
    if (name == nullptr || name[0] == 0)
        return 0;

    return nam::ConfigParserRegistry::instance().has (name) ? 1 : 0;
}
