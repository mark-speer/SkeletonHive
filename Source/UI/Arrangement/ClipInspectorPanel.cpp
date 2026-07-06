#include "ClipInspectorPanel.h"
#include "Engine/EngineHelpers.h"
#include "UI/AppLookAndFeel.h"

namespace skeletonhive
{

namespace
{
constexpr int numClipColours = 8;

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

ClipInspectorPanel::ClipInspectorPanel (te::Edit& e, te::SelectionManager& sm)
    : edit (e), selectionManager (sm)
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
    speedSlider.onValueChange = [this]
    {
        if (updatingFromModel)
            return;

        const double ratio = speedSlider.getValue() / 100.0;
        applyToAudioClips ([ratio] (te::AudioClipBase& ac)
        {
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
    addAndMakeVisible (takeLabel);
    addAndMakeVisible (takeBox);

    stretchModeBox.clear (juce::dontSendNotification);
    const auto modeNames = te::TimeStretcher::getPossibleModes (edit.engine, true);
    int modeId = 1;

    for (const auto& name : modeNames)
        stretchModeBox.addItem (name, modeId++);

    if (stretchModeBox.getNumItems() == 0)
        stretchModeBox.addItem ("Default", 1);
}

ClipInspectorPanel::~ClipInspectorPanel()
{
    detachClipListener();
}

int ClipInspectorPanel::getPreferredHeight() const
{
    if (! hasAudioSelection())
        return 0;

    if (clips.size() == 1 && EngineHelpers::hasMultipleTakes (*clips.getFirst()))
        return panelHeight + takeRowHeight + 4;

    return panelHeight;
}

void ClipInspectorPanel::setClips (const juce::Array<te::Clip*>& newClips)
{
    clips.clear();
    audioClips.clear();

    for (auto* clip : newClips)
    {
        if (clip == nullptr)
            continue;

        clips.add (clip);

        if (auto* audio = dynamic_cast<te::AudioClipBase*> (clip))
            audioClips.add (audio);
    }

    const auto linked = EngineHelpers::expandWithGroupedPeers (clips);
    if (linked.size() > clips.size())
        countLabel.setText (juce::String (linked.size()) + " clips (linked)", juce::dontSendNotification);
    else
        countLabel.setText (clips.size() > 1 ? juce::String (clips.size()) + " clips" : juce::String(),
                            juce::dontSendNotification);

    attachClipListener (clips.isEmpty() ? nullptr : clips.getFirst());
    refreshFromModel();
    setVisible (hasAudioSelection());
    resized();
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

    if (audioClips.isEmpty())
    {
        updatingFromModel = false;
        return;
    }

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
    if (! hasAudioSelection())
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

    placeRow (gainLabel, gainSlider, area);
    placeRow (transposeLabel, transposeSlider, area);

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

    if (clips.size() == 1 && EngineHelpers::hasMultipleTakes (*clips.getFirst()))
        placeRow (takeLabel, takeBox, area);
}

} // namespace skeletonhive
