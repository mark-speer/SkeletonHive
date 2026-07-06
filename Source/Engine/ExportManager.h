#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

/** Bounces the edit to an audio file via te::Renderer.

    showExportDialog() presents format/sample-rate/bit-depth/range options,
    prompts for a destination file and then renders with a progress dialog
    (see ExtendedUIBehaviour::runTaskWithProgressBar).
*/
struct ExportManager
{
    struct Options
    {
        bool useFlac = false;
        double sampleRate = 44100.0;
        int bitDepth = 24;
        bool useLoopRange = false;
    };

    /** Scoped in-place bounce parameters (consolidate / flatten). */
    struct RenderScope
    {
        te::TimeRange time;
        juce::Array<te::Track*> tracks;
        bool usePlugins = true;
        bool useMasterPlugins = false;
        double sampleRate = 0.0;
        int bitDepth = 24;
    };

    static void showExportDialog (te::Edit& edit, juce::Component* componentToCentreAround);

    /** Renders synchronously behind the modal progress dialog.
        Returns the rendered file, or an invalid file on failure/cancel. */
    static juce::File renderToFile (te::Edit& edit, const juce::File& destFile, const Options& options);

    /** Renders a subset of tracks over a time range (WAV, project sample rate). */
    static juce::File renderScopeToFile (te::Edit& edit, const juce::File& destFile, const RenderScope& scope);
};

} // namespace skeletonhive
