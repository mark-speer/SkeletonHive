#pragma once

#include "EngineHelpers.h"
#include "UI/Plugins/PluginWindow.h"

namespace skeletonhive
{

class ExtendedUIBehaviour : public te::UIBehaviour
{
public:
    std::unique_ptr<juce::Component> createPluginWindow (te::PluginWindowState& pws) override
    {
        if (auto* ws = dynamic_cast<te::Plugin::WindowState*> (&pws))
            return PluginWindow::create (ws->plugin, ws->pluginWindow.get());

        return {};
    }

    void recreatePluginWindowContentAsync (te::Plugin& p) override
    {
        if (auto* w = dynamic_cast<PluginWindow*> (p.windowState->pluginWindow.get()))
            return w->recreateEditorAsync();

        te::UIBehaviour::recreatePluginWindowContentAsync (p);
    }

    // TE calls this for renders (export, track freeze). Runs the job on a
    // background thread behind a modal cancellable progress dialog.
    void runTaskWithProgressBar (te::ThreadPoolJobWithProgress& job) override
    {
        struct TaskRunner : juce::ThreadWithProgressWindow
        {
            explicit TaskRunner (te::ThreadPoolJobWithProgress& j)
                : ThreadWithProgressWindow (j.getJobName(), true, j.canCancel()), task (j) {}

            void run() override
            {
                while (! threadShouldExit())
                {
                    setProgress (task.getCurrentTaskProgress());

                    if (task.runJob() == juce::ThreadPoolJob::jobHasFinished)
                        return;
                }

                task.signalJobShouldExit();
                task.runJob();
            }

            te::ThreadPoolJobWithProgress& task;
        };

        TaskRunner runner (job);
        runner.runThread();
    }

    void showWarningMessage (const juce::String& message) override
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "SkeletonHive", message);
    }
};

} // namespace skeletonhive
