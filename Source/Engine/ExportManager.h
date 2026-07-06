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

    static void showExportDialog (te::Edit& edit, juce::Component* componentToCentreAround);

    /** Renders synchronously behind the modal progress dialog.
        Returns the rendered file, or an invalid file on failure/cancel. */
    static juce::File renderToFile (te::Edit& edit, const juce::File& destFile, const Options& options);
};

} // namespace skeletonhive
