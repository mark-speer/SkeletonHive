#include "SynthEditor.h"

#include "Engine/SynthHelpers.h"
#include "UI/AppLookAndFeel.h"

namespace skeletonhive
{

namespace
{

constexpr int rowHeight = 44;
constexpr int compactRowHeight = 24;
constexpr int panelWidth = 520;

void configureAutomatableSlider (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 18);
    slider.setRange (0.0, 1.0, 0.001);
}

class AutomatableSliderRow : public juce::Component,
                             private te::AutomatableParameter::Listener
{
public:
    AutomatableSliderRow (te::AutomatableParameter& param, std::function<bool()> isUpdating)
        : parameter (&param), updatingCheck (std::move (isUpdating))
    {
        label.setText (param.getParameterName(), juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (label);

        configureAutomatableSlider (slider);
        slider.setValue (param.getCurrentNormalisedValue(), juce::dontSendNotification);
        slider.textFromValueFunction = [this] (double value)
        {
            if (parameter != nullptr)
                return parameter->valueToString (parameter->valueRange.convertFrom0to1 ((float) value));

            return juce::String {};
        };
        slider.onValueChange = [this]
        {
            if (parameter == nullptr || (updatingCheck != nullptr && updatingCheck()))
                return;

            parameter->setNormalisedParameter ((float) slider.getValue(), juce::sendNotification);
        };
        addAndMakeVisible (slider);

        parameter->addListener (this);
    }

    ~AutomatableSliderRow() override
    {
        if (parameter != nullptr)
            parameter->removeListener (this);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (2);
        label.setBounds (r.removeFromTop (18));
        slider.setBounds (r);
    }

    static int preferredHeight() { return rowHeight; }

private:
    void curveHasChanged (te::AutomatableParameter&) override {}
    void currentValueChanged (te::AutomatableParameter&) override
    {
        if (parameter != nullptr)
            slider.setValue (parameter->getCurrentNormalisedValue(), juce::dontSendNotification);
    }

    te::AutomatableParameter::Ptr parameter;
    std::function<bool()> updatingCheck;
    juce::Label label;
    juce::Slider slider;
};

class CachedIntSliderRow : public juce::Component
{
public:
    CachedIntSliderRow (const juce::String& name, juce::CachedValue<int>& value,
                        std::function<bool()> isUpdating, juce::Range<int> range)
        : cachedValue (value), updatingCheck (std::move (isUpdating))
    {
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (label);

        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 18);
        slider.setRange (range.getStart(), range.getEnd(), 1.0);
        slider.setNumDecimalPlacesToDisplay (0);
        slider.setValue (value.get(), juce::dontSendNotification);
        slider.onValueChange = [this]
        {
            if (updatingCheck != nullptr && updatingCheck())
                return;

            cachedValue = (int) slider.getValue();
        };
        addAndMakeVisible (slider);
    }

    void refresh()
    {
        slider.setValue (cachedValue.get(), juce::dontSendNotification);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (2);
        label.setBounds (r.removeFromTop (18));
        slider.setBounds (r);
    }

    static int preferredHeight() { return rowHeight; }

private:
    juce::CachedValue<int>& cachedValue;
    std::function<bool()> updatingCheck;
    juce::Label label;
    juce::Slider slider;
};

class CachedFloatSliderRow : public juce::Component
{
public:
    CachedFloatSliderRow (const juce::String& name, juce::CachedValue<float>& value,
                          std::function<bool()> isUpdating, juce::NormalisableRange<float> range,
                          const juce::String& suffix = {})
        : cachedValue (value), updatingCheck (std::move (isUpdating))
    {
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (label);

        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 18);
        slider.setRange (range.start, range.end, range.interval);
        slider.setTextValueSuffix (suffix);
        slider.setValue (value.get(), juce::dontSendNotification);
        slider.onValueChange = [this]
        {
            if (updatingCheck != nullptr && updatingCheck())
                return;

            cachedValue = (float) slider.getValue();
        };
        addAndMakeVisible (slider);
    }

    void refresh()
    {
        slider.setValue (cachedValue.get(), juce::dontSendNotification);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (2);
        label.setBounds (r.removeFromTop (18));
        slider.setBounds (r);
    }

    static int preferredHeight() { return rowHeight; }

private:
    juce::CachedValue<float>& cachedValue;
    std::function<bool()> updatingCheck;
    juce::Label label;
    juce::Slider slider;
};

class IntComboRow : public juce::Component,
                    private juce::ComboBox::Listener
{
public:
    IntComboRow (const juce::String& name, juce::CachedValue<int>& value,
                 const juce::StringArray& options, std::function<bool()> isUpdating)
        : cachedValue (value), updatingCheck (std::move (isUpdating))
    {
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (label);

        for (int i = 0; i < options.size(); ++i)
            combo.addItem (options[i], i + 1);

        combo.setSelectedId (value.get() + 1, juce::dontSendNotification);
        combo.addListener (this);
        addAndMakeVisible (combo);
    }

    void refresh()
    {
        combo.setSelectedId (cachedValue.get() + 1, juce::dontSendNotification);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (2);
        label.setBounds (r.removeFromLeft (110));
        combo.setBounds (r);
    }

    static int preferredHeight() { return compactRowHeight; }

private:
    void comboBoxChanged (juce::ComboBox*) override
    {
        if (updatingCheck != nullptr && updatingCheck())
            return;

        cachedValue = combo.getSelectedId() - 1;
    }

    juce::CachedValue<int>& cachedValue;
    std::function<bool()> updatingCheck;
    juce::Label label;
    juce::ComboBox combo;
};

class SlopeComboRow : public juce::Component,
                      private juce::ComboBox::Listener
{
public:
    SlopeComboRow (juce::CachedValue<int>& value, std::function<bool()> isUpdating)
        : cachedValue (value), updatingCheck (std::move (isUpdating))
    {
        label.setText ("Filter Slope", juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (label);

        combo.addItem ("12 dB", 1);
        combo.addItem ("24 dB", 2);
        combo.addListener (this);
        addAndMakeVisible (combo);
        refresh();
    }

    void refresh()
    {
        combo.setSelectedId (cachedValue.get() == 24 ? 2 : 1, juce::dontSendNotification);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (2);
        label.setBounds (r.removeFromLeft (110));
        combo.setBounds (r);
    }

    static int preferredHeight() { return compactRowHeight; }

private:
    void comboBoxChanged (juce::ComboBox*) override
    {
        if (updatingCheck != nullptr && updatingCheck())
            return;

        cachedValue = combo.getSelectedId() == 2 ? 24 : 12;
    }

    juce::CachedValue<int>& cachedValue;
    std::function<bool()> updatingCheck;
    juce::Label label;
    juce::ComboBox combo;
};

class BoolToggleRow : public juce::Component
{
public:
    BoolToggleRow (const juce::String& name, juce::CachedValue<bool>& value, std::function<bool()> isUpdating)
        : cachedValue (value), updatingCheck (std::move (isUpdating))
    {
        toggle.setButtonText (name);
        toggle.setToggleState (value.get(), juce::dontSendNotification);
        toggle.onClick = [this]
        {
            if (updatingCheck != nullptr && updatingCheck())
                return;

            cachedValue = toggle.getToggleState();
        };
        addAndMakeVisible (toggle);
    }

    void refresh()
    {
        toggle.setToggleState (cachedValue.get(), juce::dontSendNotification);
    }

    void resized() override
    {
        toggle.setBounds (getLocalBounds().reduced (2));
    }

    static int preferredHeight() { return compactRowHeight; }

private:
    juce::CachedValue<bool>& cachedValue;
    std::function<bool()> updatingCheck;
    juce::ToggleButton toggle;
};

class ScrollablePanel : public juce::Component
{
public:
    ScrollablePanel()
    {
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
    }

    void clearContent()
    {
        rows.clear();
        customRows.clear();
        rowHeights.clear();
        content.removeAllChildren();
    }

    int getDynamicRowCount() const { return rows.size(); }

    void removeDynamicRowsFrom (int index)
    {
        while (rows.size() > index)
            rows.remove (rows.size() - 1);

        layoutRows();
    }

    template <typename RowType, typename... Args>
    RowType& addRow (Args&&... args)
    {
        auto* row = new RowType (std::forward<Args> (args)...);
        rows.add (row);
        content.addAndMakeVisible (row);
        layoutRows();
        return *row;
    }

    void addCustomRow (juce::Component& row, int height)
    {
        customRows.add (&row);
        content.addAndMakeVisible (&row);
        rowHeights.add (height);
        layoutRows();
    }

    void layoutRows()
    {
        int y = 0;

        for (auto* row : rows)
        {
            const int h = row->getHeight() > 0 ? row->getHeight() : rowHeight;
            row->setBounds (0, y, panelWidth - 16, h);
            y += h;
        }

        for (int i = 0; i < customRows.size(); ++i)
        {
            const int h = rowHeights[i];
            customRows.getUnchecked (i)->setBounds (0, y, panelWidth - 16, h);
            y += h;
        }

        content.setSize (panelWidth - 16, y);
    }

    void resized() override
    {
        viewport.setBounds (getLocalBounds());
        layoutRows();
    }

private:
    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<juce::Component> rows;
    juce::Array<juce::Component*> customRows;
    juce::Array<int> rowHeights;
};

class RefreshablePanel
{
public:
    virtual ~RefreshablePanel() = default;
    virtual void refreshFromModel() = 0;
};

class GlobalPanel : public ScrollablePanel,
                    public RefreshablePanel
{
public:
    GlobalPanel (te::FourOscPlugin& synthPlugin, std::function<bool()> isUpdating)
        : synth (synthPlugin)
    {
        voiceModeRow = &addRow<IntComboRow> ("Voice Mode", synth.voiceModeValue,
                                             SynthHelpers::getVoiceModeNames(), isUpdating);
        voicesRow = &addRow<CachedIntSliderRow> ("Poly Voices", synth.voicesValue, isUpdating,
                                                 juce::Range<int> (1, 64));

        if (synth.legato != nullptr)
            legatoRow = &addRow<AutomatableSliderRow> (*synth.legato, isUpdating);

        if (synth.masterLevel != nullptr)
            masterRow = &addRow<AutomatableSliderRow> (*synth.masterLevel, isUpdating);

        analogRow = &addRow<BoolToggleRow> ("Amp Analog", synth.ampAnalogValue, isUpdating);
    }

    void refreshFromModel() override
    {
        voiceModeRow->refresh();
        voicesRow->refresh();
        analogRow->refresh();
    }

private:
    te::FourOscPlugin& synth;
    IntComboRow* voiceModeRow = nullptr;
    CachedIntSliderRow* voicesRow = nullptr;
    BoolToggleRow* analogRow = nullptr;
    AutomatableSliderRow* legatoRow = nullptr;
    AutomatableSliderRow* masterRow = nullptr;
};

class OscillatorPanel : public juce::Component,
                        public RefreshablePanel,
                        private juce::ComboBox::Listener
{
public:
    OscillatorPanel (te::FourOscPlugin& synthPlugin, std::function<bool()> isUpdating)
        : synth (synthPlugin), updatingCheck (std::move (isUpdating))
    {
        oscLabel.setText ("Oscillator", juce::dontSendNotification);
        oscLabel.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (oscLabel);

        for (int i = 1; i <= 4; ++i)
            oscSelector.addItem ("Osc " + juce::String (i), i);

        oscSelector.setSelectedId (1, juce::dontSendNotification);
        oscSelector.addListener (this);
        addAndMakeVisible (oscSelector);

        scrollPanel = std::make_unique<ScrollablePanel>();
        addAndMakeVisible (scrollPanel.get());
        rebuildOscRows();
    }

    void refreshFromModel() override
    {
        if (waveRow != nullptr) waveRow->refresh();
        if (voicesRow != nullptr) voicesRow->refresh();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (4);
        auto header = r.removeFromTop (compactRowHeight);
        oscLabel.setBounds (header.removeFromLeft (90));
        oscSelector.setBounds (header);
        scrollPanel->setBounds (r);
    }

private:
    te::FourOscPlugin::OscParams* currentOsc() const
    {
        const int index = oscSelector.getSelectedId() - 1;
        return juce::isPositiveAndBelow (index, synth.oscParams.size()) ? synth.oscParams[index] : nullptr;
    }

    void rebuildOscRows()
    {
        scrollPanel->clearContent();
        waveRow = nullptr;
        voicesRow = nullptr;

        if (auto* osc = currentOsc())
        {
            waveRow = &scrollPanel->addRow<IntComboRow> ("Wave", osc->waveShapeValue,
                                                         SynthHelpers::getOscillatorWaveNames(), updatingCheck);
            voicesRow = &scrollPanel->addRow<CachedIntSliderRow> ("Unison Voices", osc->voicesValue, updatingCheck,
                                                                   juce::Range<int> (1, 8));

            if (osc->level != nullptr) scrollPanel->addRow<AutomatableSliderRow> (*osc->level, updatingCheck);
            if (osc->tune != nullptr) scrollPanel->addRow<AutomatableSliderRow> (*osc->tune, updatingCheck);
            if (osc->fineTune != nullptr) scrollPanel->addRow<AutomatableSliderRow> (*osc->fineTune, updatingCheck);
            if (osc->pulseWidth != nullptr) scrollPanel->addRow<AutomatableSliderRow> (*osc->pulseWidth, updatingCheck);
            if (osc->detune != nullptr) scrollPanel->addRow<AutomatableSliderRow> (*osc->detune, updatingCheck);
            if (osc->spread != nullptr) scrollPanel->addRow<AutomatableSliderRow> (*osc->spread, updatingCheck);
            if (osc->pan != nullptr) scrollPanel->addRow<AutomatableSliderRow> (*osc->pan, updatingCheck);
        }
    }

    void comboBoxChanged (juce::ComboBox* box) override
    {
        if (box == &oscSelector)
            rebuildOscRows();
    }

    te::FourOscPlugin& synth;
    std::function<bool()> updatingCheck;
    std::unique_ptr<ScrollablePanel> scrollPanel;
    juce::Label oscLabel;
    juce::ComboBox oscSelector;
    IntComboRow* waveRow = nullptr;
    CachedIntSliderRow* voicesRow = nullptr;
};

class FilterPanel : public ScrollablePanel,
                    public RefreshablePanel
{
public:
    FilterPanel (te::FourOscPlugin& synthPlugin, std::function<bool()> isUpdating)
    {
        typeRow = &addRow<IntComboRow> ("Filter Type", synthPlugin.filterTypeValue,
                                        SynthHelpers::getFilterTypeNames(), isUpdating);
        slopeRow = &addRow<SlopeComboRow> (synthPlugin.filterSlopeValue, isUpdating);

        if (synthPlugin.filterFreq != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.filterFreq, isUpdating);
        if (synthPlugin.filterResonance != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.filterResonance, isUpdating);
        if (synthPlugin.filterAmount != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.filterAmount, isUpdating);
        if (synthPlugin.filterKey != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.filterKey, isUpdating);
        if (synthPlugin.filterVelocity != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.filterVelocity, isUpdating);
        if (synthPlugin.filterAttack != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.filterAttack, isUpdating);
        if (synthPlugin.filterDecay != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.filterDecay, isUpdating);
        if (synthPlugin.filterSustain != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.filterSustain, isUpdating);
        if (synthPlugin.filterRelease != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.filterRelease, isUpdating);
    }

    void refreshFromModel() override
    {
        typeRow->refresh();
        slopeRow->refresh();
    }

private:
    IntComboRow* typeRow = nullptr;
    SlopeComboRow* slopeRow = nullptr;
};

class AmpPanel : public ScrollablePanel,
                 public RefreshablePanel
{
public:
    explicit AmpPanel (te::FourOscPlugin& synthPlugin, std::function<bool()> isUpdating)
    {
        if (synthPlugin.ampAttack != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.ampAttack, isUpdating);
        if (synthPlugin.ampDecay != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.ampDecay, isUpdating);
        if (synthPlugin.ampSustain != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.ampSustain, isUpdating);
        if (synthPlugin.ampRelease != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.ampRelease, isUpdating);
        if (synthPlugin.ampVelocity != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.ampVelocity, isUpdating);
    }

    void refreshFromModel() override {}
};

class LfoSection : public juce::Component
{
public:
    LfoSection (const juce::String& title, te::FourOscPlugin::LFOParams& lfo, std::function<bool()> isUpdating)
    {
        heading.setText (title, juce::dontSendNotification);
        heading.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (heading);

        rows.add (new IntComboRow ("Wave", lfo.waveShapeValue, SynthHelpers::getLfoWaveNames(), isUpdating));
        rows.add (new BoolToggleRow ("Sync", lfo.syncValue, isUpdating));
        if (lfo.rate != nullptr) rows.add (new AutomatableSliderRow (*lfo.rate, isUpdating));
        rows.add (new CachedFloatSliderRow ("Beat", lfo.beatValue, isUpdating, { 0.25f, 16.0f, 0.25f }));
        if (lfo.depth != nullptr) rows.add (new AutomatableSliderRow (*lfo.depth, isUpdating));

        for (auto* row : rows)
            addAndMakeVisible (row);
    }

    void refresh()
    {
        for (int i = 0; i < rows.size(); ++i)
        {
            if (auto* combo = dynamic_cast<IntComboRow*> (rows[i])) combo->refresh();
            if (auto* toggle = dynamic_cast<BoolToggleRow*> (rows[i])) toggle->refresh();
            if (auto* cached = dynamic_cast<CachedFloatSliderRow*> (rows[i])) cached->refresh();
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (2);
        heading.setBounds (r.removeFromTop (20));
        r.removeFromTop (2);

        for (auto* row : rows)
        {
            const int h = dynamic_cast<BoolToggleRow*> (row) != nullptr || dynamic_cast<IntComboRow*> (row) != nullptr
                            ? compactRowHeight : rowHeight;
            row->setBounds (r.removeFromTop (h));
        }
    }

    int getPreferredHeight() const
    {
        int h = 22;
        for (auto* row : rows)
            h += dynamic_cast<BoolToggleRow*> (row) != nullptr || dynamic_cast<IntComboRow*> (row) != nullptr
                    ? compactRowHeight : rowHeight;
        return h;
    }

private:
    juce::Label heading;
    juce::OwnedArray<juce::Component> rows;
};

class ModEnvSection : public juce::Component
{
public:
    ModEnvSection (const juce::String& title, te::FourOscPlugin::MODEnvParams& env, std::function<bool()> isUpdating)
    {
        heading.setText (title, juce::dontSendNotification);
        heading.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (heading);

        if (env.modAttack != nullptr) rows.add (new AutomatableSliderRow (*env.modAttack, isUpdating));
        if (env.modDecay != nullptr) rows.add (new AutomatableSliderRow (*env.modDecay, isUpdating));
        if (env.modSustain != nullptr) rows.add (new AutomatableSliderRow (*env.modSustain, isUpdating));
        if (env.modRelease != nullptr) rows.add (new AutomatableSliderRow (*env.modRelease, isUpdating));

        for (auto* row : rows)
            addAndMakeVisible (row);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (2);
        heading.setBounds (r.removeFromTop (20));
        r.removeFromTop (2);

        for (auto* row : rows)
            row->setBounds (r.removeFromTop (rowHeight));
    }

    int getPreferredHeight() const
    {
        return 22 + rows.size() * rowHeight;
    }

private:
    juce::Label heading;
    juce::OwnedArray<juce::Component> rows;
};

class ModPanel : public juce::Component,
                 public RefreshablePanel
{
public:
    ModPanel (te::FourOscPlugin& synthPlugin, std::function<bool()> isUpdating)
        : synth (synthPlugin), modMatrix (synthPlugin)
    {
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);

        int y = 0;

        if (synth.lfoParams.size() > 0 && synth.lfoParams[0] != nullptr)
        {
            lfo1 = std::make_unique<LfoSection> ("LFO 1", *synth.lfoParams[0], isUpdating);
            lfo1->setBounds (0, y, panelWidth - 16, lfo1->getPreferredHeight());
            content.addAndMakeVisible (lfo1.get());
            y += lfo1->getPreferredHeight() + 4;
        }

        if (synth.lfoParams.size() > 1 && synth.lfoParams[1] != nullptr)
        {
            lfo2 = std::make_unique<LfoSection> ("LFO 2", *synth.lfoParams[1], isUpdating);
            lfo2->setBounds (0, y, panelWidth - 16, lfo2->getPreferredHeight());
            content.addAndMakeVisible (lfo2.get());
            y += lfo2->getPreferredHeight() + 4;
        }

        if (synth.modEnvParams.size() > 0 && synth.modEnvParams[0] != nullptr)
        {
            env1 = std::make_unique<ModEnvSection> ("Mod Env 1", *synth.modEnvParams[0], isUpdating);
            env1->setBounds (0, y, panelWidth - 16, env1->getPreferredHeight());
            content.addAndMakeVisible (env1.get());
            y += env1->getPreferredHeight() + 4;
        }

        if (synth.modEnvParams.size() > 1 && synth.modEnvParams[1] != nullptr)
        {
            env2 = std::make_unique<ModEnvSection> ("Mod Env 2", *synth.modEnvParams[1], isUpdating);
            env2->setBounds (0, y, panelWidth - 16, env2->getPreferredHeight());
            content.addAndMakeVisible (env2.get());
            y += env2->getPreferredHeight() + 4;
        }

        modMatrix.setBounds (0, y, panelWidth - 16, 120);
        content.addAndMakeVisible (modMatrix);
        y += 124;

        content.setSize (panelWidth - 16, y);
    }

    void refreshFromModel() override
    {
        if (lfo1 != nullptr) lfo1->refresh();
        if (lfo2 != nullptr) lfo2->refresh();
        modMatrix.refreshFromModel();
    }

    void setUpdatingFromModel (bool updating)
    {
        modMatrix.setUpdatingFromModel (updating);
    }

    void resized() override
    {
        viewport.setBounds (getLocalBounds());
    }

private:
    te::FourOscPlugin& synth;
    juce::Viewport viewport;
    juce::Component content;
    std::unique_ptr<LfoSection> lfo1, lfo2;
    std::unique_ptr<ModEnvSection> env1, env2;
    SynthModMatrixPanel modMatrix;
};

class FxPanel : public ScrollablePanel,
                public RefreshablePanel
{
public:
    FxPanel (te::FourOscPlugin& synthPlugin, std::function<bool()> isUpdating)
    {
        distortionOn = &addRow<BoolToggleRow> ("Distortion On", synthPlugin.distortionOnValue, isUpdating);
        if (synthPlugin.distortion != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.distortion, isUpdating);

        reverbOn = &addRow<BoolToggleRow> ("Reverb On", synthPlugin.reverbOnValue, isUpdating);
        if (synthPlugin.reverbSize != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.reverbSize, isUpdating);
        if (synthPlugin.reverbDamping != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.reverbDamping, isUpdating);
        if (synthPlugin.reverbWidth != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.reverbWidth, isUpdating);
        if (synthPlugin.reverbMix != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.reverbMix, isUpdating);

        delayOn = &addRow<BoolToggleRow> ("Delay On", synthPlugin.delayOnValue, isUpdating);
        addRow<CachedFloatSliderRow> ("Delay Time", synthPlugin.delayValue, isUpdating,
                                      juce::NormalisableRange<float> (0.0f, 2000.0f, 1.0f), " ms");
        if (synthPlugin.delayFeedback != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.delayFeedback, isUpdating);
        if (synthPlugin.delayCrossfeed != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.delayCrossfeed, isUpdating);
        if (synthPlugin.delayMix != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.delayMix, isUpdating);

        chorusOn = &addRow<BoolToggleRow> ("Chorus On", synthPlugin.chorusOnValue, isUpdating);
        if (synthPlugin.chorusSpeed != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.chorusSpeed, isUpdating);
        if (synthPlugin.chorusDepth != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.chorusDepth, isUpdating);
        if (synthPlugin.chorusWidth != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.chorusWidth, isUpdating);
        if (synthPlugin.chorusMix != nullptr) addRow<AutomatableSliderRow> (*synthPlugin.chorusMix, isUpdating);
    }

    void refreshFromModel() override
    {
        distortionOn->refresh();
        reverbOn->refresh();
        delayOn->refresh();
        chorusOn->refresh();
    }

private:
    BoolToggleRow* distortionOn = nullptr;
    BoolToggleRow* reverbOn = nullptr;
    BoolToggleRow* delayOn = nullptr;
    BoolToggleRow* chorusOn = nullptr;
};

} // namespace

std::unique_ptr<te::Plugin::EditorComponent> SynthEditor::create (te::FourOscPlugin& synthPlugin)
{
    return std::unique_ptr<te::Plugin::EditorComponent> (new SynthEditor (synthPlugin));
}

SynthEditor::SynthEditor (te::FourOscPlugin& synthPlugin)
    : synth (synthPlugin),
      tabs (juce::TabbedButtonBar::TabsAtTop)
{
    synth.state.addListener (this);

    titleLabel.setText ("4OSC Synth", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    auto isUpdating = [this] { return updatingFromModel; };

    const auto tabBg = AppColours::surfaceBackground (AppLookAndFeel::getCurrentTheme());
    tabs.addTab ("Global", tabBg, new GlobalPanel (synth, isUpdating), true);
    tabs.addTab ("Osc", tabBg, new OscillatorPanel (synth, isUpdating), true);
    tabs.addTab ("Filter", tabBg, new FilterPanel (synth, isUpdating), true);
    tabs.addTab ("Amp", tabBg, new AmpPanel (synth, isUpdating), true);
    tabs.addTab ("Mod", tabBg, new ModPanel (synth, isUpdating), true);
    tabs.addTab ("FX", tabBg, new FxPanel (synth, isUpdating), true);
    addAndMakeVisible (tabs);

    setSize (560, 480);
}

SynthEditor::~SynthEditor()
{
    synth.state.removeListener (this);
}

void SynthEditor::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    refreshFromModel();
}

void SynthEditor::refreshFromModel()
{
    updatingFromModel = true;

    if (auto* modPanel = dynamic_cast<ModPanel*> (tabs.getTabContentComponent (4)))
        modPanel->setUpdatingFromModel (true);

    for (int i = 0; i < tabs.getNumTabs(); ++i)
        if (auto* panel = dynamic_cast<RefreshablePanel*> (tabs.getTabContentComponent (i)))
            panel->refreshFromModel();

    if (auto* modPanel = dynamic_cast<ModPanel*> (tabs.getTabContentComponent (4)))
        modPanel->setUpdatingFromModel (false);

    updatingFromModel = false;
}

void SynthEditor::resized()
{
    auto r = getLocalBounds().reduced (6);
    titleLabel.setBounds (r.removeFromTop (22));
    r.removeFromTop (4);
    tabs.setBounds (r);
}

} // namespace skeletonhive
