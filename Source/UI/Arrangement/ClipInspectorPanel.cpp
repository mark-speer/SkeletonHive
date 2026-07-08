#include "ClipInspectorPanel.h"
#include "Engine/EngineHelpers.h"
#include "Engine/WarpEngine.h"
#include "UI/AppLookAndFeel.h"

namespace skeletonhive
{

namespace
{
constexpr int numClipColours = 8;

const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

juce::Colour clipPaletteColour (int index)
{
    return AppColours::clipGroupPalette (index % 6);
}

void configureLinearSlider (juce::Slider& slider, const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 52, 20);
    slider.setTextValueSuffix (suffix);
}
} // namespace

ClipInspectorPanel::ClipInspectorPanel (te::Edit& e, te::SelectionManager& sm, EditViewState& evs)
    : edit (e), selectionManager (sm), editViewState (evs), warpEditor (evs)
{
    titleLabel.setText ("Clip", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centredLeft);

    countLabel.setJustificationType (juce::Justification::centredRight);
    countLabel.setFont (juce::FontOptions (11.0f));

    nameEditor.setMultiLine (false);
    nameEditor.setReturnKeyStartsNewLine (false);
    nameEditor.setTooltip ("Clip name");
    nameEditor.onReturnKey = [this]
    {
        if (updatingFromModel)
            return;

        const auto newName = nameEditor.getText();

        for (auto* clip : clipsIncludingLinkedPeers())
            if (clip != nullptr)
                clip->setName (newName);
    };
    nameEditor.onFocusLost = nameEditor.onReturnKey;

    for (int i = 0; i < numClipColours; ++i)
    {
        auto* button = colourButtons.add (new juce::TextButton());
        button->setTooltip ("Clip colour");
        button->setColour (juce::TextButton::buttonColourId, clipPaletteColour (i));
        button->onClick = [this, i]
        {
            if (updatingFromModel)
                return;

            const auto colour = clipPaletteColour (i);

            for (auto* clip : clipsIncludingLinkedPeers())
                if (clip != nullptr)
                    clip->setColour (colour);
        };
    }

    gainLabel.setText ("Gain", juce::dontSendNotification);
    transposeLabel.setText ("Transpose", juce::dontSendNotification);
    speedLabel.setText ("Speed", juce::dontSendNotification);
    stretchLabel.setText ("Stretch", juce::dontSendNotification);
    loopLengthLabel.setText ("Loop len", juce::dontSendNotification);
    warpLabel.setText ("Warp", juce::dontSendNotification);
    warpedLengthLabel.setText ("Warp len", juce::dontSendNotification);
    takeLabel.setText ("Take", juce::dontSendNotification);

    takeBox.setTooltip ("Active take or comp");
    takeBox.onChange = [this]
    {
        if (updatingFromModel || clips.size() != 1)
            return;

        const int takeIndex = takeBox.getSelectedId() - 1;
        if (takeIndex >= 0)
            EngineHelpers::setActiveTake (*clips.getFirst(), takeIndex);
    };

    configureLinearSlider (gainSlider, " dB");
    gainSlider.setRange (-24.0, 12.0, 0.1);
    gainSlider.setSkewFactorFromMidPoint (0.0);
    gainSlider.onValueChange = [this]
    {
        if (updatingFromModel)
            return;

        applyToAudioClips ([v = (float) gainSlider.getValue()] (te::AudioClipBase& ac)
        {
            ac.setGainDB (v);
        });
    };

    configureLinearSlider (transposeSlider, " st");
    transposeSlider.setRange (-24.0, 24.0, 1.0);
    transposeSlider.setNumDecimalPlacesToDisplay (0);
    transposeSlider.onValueChange = [this]
    {
        if (updatingFromModel)
            return;

        applyToAudioClips ([v = (float) transposeSlider.getValue()] (te::AudioClipBase& ac)
        {
            ac.setPitchChange (v);
        });
    };

    configureLinearSlider (speedSlider, "%");
    speedSlider.setRange (25.0, 400.0, 1.0);
    speedSlider.setNumDecimalPlacesToDisplay (0);
    speedSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)); };
    speedSlider.setTooltip ("Clip playback speed (disabled while warp is active)");
    speedSlider.onValueChange = [this]
    {
        if (updatingFromModel)
            return;

        const double ratio = speedSlider.getValue() / 100.0;
        applyToAudioClips ([ratio] (te::AudioClipBase& ac)
        {
            if (ac.getWarpTime())
                return;

            ac.setSpeedRatio (ratio);
        });
    };

    reverseButton.onClick = [this]
    {
        if (updatingFromModel)
            return;

        const bool reversed = reverseButton.getToggleState();
        applyToAudioClips ([reversed] (te::AudioClipBase& ac)
        {
            ac.setIsReversed (reversed);
        });
    };

    stretchModeBox.setTooltip ("Time-stretch algorithm");
    stretchModeBox.onChange = [this]
    {
        if (updatingFromModel || stretchModeBox.getSelectedId() <= 0)
            return;

        const auto modeName = stretchModeBox.getText();
        const auto mode = te::TimeStretcher::getModeFromName (edit.engine, modeName);

        applyToAudioClips ([mode] (te::AudioClipBase& ac)
        {
            ac.setTimeStretchMode (mode);
        });
    };

    warpButton.setTooltip ("Enable segmented warp markers for the clip source");
    warpButton.onClick = [this]
    {
        if (updatingFromModel)
            return;

        const bool enabled = warpButton.getToggleState();
        applyToAudioClips ([enabled, um = &edit.getUndoManager()] (te::AudioClipBase& ac)
        {
            WarpEngine::setWarpEnabled (ac, enabled, um);
        });

        updateControlVisibility();
        notifyLayoutChanged();
    };

    transientButton.setTooltip ("Place warp markers at detected transients");
    transientButton.onClick = [this]
    {
        if (updatingFromModel || audioClips.isEmpty())
            return;

        warpEditor.requestTransientMarkers();
    };

    convertToMidiModeBox.addItem ("Melody", 1);
    convertToMidiModeBox.addItem ("Harmony", 2);
    convertToMidiModeBox.addItem ("Drums", 3);
    convertToMidiModeBox.setSelectedId (1, juce::dontSendNotification);
    convertToMidiModeBox.setTooltip ("Audio-to-MIDI conversion mode");

    convertToMidiButton.setTooltip ("Create a new MIDI clip from this audio clip");
    convertToMidiButton.onClick = [this]
    {
        if (updatingFromModel || clips.size() != 1 || audioClips.isEmpty() || ! onAudioToMidi)
            return;

        const AudioToMidiMode mode = [&]
        {
            switch (convertToMidiModeBox.getSelectedId())
            {
                case 2:  return AudioToMidiMode::harmony;
                case 3:  return AudioToMidiMode::drums;
                default: return AudioToMidiMode::melody;
            }
        }();

        onAudioToMidi (*clips.getFirst(), mode);
    };

    loopButton.onClick = [this]
    {
        if (updatingFromModel)
            return;

        const bool enableLoop = loopButton.getToggleState();
        applyToAudioClips ([enableLoop, beats = loopLengthSlider.getValue()] (te::AudioClipBase& ac)
        {
            if (enableLoop)
            {
                if (auto* wave = dynamic_cast<te::WaveAudioClip*> (&ac))
                    wave->setLoopDefaults();

                const auto length = te::BeatDuration::fromBeats (juce::jmax (0.25, beats));
                ac.setLoopRangeBeats ({ te::BeatPosition(), length });
            }
            else
            {
                ac.disableLooping();
            }
        });

        loopLengthSlider.setEnabled (enableLoop);
    };

    configureLinearSlider (loopLengthSlider, " bt");
    loopLengthSlider.setRange (0.25, 64.0, 0.25);
    loopLengthSlider.onValueChange = [this]
    {
        if (updatingFromModel || ! loopButton.getToggleState())
            return;

        applyToAudioClips ([beats = loopLengthSlider.getValue()] (te::AudioClipBase& ac)
        {
            if (! ac.isLooping())
                return;

            const auto length = te::BeatDuration::fromBeats (juce::jmax (0.25, beats));
            ac.setLoopRangeBeats ({ ac.getLoopStartBeats(), length });
        });
    };

    addAndMakeVisible (titleLabel);
    addAndMakeVisible (countLabel);
    addAndMakeVisible (nameEditor);

    for (auto* button : colourButtons)
        addAndMakeVisible (button);

    for (auto* label : { &gainLabel, &transposeLabel, &speedLabel, &stretchLabel, &loopLengthLabel })
        addAndMakeVisible (*label);

    addAndMakeVisible (gainSlider);
    addAndMakeVisible (transposeSlider);
    addAndMakeVisible (speedSlider);
    addAndMakeVisible (loopLengthSlider);
    addAndMakeVisible (reverseButton);
    addAndMakeVisible (loopButton);
    addAndMakeVisible (stretchModeBox);
    addAndMakeVisible (warpLabel);
    addAndMakeVisible (warpButton);
    addAndMakeVisible (transientButton);
    addAndMakeVisible (convertToMidiModeBox);
    addAndMakeVisible (convertToMidiButton);
    addAndMakeVisible (warpedLengthLabel);
    addAndMakeVisible (warpEditor);
    addAndMakeVisible (takeLabel);
    addAndMakeVisible (takeBox);

    stretchModeBox.clear (juce::dontSendNotification);
    const auto modeNames = te::TimeStretcher::getPossibleModes (edit.engine, true);
    int modeId = 1;

    for (const auto& name : modeNames)
        stretchModeBox.addItem (name, modeId++);

    if (stretchModeBox.getNumItems() == 0)
        stretchModeBox.addItem ("Default", 1);

    scaleRootLabel.setText ("Root", juce::dontSendNotification);
    scaleModeLabel.setText ("Scale", juce::dontSendNotification);
    scaleLockButton.setTooltip ("Constrain playback and editing to the selected scale");

    for (int i = 0; i < 12; ++i)
        scaleRootBox.addItem (noteNames[i], i + 1);

    scaleModeBox.addItem ("No Scale", 1);
    scaleModeBox.addItem ("Major", 2);
    scaleModeBox.addItem ("Minor", 3);

    scaleRootBox.onChange = [this]
    {
        if (updatingFromModel)
            return;

        applyToMidiClips ([root = scaleRootBox.getSelectedId() - 1] (te::MidiClip& mc)
        {
            EngineHelpers::setClipScaleRoot (mc, root);
        });
    };

    scaleModeBox.onChange = [this]
    {
        if (updatingFromModel)
            return;

        applyToMidiClips ([mode = (ScaleMode) juce::jmax (0, scaleModeBox.getSelectedId() - 1)] (te::MidiClip& mc)
        {
            EngineHelpers::setClipScaleMode (mc, mode);
        });
    };

    scaleLockButton.onClick = [this]
    {
        if (updatingFromModel)
            return;

        applyToMidiClips ([locked = scaleLockButton.getToggleState()] (te::MidiClip& mc)
        {
            EngineHelpers::setClipScaleLock (mc, locked);
        });
    };

    addAndMakeVisible (scaleRootLabel);
    addAndMakeVisible (scaleModeLabel);
    addAndMakeVisible (scaleRootBox);
    addAndMakeVisible (scaleModeBox);
    addAndMakeVisible (scaleLockButton);

    updateControlVisibility();
}

ClipInspectorPanel::~ClipInspectorPanel()
{
    detachClipListener();
}

int ClipInspectorPanel::getPreferredHeight() const
{
    if (! hasClipSelection())
        return 0;

    if (hasAudioSelection())
    {
        int height = panelHeight;

        if (clips.size() == 1 && WarpEngine::supportsWarp (*audioClips.getFirst()))
            height += warpRowHeight + 4;

        if (clips.size() == 1 && ! audioClips.isEmpty() && WarpEngine::isWarpEnabled (*audioClips.getFirst()))
        {
            height += warpRowHeight + 4;
            height += warpEditorHeight + 4;
        }

        if (clips.size() == 1 && EngineHelpers::hasMultipleTakes (*clips.getFirst()))
            height += takeRowHeight + 4;

        if (clips.size() == 1 && ! audioClips.isEmpty())
            height += convertRowHeight + 4;

        return height;
    }

    return midiPanelHeight;
}

void ClipInspectorPanel::setClips (const juce::Array<te::Clip*>& newClips)
{
    clips.clear();
    audioClips.clear();
    midiClips.clear();

    for (auto* clip : newClips)
    {
        if (clip == nullptr)
            continue;

        clips.add (clip);

        if (auto* audio = dynamic_cast<te::AudioClipBase*> (clip))
            audioClips.add (audio);
        else if (auto* midi = dynamic_cast<te::MidiClip*> (clip))
            midiClips.add (midi);
    }

    const auto linked = EngineHelpers::expandWithGroupedPeers (clips);
    if (linked.size() > clips.size())
        countLabel.setText (juce::String (linked.size()) + " clips (linked)", juce::dontSendNotification);
    else
        countLabel.setText (clips.size() > 1 ? juce::String (clips.size()) + " clips" : juce::String(),
                            juce::dontSendNotification);

    attachClipListener (clips.isEmpty() ? nullptr : clips.getFirst());
    refreshFromModel();
    updateControlVisibility();
    setVisible (hasClipSelection());

    if (clips.size() == 1 && ! audioClips.isEmpty())
        warpEditor.setClip (audioClips.getFirst());
    else
        warpEditor.setClip (nullptr);

    notifyLayoutChanged();
}

void ClipInspectorPanel::openWarpEditor (te::AudioClipBase& clip)
{
    if (! WarpEngine::supportsWarp (clip))
        return;

    juce::Array<te::Clip*> selected;
    selected.add (&clip);
    setClips (selected);

    if (! WarpEngine::isWarpEnabled (clip))
    {
        WarpEngine::setWarpEnabled (clip, true, &edit.getUndoManager());
        updateControlVisibility();
    }

    focusWarpEditor();
    notifyLayoutChanged();
}

void ClipInspectorPanel::focusWarpEditor()
{
    if (warpEditor.isVisible())
        warpEditor.grabEditorFocus();
}

void ClipInspectorPanel::notifyLayoutChanged()
{
    resized();

    if (auto* parent = getParentComponent())
        parent->resized();
}

void ClipInspectorPanel::attachClipListener (te::Clip* clip)
{
    if (listenedClip.get() == clip)
        return;

    detachClipListener();

    if (clip != nullptr)
    {
        listenedClip = clip;
        listenedClip->state.addListener (this);
    }
}

void ClipInspectorPanel::detachClipListener()
{
    if (listenedClip != nullptr)
    {
        listenedClip->state.removeListener (this);
        listenedClip = nullptr;
    }
}

void ClipInspectorPanel::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    refreshFromModel();
    updateControlVisibility();
    notifyLayoutChanged();
}

void ClipInspectorPanel::refreshFromModel()
{
    updatingFromModel = true;

    if (clips.isEmpty())
    {
        nameEditor.setText ({}, juce::dontSendNotification);
        updatingFromModel = false;
        return;
    }

    const auto& primary = *clips.getFirst();
    nameEditor.setText (primary.getName(), juce::dontSendNotification);

    if (audioClips.isEmpty() && midiClips.isEmpty())
    {
        updatingFromModel = false;
        return;
    }

    if (! audioClips.isEmpty())
    {
        const auto& audio = *audioClips.getFirst();

        gainSlider.setValue (audio.getGainDB(), juce::dontSendNotification);
        transposeSlider.setValue (audio.getPitchChange(), juce::dontSendNotification);
        speedSlider.setValue (audio.getSpeedRatio() * 100.0, juce::dontSendNotification);
        reverseButton.setToggleState (audio.getIsReversed(), juce::dontSendNotification);

        const auto modeName = te::TimeStretcher::getNameOfMode (audio.getActualTimeStretchMode());

        for (int i = 0; i < stretchModeBox.getNumItems(); ++i)
        {
            if (stretchModeBox.getItemText (i) == modeName)
            {
                stretchModeBox.setSelectedItemIndex (i, juce::dontSendNotification);
                break;
            }
        }

        const bool looping = audio.isLooping();
        loopButton.setToggleState (looping, juce::dontSendNotification);
        loopLengthSlider.setEnabled (looping);

        if (looping)
            loopLengthSlider.setValue (audio.getLoopLengthBeats().inBeats(), juce::dontSendNotification);

        const bool supportsWarp = WarpEngine::supportsWarp (audio);
        const bool warpEnabled = audio.getWarpTime();
        warpButton.setToggleState (warpEnabled, juce::dontSendNotification);
        warpButton.setEnabled (supportsWarp);
        transientButton.setEnabled (supportsWarp && warpEnabled);
        speedSlider.setEnabled (! warpEnabled);
        warpedLengthLabel.setVisible (supportsWarp && warpEnabled && clips.size() == 1);

        if (warpEnabled)
            warpedLengthLabel.setText ("Warp len: " + juce::String (WarpEngine::getWarpedLengthSeconds (audio), 2) + " s",
                                       juce::dontSendNotification);
        else
            warpedLengthLabel.setText ("Warp len", juce::dontSendNotification);
    }

    if (! midiClips.isEmpty())
    {
        const auto& midi = *midiClips.getFirst();
        scaleRootBox.setSelectedId (EngineHelpers::getClipScaleRoot (midi) + 1, juce::dontSendNotification);

        switch (EngineHelpers::getClipScaleMode (midi))
        {
            case ScaleMode::major: scaleModeBox.setSelectedId (2, juce::dontSendNotification); break;
            case ScaleMode::minor: scaleModeBox.setSelectedId (3, juce::dontSendNotification); break;
            default:               scaleModeBox.setSelectedId (1, juce::dontSendNotification); break;
        }

        scaleLockButton.setToggleState (EngineHelpers::getClipScaleLock (midi), juce::dontSendNotification);
    }

    const bool showTakes = clips.size() == 1 && EngineHelpers::hasMultipleTakes (primary);
    takeLabel.setVisible (showTakes);
    takeBox.setVisible (showTakes);

    if (showTakes)
    {
        takeBox.clear (juce::dontSendNotification);
        const auto descriptions = EngineHelpers::getTakeDescriptions (primary);
        int id = 1;

        for (const auto& desc : descriptions)
            takeBox.addItem (desc, id++);

        takeBox.setSelectedId (primary.getCurrentTake() + 1, juce::dontSendNotification);
    }

    updatingFromModel = false;
}

void ClipInspectorPanel::applyToAudioClips (std::function<void (te::AudioClipBase&)> fn)
{
    if (fn == nullptr)
        return;

    for (auto* clip : clipsIncludingLinkedPeers())
    {
        if (auto* audio = dynamic_cast<te::AudioClipBase*> (clip))
            fn (*audio);
    }
}

void ClipInspectorPanel::applyToMidiClips (std::function<void (te::MidiClip&)> fn)
{
    if (fn == nullptr)
        return;

    for (auto* clip : clipsIncludingLinkedPeers())
    {
        if (auto* midi = dynamic_cast<te::MidiClip*> (clip))
            fn (*midi);
    }
}

void ClipInspectorPanel::updateControlVisibility()
{
    const bool showAudio = hasAudioSelection();
    const bool showMidi = hasMidiSelection() && ! showAudio;
    const bool showWarpRow = showAudio && clips.size() == 1 && ! audioClips.isEmpty()
                             && WarpEngine::supportsWarp (*audioClips.getFirst());
    const bool showWarpEditor = showWarpRow && WarpEngine::isWarpEnabled (*audioClips.getFirst());
    const bool showWarpedLength = showWarpEditor;

    for (auto* label : { &gainLabel, &transposeLabel, &speedLabel, &stretchLabel, &loopLengthLabel })
        label->setVisible (showAudio);

    gainSlider.setVisible (showAudio);
    transposeSlider.setVisible (showAudio);
    speedSlider.setVisible (showAudio);
    loopLengthSlider.setVisible (showAudio);
    reverseButton.setVisible (showAudio);
    loopButton.setVisible (showAudio);
    stretchModeBox.setVisible (showAudio);
    warpLabel.setVisible (showWarpRow);
    warpButton.setVisible (showWarpRow);
    transientButton.setVisible (showWarpRow);
    const bool showConvert = showAudio && clips.size() == 1 && ! audioClips.isEmpty();
    convertToMidiModeBox.setVisible (showConvert);
    convertToMidiButton.setVisible (showConvert);
    warpedLengthLabel.setVisible (showWarpedLength);
    warpEditor.setVisible (showWarpEditor);

    if (showWarpEditor && ! audioClips.isEmpty())
        warpEditor.setClip (audioClips.getFirst());
    else if (! showWarpEditor)
        warpEditor.setClip (nullptr);

    takeLabel.setVisible (showAudio && clips.size() == 1 && ! clips.isEmpty()
                          && EngineHelpers::hasMultipleTakes (*clips.getFirst()));
    takeBox.setVisible (takeLabel.isVisible());

    scaleRootLabel.setVisible (showMidi);
    scaleModeLabel.setVisible (showMidi);
    scaleRootBox.setVisible (showMidi);
    scaleModeBox.setVisible (showMidi);
    scaleLockButton.setVisible (showMidi);
}

juce::Array<te::Clip*> ClipInspectorPanel::clipsIncludingLinkedPeers() const
{
    return EngineHelpers::expandWithGroupedPeers (clips);
}

void ClipInspectorPanel::paint (juce::Graphics& g)
{
    g.fillAll (AppColours::automationPanelBackground (AppLookAndFeel::getCurrentTheme()).darker (0.08f));
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());
}

void ClipInspectorPanel::resized()
{
    if (! hasClipSelection())
        return;

    auto area = getLocalBounds().reduced (6, 4);
    auto header = area.removeFromTop (headerHeight);

    titleLabel.setBounds (header.removeFromLeft (36));
    countLabel.setBounds (header.removeFromRight (72));
    nameEditor.setBounds (header.removeFromLeft (juce::jmax (80, header.getWidth() / 2)).reduced (0, 2));

    const int colourWidth = 18;
    const int colourGap = 2;
    auto colourArea = header.removeFromRight (numClipColours * (colourWidth + colourGap));

    for (int i = 0; i < colourButtons.size(); ++i)
        colourButtons[i]->setBounds (colourArea.removeFromLeft (colourWidth).reduced (0, 2));

    area.removeFromTop (4);
    const int rowHeight = 24;
    const int labelWidth = 64;
    const int gap = 4;

    auto placeRow = [&] (juce::Label& label, juce::Component& control, juce::Rectangle<int>& rowArea)
    {
        auto row = rowArea.removeFromTop (rowHeight);
        label.setBounds (row.removeFromLeft (labelWidth));
        row.removeFromLeft (gap);
        control.setBounds (row);
        rowArea.removeFromTop (2);
    };

    if (hasAudioSelection())
    {
        placeRow (gainLabel, gainSlider, area);

        auto row2 = area.removeFromTop (rowHeight);
        transposeLabel.setBounds (row2.removeFromLeft (labelWidth));
        row2.removeFromLeft (gap);
        transposeSlider.setBounds (row2);
        area.removeFromTop (2);

        auto row3 = area.removeFromTop (rowHeight);
        speedLabel.setBounds (row3.removeFromLeft (labelWidth));
        row3.removeFromLeft (gap);
        reverseButton.setBounds (row3.removeFromRight (72).reduced (0, 1));
        row3.removeFromRight (gap);
        speedSlider.setBounds (row3);
        area.removeFromTop (2);

        auto row4 = area.removeFromTop (rowHeight);
        stretchLabel.setBounds (row4.removeFromLeft (labelWidth));
        row4.removeFromLeft (gap);
        loopButton.setBounds (row4.removeFromRight (56).reduced (0, 1));
        row4.removeFromRight (gap);
        stretchModeBox.setBounds (row4);
        area.removeFromTop (2);

        placeRow (loopLengthLabel, loopLengthSlider, area);

        if (clips.size() == 1 && ! audioClips.isEmpty() && WarpEngine::supportsWarp (*audioClips.getFirst()))
        {
            auto warpRow = area.removeFromTop (rowHeight);
            warpLabel.setBounds (warpRow.removeFromLeft (labelWidth));
            warpRow.removeFromLeft (gap);
            transientButton.setBounds (warpRow.removeFromRight (84).reduced (0, 1));
            warpRow.removeFromRight (gap);
            warpButton.setBounds (warpRow.removeFromRight (56).reduced (0, 1));
            area.removeFromTop (2);

            if (WarpEngine::isWarpEnabled (*audioClips.getFirst()))
            {
                auto warpedRow = area.removeFromTop (rowHeight);
                warpedLengthLabel.setBounds (warpedRow.reduced (labelWidth + gap, 0));
                area.removeFromTop (2);

                warpEditor.setBounds (area.removeFromTop (warpEditorHeight));
                area.removeFromTop (2);
            }
        }

        if (clips.size() == 1 && EngineHelpers::hasMultipleTakes (*clips.getFirst()))
            placeRow (takeLabel, takeBox, area);

        if (clips.size() == 1 && ! audioClips.isEmpty())
        {
            auto convertRow = area.removeFromTop (rowHeight);
            convertToMidiButton.setBounds (convertRow.removeFromRight (120).reduced (0, 1));
            convertRow.removeFromRight (gap);
            convertToMidiModeBox.setBounds (convertRow);
        }
    }
    else if (hasMidiSelection())
    {
        placeRow (scaleRootLabel, scaleRootBox, area);

        auto row = area.removeFromTop (rowHeight);
        scaleModeLabel.setBounds (row.removeFromLeft (labelWidth));
        row.removeFromLeft (gap);
        scaleLockButton.setBounds (row.removeFromRight (96).reduced (0, 1));
        row.removeFromRight (gap);
        scaleModeBox.setBounds (row);
    }
}

} // namespace skeletonhive
