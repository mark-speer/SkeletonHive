#include "WarpEngine.h"





namespace skeletonhive


{





namespace


{


constexpr double minMarkerSpacingSeconds = 0.05;





te::WaveAudioClip* asWaveClip (te::AudioClipBase& clip)


{


    return dynamic_cast<te::WaveAudioClip*> (&clip);


}





const te::WaveAudioClip* asWaveClip (const te::AudioClipBase& clip)


{


    return dynamic_cast<const te::WaveAudioClip*> (&clip);


}





bool isNearExistingMarker (const te::WarpTimeManager& manager, double sourceSeconds)


{


    for (auto* marker : manager.getMarkers())


    {


        if (std::abs (marker->sourceTime.inSeconds() - sourceSeconds) < minMarkerSpacingSeconds)


            return true;


    }





    return false;


}





double nearestTransient (const juce::Array<double>& transients, double timeSeconds, double maxDistance)


{


    double best = timeSeconds;


    double bestDistance = maxDistance;





    for (auto transient : transients)


    {


        const double distance = std::abs (transient - timeSeconds);





        if (distance <= bestDistance)


        {


            bestDistance = distance;


            best = transient;


        }


    }





    return best;


}


} // namespace





void WarpEngine::beginTransaction (juce::UndoManager* undoManager, const juce::String& name)


{


    if (undoManager != nullptr)


        undoManager->beginNewTransaction (name);


}





bool WarpEngine::supportsWarp (const te::AudioClipBase& clip)


{


    return asWaveClip (clip) != nullptr;


}





bool WarpEngine::isWarpEnabled (const te::AudioClipBase& clip)


{


    return clip.getWarpTime();


}





void WarpEngine::setWarpEnabled (te::AudioClipBase& clip, bool enabled, juce::UndoManager* undoManager)


{


    beginTransaction (undoManager, enabled ? "Enable Warp" : "Disable Warp");


    clip.setWarpTime (enabled);





    if (enabled && supportsWarp (clip))


        getManager (clip);


}





te::AudioFile WarpEngine::getSourceFile (const te::AudioClipBase& clip)


{


    return clip.getAudioFile();


}





double WarpEngine::getSourceLengthSeconds (const te::AudioClipBase& clip)


{


    const auto file = getSourceFile (clip);


    return file.isValid() ? file.getLength() : 0.0;


}





double WarpEngine::getWarpedLengthSeconds (const te::AudioClipBase& clip)


{


    if (! supportsWarp (clip))


        return getSourceLengthSeconds (clip);





    if (! isWarpEnabled (clip))


        return getSourceLengthSeconds (clip);





    return getManager (const_cast<te::AudioClipBase&> (clip)).getWarpedEnd().inSeconds();


}





te::WarpTimeManager& WarpEngine::getManager (te::AudioClipBase& clip)


{


    jassert (supportsWarp (clip));


    return clip.getWarpTimeManager();


}





const te::WarpTimeManager& WarpEngine::getManager (const te::AudioClipBase& clip)


{


    return const_cast<te::AudioClipBase&> (clip).getWarpTimeManager();


}





juce::Array<WarpMarkerInfo> WarpEngine::getMarkers (const te::AudioClipBase& clip)


{


    juce::Array<WarpMarkerInfo> markers;





    if (! supportsWarp (clip))


        return markers;





    for (auto* marker : getManager (clip).getMarkers())


    {


        WarpMarkerInfo info;


        info.sourceTimeSeconds = marker->sourceTime.inSeconds();


        info.warpTimeSeconds = marker->warpTime.inSeconds();


        markers.add (info);


    }





    return markers;


}





int WarpEngine::insertMarkerAtSourceTime (te::AudioClipBase& clip, double sourceTimeSeconds, juce::UndoManager* undoManager)


{


    beginTransaction (undoManager, "Insert Warp Marker");





    auto& manager = getManager (clip);





    if (isNearExistingMarker (manager, sourceTimeSeconds))


        return -1;





    const auto sourceTime = te::TimePosition::fromSeconds (sourceTimeSeconds);


    return manager.insertMarker ({ sourceTime, sourceTime });


}





void WarpEngine::moveMarker (te::AudioClipBase& clip, int index, double warpTimeSeconds, juce::UndoManager* undoManager)


{


    juce::ignoreUnused (undoManager);


    getManager (clip).moveMarker (index, te::TimePosition::fromSeconds (warpTimeSeconds));


}





void WarpEngine::removeMarker (te::AudioClipBase& clip, int index, juce::UndoManager* undoManager)


{


    if (! canRemoveMarker (index, getMarkers (clip).size()))


        return;





    beginTransaction (undoManager, "Remove Warp Marker");


    getManager (clip).removeMarker (index);


}





void WarpEngine::resetMarkerWarpTime (te::AudioClipBase& clip, int index, juce::UndoManager* undoManager)


{


    const auto markers = getMarkers (clip);





    if (! juce::isPositiveAndBelow (index, markers.size()))


        return;





    beginTransaction (undoManager, "Reset Warp Marker");


    moveMarker (clip, index, markers.getReference (index).sourceTimeSeconds, undoManager);


}





std::pair<bool, juce::Array<double>> WarpEngine::getTransientTimesSeconds (const te::AudioClipBase& clip)


{


    juce::Array<double> times;





    if (! supportsWarp (clip))


        return { true, times };





    const auto [ready, transientTimes] = getManager (clip).getTransientTimes();





    for (auto time : transientTimes)


        times.add (time.inSeconds());





    return { ready, times };


}





void WarpEngine::addMarkersAtTransients (te::AudioClipBase& clip, juce::UndoManager* undoManager)


{


    if (! supportsWarp (clip))


        return;





    beginTransaction (undoManager, "Add Transient Markers");





    auto& manager = getManager (clip);


    const auto [ready, transientTimes] = manager.getTransientTimes();





    if (! ready)


        return;





    for (auto time : transientTimes)


    {


        const double sourceSeconds = time.inSeconds();





        if (isNearExistingMarker (manager, sourceSeconds))


            continue;





        manager.insertMarker ({ time, time });


    }


}





bool WarpEngine::isEndpointMarker (int index, int markerCount)


{


    return markerCount > 0 && (index == 0 || index == markerCount - 1);


}





bool WarpEngine::canRemoveMarker (int index, int markerCount)


{


    return markerCount > 2 && index > 0 && index < markerCount - 1;


}





double WarpEngine::snapWarpTimeToTransient (const te::AudioClipBase& clip,


                                            double warpTimeSeconds,


                                            bool enableSnap,


                                            double maxDistanceSeconds)


{


    if (! enableSnap)


        return warpTimeSeconds;





    const auto [ready, transients] = getTransientTimesSeconds (clip);





    if (! ready || transients.isEmpty())


        return warpTimeSeconds;





    return nearestTransient (transients, warpTimeSeconds, maxDistanceSeconds);


}





double WarpEngine::sourceTimeToWarpTimeSeconds (const te::AudioClipBase& clip, double sourceTimeSeconds)


{


    if (! supportsWarp (clip) || ! isWarpEnabled (clip))


        return sourceTimeSeconds;





    return getManager (const_cast<te::AudioClipBase&> (clip))


        .sourceTimeToWarpTime (te::TimePosition::fromSeconds (sourceTimeSeconds))


        .inSeconds();


}




double WarpEngine::warpTimeToClipLocalFraction (const te::AudioClipBase& clip, double warpTimeSeconds)


{


    if (! isWarpEnabled (clip))


        return 0.0;





    const auto markers = getManager (const_cast<te::AudioClipBase&> (clip)).getMarkers();





    if (markers.isEmpty())


        return 0.0;





    const double warpedStart = markers.getFirst()->warpTime.inSeconds();


    const double warpedEnd = markers.getLast()->warpTime.inSeconds();


    const double warpedSpan = juce::jmax (0.001, warpedEnd - warpedStart);





    return juce::jlimit (0.0, 1.0, (warpTimeSeconds - warpedStart) / warpedSpan);


}





} // namespace skeletonhive



