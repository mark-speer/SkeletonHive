#pragma once

#include "UI/Arrangement/EditViewState.h"

namespace skeletonhive
{

class PerformanceMacroPanel : public juce::Component
{
public:
    PerformanceMacroPanel (te::Edit& edit, EditViewState& viewState);

    static constexpr int preferredHeight = 160;

    void setFocusedTrack (te::EditItemID trackId);

private:
    void rebuild();
    void resized() override;
    void paint (juce::Graphics& g) override;

    te::Edit& edit;
    EditViewState& editViewState;
    te::EditItemID focusedTrackId;
    juce::Viewport viewport;
    juce::Component content;
    juce::Label emptyLabel { {}, "Select a session slot to show rack macros" };
};

} // namespace skeletonhive
