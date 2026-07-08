#include "ArrangementSelectionHelpers.h"
#include "TrackComponents.h"
#include "TimelineLOD.h"

namespace skeletonhive
{

void ArrangementSelectionHelpers::handleClipClick (EditViewState& editViewState, te::Clip& clip,
                                                   const juce::ModifierKeys& mods)
{
    if (mods.isShiftDown())
    {
        if (editViewState.selectionManager.isSelected (&clip))
            return;

        editViewState.selectionManager.addToSelection (&clip);
        return;
    }

    if (mods.isCtrlDown() || mods.isCommandDown())
    {
        if (editViewState.selectionManager.isSelected (&clip))
            editViewState.selectionManager.deselect (&clip);
        else
            editViewState.selectionManager.addToSelection (&clip);

        return;
    }

    editViewState.selectionManager.selectOnly (&clip);
}

void ArrangementSelectionHelpers::selectClipsInRect (EditViewState& editViewState,
                                                     const juce::Rectangle<int>& rectInTimelineContent,
                                                     const juce::OwnedArray<TrackLaneComponent>& lanes)
{
    juce::Array<te::Clip*> hits;
    const bool laneLevel = useLaneLevelRendering (editViewState.getPixelsPerBeat());

    for (auto* lane : lanes)
    {
        if (lane == nullptr || lane->getTrack().isFolderTrack())
            continue;

        const auto laneBounds = lane->getBounds();
        if (! rectInTimelineContent.intersects (laneBounds))
            continue;

        if (! laneLevel)
        {
            for (int i = 0; i < lane->getNumChildComponents(); ++i)
            {
                if (auto* clipComp = dynamic_cast<ClipComponent*> (lane->getChildComponent (i)))
                {
                    auto clipBounds = clipComp->getBounds();
                    clipBounds = clipBounds.withPosition (clipBounds.getX(), clipBounds.getY() + laneBounds.getY());

                    if (clipBounds.intersects (rectInTimelineContent))
                        hits.addIfNotAlreadyThere (&clipComp->getClip());
                }
            }
        }

        if (laneLevel)
        {
            if (auto* clipTrack = dynamic_cast<te::ClipTrack*> (&lane->getTrack()))
            {
                for (auto* clipPtr : clipTrack->getClips())
                {
                    if (clipPtr == nullptr)
                        continue;

                    const int x1 = editViewState.timeToX (clipPtr->getPosition().getStart());
                    const int x2 = editViewState.timeToX (clipPtr->getPosition().getEnd());
                    auto clipBounds = juce::Rectangle<int> (x1, laneBounds.getY(),
                                                            juce::jmax (1, x2 - x1), laneBounds.getHeight());

                    if (clipBounds.intersects (rectInTimelineContent))
                        hits.addIfNotAlreadyThere (clipPtr);
                }
            }
        }
    }

    if (hits.isEmpty())
    {
        editViewState.selectionManager.deselectAll();
        return;
    }

    editViewState.selectionManager.selectOnly (hits.getFirst());

    for (int i = 1; i < hits.size(); ++i)
        editViewState.selectionManager.addToSelection (hits[i]);
}

} // namespace skeletonhive
