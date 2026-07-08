#pragma once





#include "TracktionCommon.h"





namespace skeletonhive


{





struct WarpMarkerInfo


{


    double sourceTimeSeconds = 0.0;


    double warpTimeSeconds = 0.0;


};





/** Thin helpers over te::WarpTimeManager for clip warp markers and transient detection. */


class WarpEngine


{


public:


    static bool supportsWarp (const te::AudioClipBase& clip);


    static bool isWarpEnabled (const te::AudioClipBase& clip);


    static void setWarpEnabled (te::AudioClipBase& clip, bool enabled, juce::UndoManager* undoManager = nullptr);





    static te::AudioFile getSourceFile (const te::AudioClipBase& clip);


    static double getSourceLengthSeconds (const te::AudioClipBase& clip);


    static double getWarpedLengthSeconds (const te::AudioClipBase& clip);





    static juce::Array<WarpMarkerInfo> getMarkers (const te::AudioClipBase& clip);


    static int insertMarkerAtSourceTime (te::AudioClipBase& clip, double sourceTimeSeconds, juce::UndoManager* undoManager = nullptr);


    static void moveMarker (te::AudioClipBase& clip, int index, double warpTimeSeconds, juce::UndoManager* undoManager = nullptr);


    static void removeMarker (te::AudioClipBase& clip, int index, juce::UndoManager* undoManager = nullptr);


    static void resetMarkerWarpTime (te::AudioClipBase& clip, int index, juce::UndoManager* undoManager = nullptr);





    static std::pair<bool, juce::Array<double>> getTransientTimesSeconds (const te::AudioClipBase& clip);


    static void addMarkersAtTransients (te::AudioClipBase& clip, juce::UndoManager* undoManager = nullptr);





    static bool isEndpointMarker (int index, int markerCount);


    static bool canRemoveMarker (int index, int markerCount);





    static double snapWarpTimeToTransient (const te::AudioClipBase& clip,


                                           double warpTimeSeconds,


                                           bool enableSnap,


                                           double maxDistanceSeconds = 0.02);


    /** Maps a source-file time (seconds) to warped linear time (seconds). */
    static double sourceTimeToWarpTimeSeconds (const te::AudioClipBase& clip, double sourceTimeSeconds);





    /** Maps a marker warp time to a 0–1 fraction across the warped region (for timeline overlay). */


    static double warpTimeToClipLocalFraction (const te::AudioClipBase& clip, double warpTimeSeconds);





private:


    static te::WarpTimeManager& getManager (te::AudioClipBase& clip);


    static const te::WarpTimeManager& getManager (const te::AudioClipBase& clip);


    static void beginTransaction (juce::UndoManager* undoManager, const juce::String& name);


};





} // namespace skeletonhive



