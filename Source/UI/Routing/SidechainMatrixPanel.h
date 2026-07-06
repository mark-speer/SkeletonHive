#pragma once

#include "TracktionCommon.h"
#include "Engine/SidechainRouting.h"

namespace skeletonhive
{

/** Edit-wide sidechain routing matrix (one source track per sidechain-capable plugin). */
class SidechainMatrixPanel : public juce::Component,
                             private juce::ValueTree::Listener,
                             private juce::AsyncUpdater
{
public:
    explicit SidechainMatrixPanel (te::Edit& edit);
    ~SidechainMatrixPanel() override;

    void focusPlugin (te::EditItemID pluginId);

private:
    void resized() override;
    void paint (juce::Graphics& g) override;

    void handleAsyncUpdate() override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    void markDirty();
    void rebuildFromEdit();
    void rebuildGrid();
    void syncDraftFromControls();
    void applyChanges();
    void cancelChanges();

    te::Edit& edit;

    juce::Label titleLabel;
    juce::Label helpLabel;
    juce::TextButton applyButton { "Apply" };
    juce::TextButton cancelButton { "Cancel" };

    juce::Viewport gridViewport;
    juce::Component gridContent;

    juce::Array<SidechainMatrixRow> draftRows;
    juce::Array<te::AudioTrack*> sourceTracks;
    te::EditItemID focusPluginId;

    struct RowControls
    {
        te::EditItemID pluginId;
        juce::OwnedArray<juce::ToggleButton> radios;
    };

    juce::OwnedArray<RowControls> rowControls;
    juce::OwnedArray<juce::Label> headerLabels;

    bool dirty = false;
    static constexpr int labelColumnWidth = 200;
    static constexpr int columnWidth = 72;
    static constexpr int rowHeight = 28;
    static constexpr int headerHeight = 30;
};

} // namespace skeletonhive
