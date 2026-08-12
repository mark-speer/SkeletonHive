#include "NamPlugin.h"
#include "NamCoreBootstrap.h"

#include "get_dsp.h"

#include <cstdio>
#include <cstring>
#include <filesystem>

#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace skeletonhive
{

namespace
{

std::filesystem::path namPathFromJuString (const juce::String& path)
{
   #if JUCE_WINDOWS
    return std::filesystem::path (path.toWideCharPointer());
   #else
    return std::filesystem::u8path (path.toStdString());
   #endif
}

struct LoadResult
{
    std::shared_ptr<nam::DSP> model;
    juce::String status;
    double preparedSampleRate = 0.0;
    int preparedMaxBlock = 0;
};

void setLoadError (char* error, int errorBytes, const char* text)
{
    if (error == nullptr || errorBytes <= 0 || text == nullptr)
        return;

    std::strncpy (error, text, (size_t) errorBytes - 1);
    error[errorBytes - 1] = 0;
}

nam::DSP* loadNamModelCpp (const std::filesystem::path& path, char* error, int errorBytes)
{
    try
    {
        auto model = nam::get_dsp (path);
        if (model == nullptr)
        {
            setLoadError (error, errorBytes, "Failed to load model");
            return nullptr;
        }

        return model.release();
    }
    catch (const std::exception& e)
    {
        setLoadError (error, errorBytes, e.what());
        return nullptr;
    }
    catch (...)
    {
        setLoadError (error, errorBytes, "unknown exception");
        return nullptr;
    }
}

#if JUCE_WINDOWS
nam::DSP* loadNamModelCppFromWide (const wchar_t* pathUtf16, char* error, int errorBytes)
{
    return loadNamModelCpp (std::filesystem::path (pathUtf16), error, errorBytes);
}

// No C++ objects with destructors in this function — required for __try.
nam::DSP* loadNamModelSeh (const wchar_t* pathUtf16, char* error, int errorBytes)
{
    nam::DSP* result = nullptr;

    __try
    {
        result = loadNamModelCppFromWide (pathUtf16, error, errorBytes);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (error != nullptr && errorBytes > 0)
        {
            const unsigned code = (unsigned) GetExceptionCode();
            std::snprintf (error, (size_t) errorBytes, "Native crash while loading model (0x%08X)", code);
        }
        result = nullptr;
    }

    return result;
}

int resetNamModelSeh (nam::DSP* model, double sampleRate, int maxBlock, char* error, int errorBytes)
{
    if (model == nullptr)
        return 0;

    __try
    {
        model->Reset (sampleRate, maxBlock);
        return 1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (error != nullptr && errorBytes > 0)
        {
            const unsigned code = (unsigned) GetExceptionCode();
            std::snprintf (error, (size_t) errorBytes, "Native crash while preparing model (0x%08X)", code);
        }
        return 0;
    }
}

// No C++ objects with destructors — required for __try.
int processNamModelSeh (nam::DSP* model, NAM_SAMPLE** inputs, NAM_SAMPLE** outputs, int numFrames)
{
    if (model == nullptr || inputs == nullptr || outputs == nullptr || numFrames <= 0)
        return 0;

    __try
    {
        model->process (inputs, outputs, numFrames);
        return 1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}
#endif

void destroyNamModelOnMessageThread (std::shared_ptr<nam::DSP> model)
{
    if (model == nullptr)
        return;

    // WaveNet / SlimmableContainer teardown is heavy and must not run on the audio thread.
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr
        && ! juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        juce::MessageManager::callAsync ([victim = std::move (model)]() mutable
        {
            victim.reset();
        });
        return;
    }

    model.reset();
}

bool prepareNamModel (nam::DSP& model, double sampleRate, int maxBlock, char* error, int errorBytes)
{
    if (sampleRate <= 0.0 || maxBlock <= 0)
    {
        setLoadError (error, errorBytes, "Invalid sample rate or block size");
        return false;
    }

    try
    {
       #if JUCE_WINDOWS
        if (resetNamModelSeh (&model, sampleRate, maxBlock, error, errorBytes) == 0)
            return false;
       #else
        model.Reset (sampleRate, maxBlock);
       #endif
        return true;
    }
    catch (const std::exception& e)
    {
        setLoadError (error, errorBytes, e.what());
        return false;
    }
    catch (...)
    {
        setLoadError (error, errorBytes, "unknown exception during prepare");
        return false;
    }
}

LoadResult loadNamModelFromFile (const juce::String& path, double sampleRate, int maxBlock)
{
    LoadResult result;
    char errorBuf[512] {};

   #if JUCE_WINDOWS
    nam::DSP* raw = loadNamModelSeh (path.toWideCharPointer(), errorBuf, (int) sizeof (errorBuf));
   #else
    nam::DSP* raw = loadNamModelCpp (namPathFromJuString (path), errorBuf, (int) sizeof (errorBuf));
   #endif

    if (raw == nullptr)
    {
        const juce::String detail = errorBuf[0] != 0 ? juce::String (errorBuf) : "Failed to load model";
        result.status = detail.startsWithIgnoreCase ("Load error") || detail.startsWithIgnoreCase ("Native")
                            ? detail
                            : ("Load error: " + detail);
        return result;
    }

    result.model.reset (raw);

    // Always prepare offline before the model is published to the audio thread.
    // Never Reset a live model — NAM WaveNet/Container state is not thread-safe with process().
    errorBuf[0] = 0;
    if (! prepareNamModel (*result.model, sampleRate, maxBlock, errorBuf, (int) sizeof (errorBuf)))
    {
        const juce::String detail = errorBuf[0] != 0 ? juce::String (errorBuf) : "Failed to prepare model";
        result.model.reset();
        result.status = detail.startsWithIgnoreCase ("Load error") || detail.startsWithIgnoreCase ("Native")
                            ? detail
                            : ("Load error: " + detail);
        return result;
    }

    result.preparedSampleRate = sampleRate;
    result.preparedMaxBlock = maxBlock;
    result.status = "Loaded " + juce::File (path).getFileName();
    return result;
}

} // namespace

//==============================================================================
class NamPlugin::RetireDrainer : public juce::AsyncUpdater
{
public:
    explicit RetireDrainer (NamPlugin& ownerToUse) : owner (ownerToUse) {}

    void handleAsyncUpdate() override
    {
        owner.drainRetiredModels();
    }

private:
    NamPlugin& owner;
};

//==============================================================================
class NamPlugin::LoadThread : public juce::Thread
{
public:
    LoadThread (NamPlugin& ownerToUse,
                uint64_t generationToUse,
                juce::String pathToUse,
                double sampleRateToUse,
                int maxBlockToUse)
        : juce::Thread ("NAM Loader", 16 * 1024 * 1024),
          owner (ownerToUse),
          generation (generationToUse),
          path (std::move (pathToUse)),
          sampleRate (sampleRateToUse),
          maxBlock (maxBlockToUse)
    {
    }

    void run() override
    {
        te::Plugin::Ptr keepAlive (&owner);

        LoadResult loaded;
        try
        {
            ensureNamParsersRegistered();
            loaded = loadNamModelFromFile (path, sampleRate, maxBlock);
        }
        catch (const std::exception& e)
        {
            loaded = {};
            loaded.status = "Load error: " + juce::String (e.what());
        }
        catch (...)
        {
            loaded = {};
            loaded.status = "Load error: unknown exception";
        }

        juce::MessageManager::callAsync ([keepAlive,
                                          generation = generation,
                                          path = path,
                                          model = std::move (loaded.model),
                                          status = loaded.status,
                                          preparedSampleRate = loaded.preparedSampleRate,
                                          preparedMaxBlock = loaded.preparedMaxBlock]() mutable
        {
            if (auto* nam = dynamic_cast<NamPlugin*> (keepAlive.get()))
            {
                if (generation != nam->loadGeneration.load())
                {
                    if (model != nullptr)
                    {
                        const juce::ScopedLock sl (nam->retireLock);
                        nam->retiredModels.push_back (std::move (model));
                    }
                }
                else
                {
                    nam->installModel (std::move (model), path, status, preparedSampleRate, preparedMaxBlock);
                }

                nam->scheduleRetiredDrain();
                nam->loadThreadFinished();
            }
        });
    }

private:
    NamPlugin& owner;
    uint64_t generation;
    juce::String path;
    double sampleRate;
    int maxBlock;
};

//==============================================================================
const char* NamPlugin::xmlTypeName = "skeletonhiveNam";

NamPlugin::NamPlugin (te::PluginCreationInfo info)
    : te::Plugin (info),
      retireDrainer (std::make_unique<RetireDrainer> (*this))
{
    ensureNamParsersRegistered();

    auto* um = getUndoManager();

    inputValue.referTo (state, "input", um, 0.0f);
    outputValue.referTo (state, "output", um, 0.0f);
    modelPathValue.referTo (state, "modelPath", um, {});
    statusValue.referTo (state, "status", um, "No model loaded");

    inputParam = addParam ("input", TRANS ("Input"), { -24.0f, 24.0f },
                           [] (float value) { return juce::Decibels::toString (value); },
                           [] (const juce::String& s) { return s.getFloatValue(); });
    outputParam = addParam ("output", TRANS ("Output"), { -24.0f, 24.0f },
                            [] (float value) { return juce::Decibels::toString (value); },
                            [] (const juce::String& s) { return s.getFloatValue(); });

    inputParam->attachToCurrentValue (inputValue);
    outputParam->attachToCurrentValue (outputValue);

    if (modelPathValue.get().isNotEmpty())
        reloadModelFromState();
}

NamPlugin::~NamPlugin()
{
    ++loadGeneration;
    reloadQueued = false;

    if (loadThread != nullptr)
    {
        // Never force-kill: TerminateThread during NAM/Eigen work crashes the process.
        loadThread->signalThreadShouldExit();
        loadThread->waitForThreadToExit (180000);
        loadThread.reset();
    }

    if (retireDrainer != nullptr)
        retireDrainer->cancelPendingUpdate();

    notifyListenersOfDeletion();

    if (inputParam != nullptr) inputParam->detachFromCurrentValue();
    if (outputParam != nullptr) outputParam->detachFromCurrentValue();

    std::atomic_store (&activeModel, std::shared_ptr<nam::DSP> {});

    // Plugin teardown runs off the audio callback; force-clear any stragglers.
    {
        const juce::ScopedLock sl (retireLock);
        retiredModels.clear();
    }
}

juce::String NamPlugin::getName() const
{
    return TRANS (getPluginName());
}

juce::String NamPlugin::getPluginType()
{
    return xmlTypeName;
}

juce::String NamPlugin::getSelectableDescription()
{
    return TRANS ("Neural Amp Modeler");
}

int NamPlugin::getNumOutputChannelsGivenInputs (int numInputChannels)
{
    return juce::jmin (numInputChannels, 2);
}

void NamPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    const double newSr = info.sampleRate;
    const int newBlock = juce::jmax (1, info.blockSizeSamples);
    const int newMaxBlock = juce::jmax (newBlock, (int) NAM_DEFAULT_MAX_BUFFER_SIZE);
    const bool needsReprepare = std::atomic_load (&activeModel) != nullptr
                                && (preparedSampleRate != newSr || preparedMaxBlock != newMaxBlock);

    sampleRate = newSr;
    blockSizeSamples = newBlock;
    isPrepared = true;

    monoIn.setSize (1, newMaxBlock);
    monoOut.setSize (1, newMaxBlock);

    // Never Reset a published model (audio may be in process()). If the graph
    // rate/block differs from how the model was prepared, reload offline on the worker.
    if (needsReprepare && modelPathValue.get().isNotEmpty())
        loadModelFile (modelPathValue.get());
}

void NamPlugin::deinitialise()
{
    isPrepared = false;
    monoIn.setSize (0, 0);
    monoOut.setSize (0, 0);
}

void NamPlugin::applyToBuffer (const te::PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    const int numSamples = fc.bufferNumSamples;
    const int numChannels = juce::jmin (fc.destBuffer->getNumChannels(), 2);
    if (numSamples <= 0 || numChannels <= 0)
        return;

    const float inputDb = inputParam != nullptr ? inputParam->getCurrentValue() : inputValue.get();
    const float outputDb = outputParam != nullptr ? outputParam->getCurrentValue() : outputValue.get();
    const float inputGain = juce::Decibels::decibelsToGain (inputDb);
    const float outputGain = juce::Decibels::decibelsToGain (outputDb);

    auto model = std::atomic_load (&activeModel);
    if (model == nullptr
        || numSamples > monoIn.getNumSamples()
        || numSamples > monoOut.getNumSamples()
        || (preparedMaxBlock > 0 && numSamples > preparedMaxBlock))
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = fc.destBuffer->getWritePointer (ch, fc.bufferStartSample);
            juce::FloatVectorOperations::multiply (data, inputGain * outputGain, numSamples);
        }
        return;
    }

    float* monoInPtr = monoIn.getWritePointer (0);
    float* monoOutPtr = monoOut.getWritePointer (0);

    if (numChannels == 1)
    {
        const float* in = fc.destBuffer->getReadPointer (0, fc.bufferStartSample);
        juce::FloatVectorOperations::copyWithMultiply (monoInPtr, in, inputGain, numSamples);
    }
    else
    {
        const float* left = fc.destBuffer->getReadPointer (0, fc.bufferStartSample);
        const float* right = fc.destBuffer->getReadPointer (1, fc.bufferStartSample);
        const float half = 0.5f * inputGain;

        for (int i = 0; i < numSamples; ++i)
            monoInPtr[i] = (left[i] + right[i]) * half;
    }

    NAM_SAMPLE* inPtrs[1] { monoInPtr };
    NAM_SAMPLE* outPtrs[1] { monoOutPtr };

   #if JUCE_WINDOWS
    const bool processed = processNamModelSeh (model.get(), inPtrs, outPtrs, numSamples) != 0;
   #else
    bool processed = true;
    try
    {
        model->process (inPtrs, outPtrs, numSamples);
    }
    catch (...)
    {
        processed = false;
    }
   #endif

    if (! processed)
    {
        // Only unpublish if this callback still owns the live model.
        auto expected = model;
        std::atomic_compare_exchange_strong (&activeModel, &expected, std::shared_ptr<nam::DSP> {});
        destroyNamModelOnMessageThread (std::move (model));

        te::Plugin::Ptr keepAlive (this);
        juce::MessageManager::callAsync ([keepAlive]()
        {
            if (auto* nam = dynamic_cast<NamPlugin*> (keepAlive.get()))
            {
                nam->preparedSampleRate = 0.0;
                nam->preparedMaxBlock = 0;
                nam->statusValue.setValue ("Load error: Native crash while processing model", nullptr);
                nam->scheduleRetiredDrain();
            }
        });

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = fc.destBuffer->getWritePointer (ch, fc.bufferStartSample);
            juce::FloatVectorOperations::multiply (data, inputGain * outputGain, numSamples);
        }
        return;
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* out = fc.destBuffer->getWritePointer (ch, fc.bufferStartSample);
        juce::FloatVectorOperations::copyWithMultiply (out, monoOutPtr, outputGain, numSamples);
    }

    // If installModel already retired this graph and drained the message-thread
    // copy, we may be the last holder — never run ~DSP here.
    if (model.use_count() == 1)
        destroyNamModelOnMessageThread (std::move (model));
}

void NamPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    tracktion::engine::copyPropertiesToCachedValues (v, inputValue, outputValue, modelPathValue, statusValue);

    for (auto* p : getAutomatableParameters())
        if (p != nullptr)
            p->updateFromAttachedValue();

    reloadModelFromState();
}

void NamPlugin::loadModelFile (const juce::String& absolutePath)
{
    const auto path = absolutePath.trim();
    modelPathValue.setValue (path, getUndoManager());

    if (path.isEmpty())
    {
        reloadQueued = false;
        ++loadGeneration;
        installModel (nullptr, {}, "No model loaded", 0.0, 0);
        return;
    }

    const auto file = juce::File (path);
    if (! file.existsAsFile())
    {
        reloadQueued = false;
        ++loadGeneration;
        installModel (nullptr, path, "Model file not found", 0.0, 0);
        return;
    }

    ensureNamParsersRegistered();

    statusValue.setValue ("Loading...", nullptr);
    const auto generation = ++loadGeneration;

    // Large SlimmableContainer models can take well over 10s. Forcibly stopping the
    // loader (JUCE killThread / TerminateThread) during get_dsp/Reset crashes the app.
    if (loadThread != nullptr && loadThread->isThreadRunning())
    {
        reloadQueued = true;
        return;
    }

    reloadQueued = false;
    startLoadThread (generation, path);
}

void NamPlugin::startLoadThread (uint64_t generation, const juce::String& path)
{
    const double sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    const int maxBlock = juce::jmax (blockSizeSamples > 0 ? blockSizeSamples : 512,
                                     (int) NAM_DEFAULT_MAX_BUFFER_SIZE);

    loadThread.reset();
    loadThread = std::make_unique<LoadThread> (*this, generation, path, sr, maxBlock);
    if (! loadThread->startThread (juce::Thread::Priority::background))
    {
        loadThread.reset();
        installModel (nullptr, path, "Failed to start model loader thread", 0.0, 0);
    }
}

void NamPlugin::loadThreadFinished()
{
    loadThread.reset();

    if (! reloadQueued)
        return;

    reloadQueued = false;

    const auto path = modelPathValue.get().trim();
    if (path.isEmpty() || ! juce::File (path).existsAsFile())
    {
        installModel (nullptr, path, path.isEmpty() ? "No model loaded" : "Model file not found", 0.0, 0);
        return;
    }

    statusValue.setValue ("Loading...", nullptr);
    startLoadThread (loadGeneration.load(), path);
}

juce::String NamPlugin::getModelPath() const
{
    return modelPathValue.get();
}

juce::String NamPlugin::getStatusMessage() const
{
    return statusValue.get();
}

bool NamPlugin::isModelLoaded() const
{
    return std::atomic_load (&activeModel) != nullptr;
}

void NamPlugin::installModel (std::shared_ptr<nam::DSP> model, const juce::String& path,
                              const juce::String& status, double modelSampleRate, int modelMaxBlock)
{
    // Publish only — never Reset/prewarm here. The audio thread may already be
    // calling process() on the previous model, and must not share Reset with us.
    auto previous = std::atomic_exchange (&activeModel, std::move (model));
    if (previous != nullptr)
    {
        const juce::ScopedLock sl (retireLock);
        retiredModels.push_back (std::move (previous));
    }

    preparedSampleRate = modelSampleRate;
    preparedMaxBlock = modelMaxBlock;

    if (path.isNotEmpty() && modelPathValue.get() != path)
        modelPathValue.setValue (path, nullptr);

    statusValue.setValue (status, nullptr);
    scheduleRetiredDrain();
}

void NamPlugin::reloadModelFromState()
{
    const auto path = modelPathValue.get();
    if (path.isNotEmpty())
        loadModelFile (path);
    else
        installModel (nullptr, {}, "No model loaded", 0.0, 0);
}

void NamPlugin::scheduleRetiredDrain()
{
    if (retireDrainer != nullptr)
        retireDrainer->triggerAsyncUpdate();
}

void NamPlugin::drainRetiredModels()
{
    std::vector<std::shared_ptr<nam::DSP>> toDestroy;
    bool anyStillHeld = false;

    {
        const juce::ScopedLock sl (retireLock);
        std::vector<std::shared_ptr<nam::DSP>> stillHeld;
        stillHeld.reserve (retiredModels.size());

        for (auto& model : retiredModels)
        {
            if (model == nullptr)
                continue;

            // Audio may still hold a copy from applyToBuffer. Only destroy when
            // this vector owns the last reference — otherwise ~DSP runs later on
            // the audio thread and crashes the process.
            if (model.use_count() <= 1)
                toDestroy.push_back (std::move (model));
            else
                stillHeld.push_back (std::move (model));
        }

        retiredModels.swap (stillHeld);
        anyStillHeld = ! retiredModels.empty();
    }

    toDestroy.clear();

    if (anyStillHeld && retireDrainer != nullptr)
    {
        te::Plugin::Ptr keepAlive (this);
        juce::Timer::callAfterDelay (30, [keepAlive]()
        {
            if (auto* nam = dynamic_cast<NamPlugin*> (keepAlive.get()))
                nam->scheduleRetiredDrain();
        });
    }
}

} // namespace skeletonhive
