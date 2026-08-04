#include "TakeLaneComponent.h"
#include "ClipComponents.h"
#include "UI/AppLookAndFeel.h"

namespace skeletonhive
{

namespace
{
constexpr int labelWidth = 52;
} // namespace

//==============================================================================
// TakeLaneComponent

TakeLaneComponent::TakeLaneComponent (EditViewState& evs, te::Clip& parent, int index, bool isComp)
    : editViewState (evs), parentClip (parent), takeIndex (index), compLane (isComp)
{
    if (compLane)
        startTimerHz (15);
}

TakeLaneComponent::~TakeLaneComponent()
{
    stopTimer();
    releaseThumbnail();
}

void TakeLaneComponent::refreshLayout()
{
    ensureThumbnail();
    repaint();
}

void TakeLaneComponent::releaseResources()
{
    releaseThumbnail();
}

juce::Colour TakeLaneComponent::colourForTakeIndex (int index) const
{
    return AppColours::clipGroupPalette (index % 6);
}

double TakeLaneComponent::compTimeAtX (int x) const
{
    if (auto* cm = EngineHelpers::getCompManager (*parentClip))
    {
        const auto compRange = cm->getCompRange();
        const auto contentWidth = juce::jmax (1, getWidth() - labelWidth);
        const auto relX = juce::jlimit (0, contentWidth, x - labelWidth);
        return compRange.getStart()
             + compRange.getLength() * (double (relX) / double (contentWidth));
    }

    return 0.0;
}

int TakeLaneComponent::xForCompTime (double time) const
{
    if (auto* cm = EngineHelpers::getCompManager (*parentClip))
    {
        const auto compRange = cm->getCompRange();
        const auto contentWidth = juce::jmax (1, getWidth() - labelWidth);
        const auto rel = (time - compRange.getStart()) / juce::jmax (0.001, compRange.getLength());
        return labelWidth + juce::roundToInt (rel * contentWidth);
    }

    return labelWidth;
}

void TakeLaneComponent::ensureThumbnail()
{
    if (compLane)
        return;

    if (dynamic_cast<te::WaveAudioClip*> (parentClip.get()) == nullptr)
        return;

    const auto file = EngineHelpers::getTakeSourceFile (*parentClip, takeIndex);

    if (! file.existsAsFile())
    {
        releaseThumbnail();
        return;
    }

    const te::AudioFile audioFile (editViewState.edit.engine, file);
    cachedThumbnailFile = file;

    if (thumbnailHeld && thumbnail != nullptr)
        return;

    releaseThumbnail();
    thumbnail = editViewState.waveformCache.acquire (editViewState.edit.engine, audioFile, *this, &editViewState.edit);
    thumbnailHeld = thumbnail != nullptr;
}

void TakeLaneComponent::releaseThumbnail()
{
    if (thumbnailHeld && cachedThumbnailFile.existsAsFile())
        editViewState.waveformCache.suggestEviction (te::AudioFile (editViewState.edit.engine, cachedThumbnailFile));

    thumbnail.reset();
    thumbnailHeld = false;
    cachedThumbnailFile = juce::File();
}

void TakeLaneComponent::paintWaveform (juce::Graphics& g, juce::Rectangle<int> area)
{
    if (thumbnail == nullptr || area.isEmpty())
        return;

    g.setColour (juce::Colours::white.withAlpha (0.75f));
    const te::TimeRange viewRange { editViewState.viewX1, editViewState.viewX2 };
    thumbnail->drawChannels (g, area, viewRange, 1.0f);
}

void TakeLaneComponent::paintMidiPreview (juce::Graphics& g, juce::Rectangle<int> area)
{
    if (auto* midi = dynamic_cast<te::MidiClip*> (parentClip.get()))
    {
        if (auto* seq = midi->getTakeSequence (takeIndex))
        {
            const auto& ts = editViewState.edit.tempoSequence;
            const auto clipRange = midi->getPosition().time;
            const double clipLengthBeats = (ts.toBeats (clipRange.getEnd()) - ts.toBeats (clipRange.getStart())).inBeats();

            if (clipLengthBeats <= 0.0)
                return;

            const int lowestNote = 36, highestNote = 96, noteRange = highestNote - lowestNote;
            g.setColour (juce::Colours::white.withAlpha (0.75f));

            for (int i = 0; i < seq->getNumNotes(); ++i)
            {
                const auto* note = seq->getNote (i);
                const float x = (float) (note->getStartBeat().inBeats() / clipLengthBeats) * (float) area.getWidth();
                const float w = juce::jmax (2.0f, (float) (note->getLengthBeats().inBeats() / clipLengthBeats) * (float) area.getWidth());
                const float y = area.getHeight() * (1.0f - (float) (note->getNoteNumber() - lowestNote) / (float) noteRange);
                const float h = juce::jmax (2.0f, area.getHeight() / (float) noteRange);
                g.fillRect (area.getX() + x, area.getY() + y - h, w, h);
            }
        }
    }
}

void TakeLaneComponent::paintCompSections (juce::Graphics& g, juce::Rectangle<int> area)
{
    auto* cm = EngineHelpers::getCompManager (*parentClip.get());
    if (cm == nullptr || area.isEmpty())
        return;

    const auto compRange = cm->getCompRange();
    if (compRange.getLength() <= 0.0)
        return;

    int compTakeIndex = -1;

    for (int i = 0; i < cm->getTotalNumTakes(); ++i)
    {
        if (cm->isTakeComp (i))
        {
            compTakeIndex = i;
            break;
        }
    }

    if (compTakeIndex < 0)
        return;

    const auto compTree = cm->getTakesTree().getChild (compTakeIndex);
    double prevEnd = compRange.getStart();

    for (int i = 0; i < compTree.getNumChildren(); ++i)
    {
        const auto section = compTree.getChild (i);
        const double endTime = section.getProperty (te::IDs::endTime);
        const int take = section.getProperty (te::IDs::takeIndex);

        const int x1 = xForCompTime (prevEnd);
        const int x2 = xForCompTime (endTime);
        auto sectionArea = juce::Rectangle<int> (x1, area.getY(), juce::jmax (2, x2 - x1), area.getHeight());

        g.setColour (colourForTakeIndex (take).withAlpha (0.45f));
        g.fillRect (sectionArea);

        if (i < compTree.getNumChildren() - 1)
        {
            g.setColour (juce::Colours::white.withAlpha (hoveredBoundary == i ? 0.9f : 0.35f));
            g.fillRect (x2 - 1, area.getY(), 2, area.getHeight());
        }

        prevEnd = endTime;
    }

    const float progress = cm->getRenderProgress();
    if (progress < 1.0f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.fillRect (area.getX(), area.getBottom() - 3, juce::roundToInt (area.getWidth() * progress), 3);
    }

    const auto warning = cm->getWarning();
    if (warning.isNotEmpty())
    {
        g.setColour (juce::Colours::orange.withAlpha (0.9f));
        g.setFont (juce::FontOptions (9.0f));
        g.drawText (warning, area.reduced (2), juce::Justification::centredRight, true);
    }
}

void TakeLaneComponent::paint (juce::Graphics& g)
{
    const bool active = parentClip->getCurrentTake() == takeIndex;
    const auto bg = active ? juce::Colour (0xff2a3a5c) : juce::Colour (0xff141428);
    g.fillAll (bg);

    auto bounds = getLocalBounds();
    auto labelArea = bounds.removeFromLeft (labelWidth);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.fillRect (labelArea);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (juce::FontOptions (9.0f));

    juce::String label = compLane ? "Comp" : EngineHelpers::getTakeName (*parentClip, takeIndex);
    g.drawText (label, labelArea.reduced (2, 0), juce::Justification::centredLeft, true);

    if (active && ! compLane)
    {
        g.setColour (AppColours::accentLoop (AppLookAndFeel::getCurrentTheme()));
        g.fillRect (0, 0, 3, getHeight());
    }

    auto content = bounds.reduced (1, 1);

    if (compLane)
        paintCompSections (g, content);
    else if (dynamic_cast<te::WaveAudioClip*> (parentClip.get()) != nullptr)
        paintWaveform (g, content);
    else
        paintMidiPreview (g, content);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

int TakeLaneComponent::sectionBoundaryAtX (int x, juce::ValueTree& outSection) const
{
    auto* cm = EngineHelpers::getCompManager (*parentClip.get());
    if (cm == nullptr)
        return -1;

    const double time = compTimeAtX (x);
    bool atStart = false;
    const int sectionIndex = cm->findSectionWithEndTime ({ time - 0.001, time + 0.001 }, -1, atStart);

    if (sectionIndex >= 0)
    {
        outSection = cm->getActiveTakeTree().getChild (sectionIndex);
        return sectionIndex;
    }

    return -1;
}

void TakeLaneComponent::mouseDown (const juce::MouseEvent& e)
{
    if (compLane)
    {
        juce::ValueTree section;
        const int boundary = sectionBoundaryAtX (e.x, section);

        if (boundary >= 0 && section.isValid())
        {
            draggingSection = true;
            draggedSection = section;
            dragAnchorCompTime = compTimeAtX (e.x);
            return;
        }

        const double time = compTimeAtX (e.x);
        if (auto* cm = EngineHelpers::getCompManager (*parentClip))
        {
            if (! cm->isCurrentTakeComp())
                EngineHelpers::ensureCompTake (*parentClip);

            int selectedTake = parentClip->getCurrentTake();
            if (cm->isTakeComp (selectedTake))
            {
                for (int i = 0; i < cm->getTotalNumTakes(); ++i)
                {
                    if (! cm->isTakeComp (i))
                    {
                        selectedTake = i;
                        break;
                    }
                }
            }

            cm->changeSectionIndexAtTime (time, selectedTake);
            cm->triggerCompRender();

            if (onCompChanged)
                onCompChanged();
        }

        return;
    }

    EngineHelpers::setActiveTake (*parentClip, takeIndex);
    repaint();
}

void TakeLaneComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! compLane || ! draggingSection || ! draggedSection.isValid())
        return;

    const double newTime = compTimeAtX (e.x);

    if (auto* cm = EngineHelpers::getCompManager (*parentClip))
    {
        cm->moveSectionEndTime (draggedSection, newTime);
        cm->triggerCompRender();

        if (onCompChanged)
            onCompChanged();
    }
}

void TakeLaneComponent::mouseUp (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    draggingSection = false;
    draggedSection = {};
}

void TakeLaneComponent::mouseMove (const juce::MouseEvent& e)
{
    if (! compLane)
        return;

    juce::ValueTree section;
    const int boundary = sectionBoundaryAtX (e.x, section);

    if (boundary != hoveredBoundary)
    {
        hoveredBoundary = boundary;
        repaint();
    }

    setMouseCursor (boundary >= 0 ? juce::MouseCursor::LeftRightResizeCursor
                                 : juce::MouseCursor::PointingHandCursor);
}

void TakeLaneComponent::timerCallback()
{
    if (compLane)
        repaint();
}

//==============================================================================
// TakeLaneStack

TakeLaneStack::TakeLaneStack (EditViewState& evs, te::Clip& parent)
    : editViewState (evs), clip (parent)
{
    clip->state.addListener (this);
    rebuildLanes();
}

TakeLaneStack::~TakeLaneStack()
{
    clip->state.removeListener (this);
    releaseResources();
}

void TakeLaneStack::releaseResources()
{
    for (auto* lane : lanes)
        lane->releaseResources();
}

void TakeLaneStack::refreshLayout()
{
    const auto pos = clip->getPosition();
    const int x = editViewState.timeToX (pos.getStart());
    const int w = juce::jmax (4, editViewState.timeToX (pos.getEnd()) - x);
    const int h = stackHeight();

    setBounds (x, 0, w, h);

    int y = 0;

    for (auto* lane : lanes)
    {
        const int laneH = lane->isCompLane() ? compLaneStripHeight : takeLaneStripHeight;
        lane->setBounds (0, y, w, laneH);
        lane->refreshLayout();
        y += laneH;
    }
}

int TakeLaneStack::stackHeight() const
{
    int h = 0;

    for (auto* lane : lanes)
        h += lane->isCompLane() ? compLaneStripHeight : takeLaneStripHeight;

    return h;
}

void TakeLaneStack::rebuildLanes()
{
    lanes.clear();

    if (! EngineHelpers::hasMultipleTakes (*clip))
        return;

    if (auto* cm = EngineHelpers::getCompManager (*clip))
    {
        int compIndex = -1;

        for (int i = 0; i < cm->getTotalNumTakes(); ++i)
        {
            if (cm->isTakeComp (i))
            {
                compIndex = i;
                break;
            }
        }

        if (compIndex < 0)
            compIndex = cm->getActiveTakeIndex();

        auto* compLanePtr = lanes.add (new TakeLaneComponent (editViewState, *clip, compIndex, true));
        compLanePtr->onCompChanged = [this]
        {
            if (onLayoutChanged)
                onLayoutChanged();
            repaint();
        };
        addAndMakeVisible (compLanePtr);
    }

    const int totalTakes = EngineHelpers::getTakeCount (*clip, true);

    for (int i = 0; i < totalTakes; ++i)
    {
        if (auto* cm = EngineHelpers::getCompManager (*clip))
        {
            if (cm->isTakeComp (i))
                continue;
        }

        auto* lane = lanes.add (new TakeLaneComponent (editViewState, *clip, i, false));
        addAndMakeVisible (lane);
    }

    refreshLayout();
}

void TakeLaneStack::handleAsyncUpdate()
{
    rebuildLanes();

    if (onLayoutChanged)
        onLayoutChanged();
}

} // namespace skeletonhive
