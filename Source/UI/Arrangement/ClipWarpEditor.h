#pragma once

#include "EditViewState.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

/** Interactive warp-marker editor for a single te::AudioClipBase. */
class ClipWarpEditor : public juce::Component,
                       private juce::Timer,
                       private te::ValueTreeAllEventListener
{
public:
    explicit ClipWarpEditor (EditViewState& editViewState);

    void setClip (te::AudioClipBase* clip);
    te::AudioClipBase* getClip() const { return audioClip; }
    void requestTransientMarkers();

    void grabEditorFocus();

    void resized() override;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void timerCallback() override;
    void valueTreeChanged() override {}
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override;

    void attachListener();
    void detachListener();
    void refreshThumbnail();
    void releaseThumbnail();
    void refreshFromModel();
    void showMarkerContextMenu (juce::Point<int> screenPosition, int markerIndex);
    void applyMarkerDrag (const juce::MouseEvent& e);
    juce::String markerStatusText() const;

    juce::Rectangle<int> waveformArea() const;
    juce::Rectangle<int> statusArea() const;
    double timeAtX (int x) const;
    int xForTime (double seconds) const;
    int hitTestMarker (juce::Point<int> pos) const;
    juce::UndoManager* undoManager() const;

    EditViewState& editViewState;
    te::AudioClipBase* audioClip = nullptr;
    te::Clip::Ptr listenedClip;
    std::shared_ptr<te::SmartThumbnail> thumbnail;
    juce::int64 cachedFileKey = 0;

    int selectedMarker = -1;
    int hoveredMarker = -1;
    int draggingMarker = -1;
    bool waitingForTransients = false;
    bool dragTransactionOpen = false;
};

} // namespace skeletonhive

