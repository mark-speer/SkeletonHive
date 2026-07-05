#include "TimelineOpenGLRenderer.h"
#include "LaneBackgroundCache.h"
#include "TimelineTypes.h"
#include "EditViewState.h"

namespace arrange
{

TimelineOpenGLRenderer::TimelineOpenGLRenderer (EditViewState& viewState, juce::Component& timelineContent)
    : editViewState (viewState), content (timelineContent)
{
    setInterceptsMouseClicks (false, false);
    openGLContext.setRenderer (this);
    openGLContext.setContinuousRepainting (false);
    openGLContext.setComponentPaintingEnabled (true);
}

void TimelineOpenGLRenderer::attach()
{
    openGLContext.attachTo (*this);
}

void TimelineOpenGLRenderer::detach()
{
    openGLContext.detach();
}

void TimelineOpenGLRenderer::syncVisibleRows (const juce::Array<TrackRowInfo>& rows, int timelineWidth)
{
    visibleRows.clear();
    contentWidth = timelineWidth;

    for (const auto& row : rows)
    {
        visibleRows.add ({ row.track->itemID, row.y, row.height });
        editViewState.laneBackgroundCache.ensureImage (editViewState.edit, editViewState, row.track->itemID,
                                                       { 0, 0, timelineWidth, row.height });
    }

    setBounds (0, 0, timelineWidth, content.getHeight());
    openGLContext.triggerRepaint();
}

void TimelineOpenGLRenderer::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void TimelineOpenGLRenderer::newOpenGLContextCreated()
{
}

void TimelineOpenGLRenderer::openGLContextClosing()
{
}

void TimelineOpenGLRenderer::renderOpenGL()
{
    juce::OpenGLHelpers::clear (juce::Colours::transparentBlack);

    using namespace juce::gl;

    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (const auto& row : visibleRows)
    {
        const auto image = editViewState.laneBackgroundCache.getCachedImage (row.trackId,
                                                                             editViewState.getPixelsPerBeat(),
                                                                             editViewState.viewX1.get(),
                                                                             editViewState.viewX2.get(),
                                                                             row.height,
                                                                             editViewState.showGrid.get());
        if (! image.isValid())
            continue;

        juce::OpenGLTexture texture;
        texture.loadImage (image);
        texture.bind();

        glBegin (GL_QUADS);
        glTexCoord2f (0.0f, 1.0f); glVertex2f ((GLfloat) 0, (GLfloat) row.y);
        glTexCoord2f (1.0f, 1.0f); glVertex2f ((GLfloat) contentWidth, (GLfloat) row.y);
        glTexCoord2f (1.0f, 0.0f); glVertex2f ((GLfloat) contentWidth, (GLfloat) (row.y + row.height));
        glTexCoord2f (0.0f, 0.0f); glVertex2f ((GLfloat) 0, (GLfloat) (row.y + row.height));
        glEnd();
    }
}

} // namespace arrange
