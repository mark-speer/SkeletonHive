#include "ClipWarpEditor.h"

#include "Engine/WarpEngine.h"
#include "UI/AppLookAndFeel.h"

namespace skeletonhive
{

namespace
{
constexpr int markerHitRadius = 6;
constexpr double transientSnapDistanceSeconds = 0.02;
} // namespace

ClipWarpEditor::ClipWarpEditor (EditViewState& evs)
    : editViewState (evs)
{
    setWantsKeyboardFocus (true);
}

juce::UndoManager* ClipWarpEditor::undoManager() const
{
    return audioClip != nullptr ? &audioClip->edit.getUndoManager() : nullptr;
}

void ClipWarpEditor::setClip (te::AudioClipBase* clip)
{
    if (audioClip == clip)
        return;

    detachListener();
    audioClip = clip;
    selectedMarker = -1;
    hoveredMarker = -1;
    draggingMarker = -1;
    waitingForTransients = false;
    dragTransactionOpen = false;
    stopTimer();
    releaseThumbnail();

    if (audioClip != nullptr && WarpEngine::supportsWarp (*audioClip))
    {
        attachListener();
        refreshThumbnail();
    }

    refreshFromModel();
    repaint();
}

void ClipWarpEditor::grabEditorFocus()
{
    grabKeyboardFocus();
    repaint();
}

void ClipWarpEditor::attachListener()
{
    if (audioClip == nullptr)
        return;

    if (auto* clip = dynamic_cast<te::Clip*> (audioClip))
    {
        listenedClip = clip;
        listenedClip->state.addListener (this);
    }
}

void ClipWarpEditor::detachListener()
{
    if (listenedClip != nullptr)
    {
        listenedClip->state.removeListener (this);
        listenedClip = nullptr;
    }
}

void ClipWarpEditor::refreshThumbnail()
{
    releaseThumbnail();

    if (audioClip == nullptr)
        return;

    const auto file = WarpEngine::getSourceFile (*audioClip);

    if (! file.isValid())
        return;

    cachedFileKey = (juce::int64) file.getHash();
    thumbnail = editViewState.waveformCache.acquire (audioClip->edit.engine, file, *this, &audioClip->edit);
}

void ClipWarpEditor::releaseThumbnail()
{
    if (thumbnail != nullptr && audioClip != nullptr)
        editViewState.waveformCache.suggestEviction (WarpEngine::getSourceFile (*audioClip));

    thumbnail.reset();
    cachedFileKey = 0;
}

void ClipWarpEditor::refreshFromModel()
{
    repaint();
}

void ClipWarpEditor::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    refreshFromModel();
}

void ClipWarpEditor::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&)
{
    refreshFromModel();
}

void ClipWarpEditor::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int)
{
    refreshFromModel();
}

void ClipWarpEditor::valueTreeChildOrderChanged (juce::ValueTree&, int, int)
{
    refreshFromModel();
}

void ClipWarpEditor::requestTransientMarkers()
{
    if (audioClip == nullptr || ! WarpEngine::isWarpEnabled (*audioClip))
        return;

    const auto [ready, transients] = WarpEngine::getTransientTimesSeconds (*audioClip);

    if (ready)
    {
        if (! transients.isEmpty())
            WarpEngine::addMarkersAtTransients (*audioClip, undoManager());

        repaint();
        return;
    }

    waitingForTransients = true;
    startTimerHz (4);
}

void ClipWarpEditor::timerCallback()
{
    if (audioClip == nullptr || ! waitingForTransients)
    {
        stopTimer();
        return;
    }

    const auto [ready, transients] = WarpEngine::getTransientTimesSeconds (*audioClip);

    if (! ready)
        return;

    waitingForTransients = false;
    stopTimer();

    if (! transients.isEmpty())
        WarpEngine::addMarkersAtTransients (*audioClip, undoManager());

    repaint();
}

juce::Rectangle<int> ClipWarpEditor::waveformArea() const
{
    auto area = getLocalBounds().reduced (4, 4);
    area.removeFromBottom (16);
    return area;
}

juce::Rectangle<int> ClipWarpEditor::statusArea() const
{
    auto area = getLocalBounds().reduced (4, 2);
    return area.removeFromBottom (14);
}

double ClipWarpEditor::timeAtX (int x) const
{
    const auto area = waveformArea();

    if (area.isEmpty() || audioClip == nullptr)
        return 0.0;

    const double length = juce::jmax (0.001, WarpEngine::getSourceLengthSeconds (*audioClip));
    const double rel = juce::jlimit (0.0, 1.0, double (x - area.getX()) / double (juce::jmax (1, area.getWidth())));
    return rel * length;
}

int ClipWarpEditor::xForTime (double seconds) const
{
    const auto area = waveformArea();

    if (area.isEmpty() || audioClip == nullptr)
        return area.getX();

    const double length = juce::jmax (0.001, WarpEngine::getSourceLengthSeconds (*audioClip));
    const double rel = juce::jlimit (0.0, 1.0, seconds / length);
    return area.getX() + juce::roundToInt (rel * area.getWidth());
}

int ClipWarpEditor::hitTestMarker (juce::Point<int> pos) const
{
    if (audioClip == nullptr || ! WarpEngine::isWarpEnabled (*audioClip))
        return -1;

    const auto markers = WarpEngine::getMarkers (*audioClip);
    int bestIndex = -1;
    int bestDistance = markerHitRadius * markerHitRadius + 1;

    for (int i = 0; i < markers.size(); ++i)
    {
        const int markerX = xForTime (markers.getReference (i).sourceTimeSeconds);
        const int distance = (pos.x - markerX) * (pos.x - markerX)
                           + (pos.y - waveformArea().getCentreY()) * (pos.y - waveformArea().getCentreY());

        if (distance <= markerHitRadius * markerHitRadius && distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = i;
        }
    }

    return bestIndex;
}

void ClipWarpEditor::applyMarkerDrag (const juce::MouseEvent& e)
{
    if (audioClip == nullptr || draggingMarker < 0)
        return;

    const auto markers = WarpEngine::getMarkers (*audioClip);

    if (! juce::isPositiveAndBelow (draggingMarker, markers.size()))
        return;

    double newWarpTime = timeAtX (e.x);
    newWarpTime = WarpEngine::snapWarpTimeToTransient (*audioClip, newWarpTime,
                                                       ! e.mods.isAltDown(),
                                                       transientSnapDistanceSeconds);
    WarpEngine::moveMarker (*audioClip, draggingMarker, newWarpTime, undoManager());
}

void ClipWarpEditor::showMarkerContextMenu (juce::Point<int> screenPosition, int markerIndex)
{
    if (audioClip == nullptr)
        return;

    const auto markers = WarpEngine::getMarkers (*audioClip);
    juce::PopupMenu menu;

    menu.addItem (1, "Add Marker Here");
    menu.addSeparator();

    if (juce::isPositiveAndBelow (markerIndex, markers.size()))
    {
        menu.addItem (2, "Reset Warp Time");
        menu.addItem (3, "Delete Marker", WarpEngine::canRemoveMarker (markerIndex, markers.size()));
    }

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea ({ screenPosition.x, screenPosition.y, 1, 1 }),
                        [safeThis = juce::Component::SafePointer<ClipWarpEditor> (this), markerIndex] (int result)
    {
        if (safeThis == nullptr || safeThis->audioClip == nullptr)
            return;

        auto* um = safeThis->undoManager();

        switch (result)
        {
            case 1:
            {
                const auto pos = safeThis->getMouseXYRelative();
                const double sourceTime = safeThis->timeAtX (pos.x);
                safeThis->selectedMarker = WarpEngine::insertMarkerAtSourceTime (*safeThis->audioClip, sourceTime, um);
                safeThis->repaint();
                break;
            }
            case 2:
                if (markerIndex >= 0)
                {
                    WarpEngine::resetMarkerWarpTime (*safeThis->audioClip, markerIndex, um);
                    safeThis->repaint();
                }
                break;
            case 3:
                if (markerIndex >= 0)
                {
                    WarpEngine::removeMarker (*safeThis->audioClip, markerIndex, um);
                    safeThis->selectedMarker = -1;
                    safeThis->repaint();
                }
                break;
            default:
                break;
        }
    });
}

void ClipWarpEditor::mouseDown (const juce::MouseEvent& e)
{
    if (audioClip == nullptr || ! WarpEngine::isWarpEnabled (*audioClip))
        return;

    if (e.mods.isPopupMenu())
    {
        selectedMarker = hitTestMarker (e.getPosition());
        showMarkerContextMenu (e.getScreenPosition(), selectedMarker);
        return;
    }

    grabKeyboardFocus();
    selectedMarker = hitTestMarker (e.getPosition());
    draggingMarker = selectedMarker;

    if (draggingMarker >= 0 && undoManager() != nullptr)
    {
        undoManager()->beginNewTransaction ("Move Warp Marker");
        dragTransactionOpen = true;
    }
}

void ClipWarpEditor::mouseDrag (const juce::MouseEvent& e)
{
    applyMarkerDrag (e);
}

void ClipWarpEditor::mouseUp (const juce::MouseEvent&)
{
    draggingMarker = -1;
    dragTransactionOpen = false;
}

void ClipWarpEditor::mouseMove (const juce::MouseEvent& e)
{
    const int hit = hitTestMarker (e.getPosition());

    if (hit != hoveredMarker)
    {
        hoveredMarker = hit;
        repaint();
    }

    setMouseCursor (hit >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void ClipWarpEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (audioClip == nullptr || ! WarpEngine::isWarpEnabled (*audioClip))
        return;

    if (hitTestMarker (e.getPosition()) >= 0)
        return;

    const double sourceTime = timeAtX (e.x);
    selectedMarker = WarpEngine::insertMarkerAtSourceTime (*audioClip, sourceTime, undoManager());
    repaint();
}

bool ClipWarpEditor::keyPressed (const juce::KeyPress& key)
{
    if (audioClip == nullptr)
        return false;

    if (key == juce::KeyPress::escapeKey)
    {
        selectedMarker = -1;
        repaint();
        return true;
    }

    if (selectedMarker < 0)
        return false;

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        const auto markers = WarpEngine::getMarkers (*audioClip);

        if (WarpEngine::canRemoveMarker (selectedMarker, markers.size()))
        {
            WarpEngine::removeMarker (*audioClip, selectedMarker, undoManager());
            selectedMarker = -1;
            return true;
        }
    }

    return false;
}

juce::String ClipWarpEditor::markerStatusText() const
{
    if (audioClip == nullptr || selectedMarker < 0)
        return {};

    const auto markers = WarpEngine::getMarkers (*audioClip);

    if (! juce::isPositiveAndBelow (selectedMarker, markers.size()))
        return {};

    const auto& marker = markers.getReference (selectedMarker);
    return juce::String ("src ") + juce::String (marker.sourceTimeSeconds, 3) + " s"
         + juce::String ("  warp ") + juce::String (marker.warpTimeSeconds, 3) + " s";
}

void ClipWarpEditor::paint (juce::Graphics& g)
{
    const auto area = waveformArea();

    g.setColour (AppColours::automationPanelBackground (AppLookAndFeel::getCurrentTheme()).brighter (0.06f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

    if (audioClip == nullptr || ! WarpEngine::supportsWarp (*audioClip))
    {
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.drawText ("Warp editing requires a wave clip", area, juce::Justification::centred, false);
        return;
    }

    if (thumbnail != nullptr)
    {
        g.setColour (juce::Colours::white.withAlpha (0.65f));
        const double length = WarpEngine::getSourceLengthSeconds (*audioClip);
        const te::TimeRange fullRange { te::TimePosition(), te::TimePosition::fromSeconds (length) };
        thumbnail->drawChannels (g, area, fullRange, 1.0f);
    }

    if (! WarpEngine::isWarpEnabled (*audioClip))
    {
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.drawText ("Enable warp to edit markers", area, juce::Justification::centred, false);
        return;
    }

    const auto markers = WarpEngine::getMarkers (*audioClip);
    const auto [transientsReady, transients] = WarpEngine::getTransientTimesSeconds (*audioClip);

    if (transientsReady)
    {
        g.setColour (juce::Colours::orange.withAlpha (0.35f));

        for (auto transient : transients)
        {
            const int x = xForTime (transient);
            g.drawVerticalLine (x, (float) area.getY(), (float) area.getBottom());
        }
    }
    else if (waitingForTransients)
    {
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawText ("Detecting transients…", statusArea(), juce::Justification::centredRight, false);
    }

    for (int i = 0; i < markers.size(); ++i)
    {
        const int x = xForTime (markers.getReference (i).sourceTimeSeconds);
        const bool selected = i == selectedMarker;
        const bool hovered = i == hoveredMarker;
        const bool isEndPoint = WarpEngine::isEndpointMarker (i, markers.size());

        g.setColour (selected ? juce::Colours::yellow
                              : (hovered ? juce::Colours::white.withAlpha (0.95f)
                                         : (isEndPoint ? juce::Colours::lightblue : juce::Colours::white.withAlpha (0.85f))));
        g.drawVerticalLine (x, (float) area.getY(), (float) area.getBottom());

        juce::Path handle;
        handle.addTriangle ((float) x - 4.0f, (float) area.getY(),
                            (float) x + 4.0f, (float) area.getY(),
                            (float) x, (float) area.getY() + 7.0f);
        g.fillPath (handle);
    }

    const auto status = markerStatusText();

    if (status.isNotEmpty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (status, statusArea(), juce::Justification::centredLeft, false);
    }
    else
    {
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("Double-click to add  |  Drag to stretch  |  Alt bypasses snap",
                    statusArea(), juce::Justification::centredLeft, false);
    }
}

void ClipWarpEditor::resized()
{
}

} // namespace skeletonhive

