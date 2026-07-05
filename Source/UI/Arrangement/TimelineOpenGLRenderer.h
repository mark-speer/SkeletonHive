#pragma once

#include "TimelineTypes.h"
#include "EditViewState.h"
#include <juce_opengl/juce_opengl.h>

namespace arrange
{

/** Optional OpenGL acceleration for lane grid backgrounds (uses LaneBackgroundCache images). */
class TimelineOpenGLRenderer : public juce::Component,
                               private juce::OpenGLRenderer
{
public:
    TimelineOpenGLRenderer (EditViewState& viewState, juce::Component& timelineContent);

    void attach();
    void detach();
    void syncVisibleRows (const juce::Array<TrackRowInfo>& rows, int timelineWidth);

    void paint (juce::Graphics& g) override;

private:
    struct RowDrawInfo
    {
        te::EditItemID trackId;
        int y = 0;
        int height = 0;
    };

    void newOpenGLContextCreated() override;
    void openGLContextClosing() override;
    void renderOpenGL() override;

    EditViewState& editViewState;
    juce::Component& content;
    juce::OpenGLContext openGLContext;
    juce::Array<RowDrawInfo> visibleRows;
    int contentWidth = 0;
};

} // namespace arrange
