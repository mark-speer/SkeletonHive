#include "ExportManager.h"

namespace skeletonhive
{

namespace
{

class ExportOptionsPanel : public juce::Component
{
public:
    ExportOptionsPanel (te::Edit& e) : edit (e)
    {
        formatBox.addItem ("WAV", 1);
        formatBox.addItem ("FLAC", 2);
        formatBox.setSelectedId (1, juce::dontSendNotification);
        formatBox.onChange = [this] { updateBitDepthChoices(); };

        sampleRateBox.addItem ("44100 Hz", 1);
        sampleRateBox.addItem ("48000 Hz", 2);
        sampleRateBox.addItem ("88200 Hz", 3);
        sampleRateBox.addItem ("96000 Hz", 4);
        sampleRateBox.setSelectedId (1, juce::dontSendNotification);

        updateBitDepthChoices();

        rangeBox.addItem ("Entire project", 1);
        rangeBox.addItem ("Loop selection", 2);
        rangeBox.setSelectedId (1, juce::dontSendNotification);

        exportButton.onClick = [this] { chooseFileAndRender(); };
        cancelButton.onClick = [this] { close (0); };

        for (auto* label : { &formatLabel, &sampleRateLabel, &bitDepthLabel, &rangeLabel })
            label->setJustificationType (juce::Justification::centredLeft);

        addAndMakeVisible (formatLabel);
        addAndMakeVisible (formatBox);
        addAndMakeVisible (sampleRateLabel);
        addAndMakeVisible (sampleRateBox);
        addAndMakeVisible (bitDepthLabel);
        addAndMakeVisible (bitDepthBox);
        addAndMakeVisible (rangeLabel);
        addAndMakeVisible (rangeBox);
        addAndMakeVisible (exportButton);
        addAndMakeVisible (cancelButton);

        setSize (320, 190);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        const int rowH = 28;

        auto layoutRow = [&r, rowH] (juce::Label& label, juce::Component& value)
        {
            auto row = r.removeFromTop (rowH).reduced (0, 2);
            label.setBounds (row.removeFromLeft (100));
            value.setBounds (row);
        };

        layoutRow (formatLabel, formatBox);
        layoutRow (sampleRateLabel, sampleRateBox);
        layoutRow (bitDepthLabel, bitDepthBox);
        layoutRow (rangeLabel, rangeBox);

        auto buttons = r.removeFromBottom (rowH);
        cancelButton.setBounds (buttons.removeFromRight (80).reduced (2));
        exportButton.setBounds (buttons.removeFromRight (80).reduced (2));
    }

private:
    void updateBitDepthChoices()
    {
        const bool flac = formatBox.getSelectedId() == 2;
        const int previous = bitDepthBox.getSelectedId();

        bitDepthBox.clear (juce::dontSendNotification);
        bitDepthBox.addItem ("16-bit", 16);
        bitDepthBox.addItem ("24-bit", 24);
        if (! flac)
            bitDepthBox.addItem ("32-bit", 32);

        bitDepthBox.setSelectedId ((previous == 32 && flac) ? 24 : (previous > 0 ? previous : 24),
                                   juce::dontSendNotification);
    }

    void close (int result)
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (result);
    }

    void chooseFileAndRender()
    {
        ExportManager::Options options;
        options.useFlac = formatBox.getSelectedId() == 2;
        options.sampleRate = formatSampleRate();
        options.bitDepth = bitDepthBox.getSelectedId();
        options.useLoopRange = rangeBox.getSelectedId() == 2;

        if (options.useLoopRange && edit.getTransport().getLoopRange().getLength() <= 0s)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "Export",
                                                    "The loop selection is empty. Set a loop range first.");
            return;
        }

        if (! options.useLoopRange && edit.getLength() <= 0s)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "Export",
                                                    "The project is empty; there is nothing to export.");
            return;
        }

        const auto extension = options.useFlac ? ".flac" : ".wav";
        auto defaultFile = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                               .getChildFile (edit.getName().isNotEmpty() ? edit.getName() : "Export")
                               .withFileExtension (extension);

        auto& editRef = edit;
        auto fc = std::make_shared<juce::FileChooser> ("Export Audio", defaultFile, juce::String ("*") + extension);
        fc->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::warnAboutOverwriting,
                         [&editRef, fc, options, extension] (const juce::FileChooser&)
                         {
                             auto file = fc->getResult();
                             if (file == juce::File())
                                 return;

                             if (! file.hasFileExtension (extension))
                                 file = file.withFileExtension (extension);

                             const auto rendered = ExportManager::renderToFile (editRef, file, options);

                             if (rendered.existsAsFile())
                                 juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon, "Export",
                                                                         "Exported to:\n" + rendered.getFullPathName());
                         });

        close (1);
    }

    double formatSampleRate() const
    {
        switch (sampleRateBox.getSelectedId())
        {
            case 2:  return 48000.0;
            case 3:  return 88200.0;
            case 4:  return 96000.0;
            default: return 44100.0;
        }
    }

    te::Edit& edit;

    juce::Label formatLabel { {}, "Format" }, sampleRateLabel { {}, "Sample rate" },
        bitDepthLabel { {}, "Bit depth" }, rangeLabel { {}, "Range" };
    juce::ComboBox formatBox, sampleRateBox, bitDepthBox, rangeBox;
    juce::TextButton exportButton { "Export" }, cancelButton { "Cancel" };
};

} // namespace

void ExportManager::showExportDialog (te::Edit& edit, juce::Component* componentToCentreAround)
{
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Export Audio";
    opts.content.setOwned (new ExportOptionsPanel (edit));
    opts.componentToCentreAround = componentToCentreAround;
    opts.useNativeTitleBar = true;
    opts.resizable = false;
    opts.launchAsync();
}

juce::File ExportManager::renderToFile (te::Edit& edit, const juce::File& destFile, const Options& options)
{
    auto& formatManager = edit.engine.getAudioFileFormatManager();

    te::Renderer::Parameters params (edit);
    params.destFile = destFile;
    params.audioFormat = options.useFlac ? formatManager.getFlacFormat()
                                         : formatManager.getWavFormat();
    params.bitDepth = options.bitDepth;
    params.sampleRateForAudio = options.sampleRate;
    params.blockSizeForAudio = edit.engine.getDeviceManager().getBlockSize();
    params.time = options.useLoopRange ? edit.getTransport().getLoopRange()
                                       : te::TimeRange (0s, edit.getLength());
    params.tracksToDo = te::toBitSet (te::getAllTracks (edit));
    params.usePlugins = true;
    params.useMasterPlugins = true;
    params.ditheringEnabled = options.bitDepth < 32;

    return te::Renderer::renderToFile ("Exporting " + destFile.getFileName(), params);
}

} // namespace skeletonhive
