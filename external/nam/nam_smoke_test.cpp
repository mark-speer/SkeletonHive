#include "get_dsp.h"

#include <filesystem>
#include <iostream>
#include <string>

extern "C" void nam_ensure_builtin_parsers_registered();
extern "C" int nam_has_builtin_parser (const char* name);

int main (int argc, char** argv)
{
    nam_ensure_builtin_parsers_registered();

    const char* arches[] = { "WaveNet", "LSTM", "ConvNet", "Linear", "SlimmableContainer" };
    bool allOk = true;
    for (auto* a : arches)
    {
        // Query via nam_core so we don't hit a duplicate registry singleton in this TU.
        const bool ok = nam_has_builtin_parser (a) != 0;
        std::cout << a << " registered: " << (ok ? "yes" : "no") << "\n";
        allOk = allOk && ok;
    }

    if (argc < 2)
        return allOk ? 0 : 2;

    try
    {
        const std::filesystem::path path (argv[1]);
        auto model = nam::get_dsp (path);
        if (model == nullptr)
        {
            std::cerr << "get_dsp returned null\n";
            return 3;
        }

        model->Reset (48000.0, 512);
        float in[64] {};
        float out[64] {};
        float* inPtrs[1] { in };
        float* outPtrs[1] { out };
        model->process (inPtrs, outPtrs, 64);
        std::cout << "Loaded and processed OK: " << path.filename().string() << "\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
        return 4;
    }
}
