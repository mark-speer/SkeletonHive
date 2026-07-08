cmake_minimum_required(VERSION 3.22)

project(SkeletonHive VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE INTERNAL "")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS YES)
option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Enable Module Source Groups" ON)

# Fetch Tracktion Engine (includes JUCE as submodule when using git submodule)
set(TE_ADD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_CPM_DEVELOP ON CACHE BOOL "" FORCE)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/tracktion_engine/CMakeLists.txt")
    add_subdirectory(external/tracktion_engine)
else()
    include(FetchContent)
    # Skip JUCE submodule (SSH); Tracktion fetches JUCE via CPM when JUCE_CPM_DEVELOP is ON
    FetchContent_Declare(
        tracktion_engine
        GIT_REPOSITORY https://github.com/Tracktion/tracktion_engine.git
        GIT_TAG        develop
        GIT_SHALLOW    TRUE
        GIT_SUBMODULES ""
    )
    FetchContent_MakeAvailable(tracktion_engine)
endif()

include(FetchContent)
FetchContent_Declare(
    aubio
    GIT_REPOSITORY https://github.com/aubio/aubio.git
    GIT_TAG        0.4.9
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(aubio)
if(NOT aubio_POPULATED)
    FetchContent_Populate(aubio)
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/external/aubio/CMakeLists.txt"
         DESTINATION "${aubio_SOURCE_DIR}")
    add_subdirectory(${aubio_SOURCE_DIR} ${aubio_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

juce_add_gui_app(SkeletonHive
    PRODUCT_NAME "SkeletonHive"
    COMPANY_NAME "SkeletonHive"
    BUNDLE_ID "com.skeletonhive.app"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/SkeletonHive.png"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Audio recording requires microphone access"
)

juce_generate_juce_header(SkeletonHive)

target_sources(SkeletonHive PRIVATE
    Source/App/Main.cpp
    Source/App/SkeletonHiveApplication.h
    Source/App/MainWindow.h
    Source/App/MainWindow.cpp
    Source/Engine/ProjectManager.h
    Source/Engine/ProjectManager.cpp
    Source/Engine/TransportController.h
    Source/Engine/TransportController.cpp
    Source/Engine/PluginScanner.h
    Source/Engine/PluginScanner.cpp
    Source/Engine/PluginHost/PluginHostConstants.h
    Source/Engine/PluginHost/PluginHostProtocol.h
    Source/Engine/PluginHost/PluginHostProtocol.cpp
    Source/Engine/PluginHost/PluginHostSharedMemory.h
    Source/Engine/PluginHost/PluginHostSharedMemory.cpp
    Source/Engine/PluginHost/PluginHostHelpers.h
    Source/Engine/PluginHost/PluginHostHelpers.cpp
    Source/Engine/PluginHost/PluginHostCoordinator.h
    Source/Engine/PluginHost/PluginHostCoordinator.cpp
    Source/Engine/PluginHost/SandboxedPluginInstance.h
    Source/Engine/PluginHost/SandboxedPluginInstance.cpp
    Source/Engine/PluginHost/PluginHostWorker.h
    Source/Engine/PluginHost/PluginHostWorker.cpp
    Source/Engine/PluginHost/PluginHostManager.h
    Source/Engine/PluginHost/PluginHostManager.cpp
    Source/Engine/EngineBenchmarkHarness.h
    Source/Engine/EngineBenchmarkHarness.cpp
    Source/Engine/NativePluginCatalog.h
    Source/Engine/NativePluginCatalog.cpp
    Source/Engine/DrumRackHelpers.h
    Source/Engine/DrumRackHelpers.cpp
    Source/Engine/SamplerHelpers.h
    Source/Engine/SamplerHelpers.cpp
    Source/Engine/SynthHelpers.h
    Source/Engine/SynthHelpers.cpp
    Source/Engine/WarpEngine.h
    Source/Engine/WarpEngine.cpp
    Source/Engine/AudioToMidiTypes.h
    Source/Engine/AudioSampleExtractor.h
    Source/Engine/AudioSampleExtractor.cpp
    Source/Engine/AudioToMidiAnalysis.h
    Source/Engine/AudioToMidiAnalysis.cpp
    Source/Engine/AudioToMidiHarmony.h
    Source/Engine/AudioToMidiHarmony.cpp
    Source/Engine/AudioToMidiEngine.h
    Source/Engine/AudioToMidiEngine.cpp
    Source/Engine/Effects/NativeCustomPlugins.h
    Source/Engine/Effects/NativeCustomPlugins.cpp
    Source/Engine/Effects/SaturationPlugin.h
    Source/Engine/Effects/SaturationPlugin.cpp
    Source/Engine/Effects/MultibandDynamicsPlugin.h
    Source/Engine/Effects/MultibandDynamicsPlugin.cpp
    Source/Engine/Effects/EqualiserHelpers.h
    Source/Engine/Effects/EqualiserHelpers.cpp
    Source/Engine/Effects/CompressorHelpers.h
    Source/Engine/Effects/CompressorHelpers.cpp
    Source/Engine/ExtendedUIBehaviour.h
    Source/Engine/ExtendedEngineBehaviour.h
    Source/Engine/EngineHelpers.h
    Source/Engine/EngineHelpers.cpp
    Source/Engine/MidiScale.h
    Source/Engine/MidiScale.cpp
    Source/Engine/SessionClipPlaybackResolver.h
    Source/Engine/SessionClipPlaybackResolver.cpp
    Source/Engine/AppSettings.h
    Source/Engine/AppSettings.cpp
    Source/Engine/AppCommands.h
    Source/Engine/AppCommands.cpp
    Source/Engine/MidiLearnController.h
    Source/Engine/MidiLearnController.cpp
    Source/Engine/ControlSurfaceManager.h
    Source/Engine/ControlSurfaceManager.cpp
    Source/Engine/ExportManager.h
    Source/Engine/ExportManager.cpp
    Source/Engine/MultiOutputRouting.h
    Source/Engine/MultiOutputRouting.cpp
    Source/Engine/SidechainRouting.h
    Source/Engine/SidechainRouting.cpp
    Source/Engine/TrackInputRouting.h
    Source/Engine/TrackInputRouting.cpp
    Source/Engine/WaveformCache.h
    Source/Engine/WaveformCache.cpp
    Source/Engine/PluginDragManager.h
    Source/Engine/PluginDragManager.cpp
    Source/Engine/TrackPluginChainModel.h
    Source/Engine/TrackPluginChainModel.cpp
    Source/Engine/PluginStateManager.h
    Source/Engine/PluginStateManager.cpp
    Source/Engine/PluginPresetManager.h
    Source/Engine/PluginPresetManager.cpp
    Source/Engine/ContentLibraryManager.h
    Source/Engine/ContentLibraryManager.cpp
    Source/Engine/ClipLibraryManager.h
    Source/Engine/ClipLibraryManager.cpp
    Source/Engine/GrooveTemplate.h
    Source/Engine/GrooveEngine.h
    Source/Engine/GrooveEngine.cpp
    Source/Engine/GroovePoolManager.h
    Source/Engine/GroovePoolManager.cpp
    Source/Engine/ContentDragManager.h
    Source/Engine/ContentDragManager.cpp
    Source/Engine/PreviewPlayer.h
    Source/Engine/PreviewPlayer.cpp
    Source/Engine/UiTelemetryHub.h
    Source/Engine/UiTelemetryHub.cpp
    Source/Engine/SessionManager.h
    Source/Engine/SessionManager.cpp
    Source/Engine/SessionMidiMapper.h
    Source/Engine/SessionMidiMapper.cpp
    Source/Engine/SessionArrangementBridge.h
    Source/Engine/SessionArrangementBridge.cpp
    Source/UI/Arrangement/EditViewState.h
    Source/UI/Arrangement/ClipComponents.h
    Source/UI/Arrangement/ClipComponents.cpp
    Source/UI/Arrangement/TrackComponents.h
    Source/UI/Arrangement/TrackComponents.cpp
    Source/UI/Arrangement/TimelineComponent.h
    Source/UI/Arrangement/TimelineComponent.cpp
    Source/UI/Arrangement/TimelineGrid.h
    Source/UI/Arrangement/TimelineGrid.cpp
    Source/UI/Arrangement/LaneBackgroundCache.h
    Source/UI/Arrangement/LaneBackgroundCache.cpp
    Source/UI/Arrangement/LaneClipSummaryPaint.h
    Source/UI/Arrangement/LaneClipSummaryPaint.cpp
    Source/UI/Arrangement/ArrangementSelectionHelpers.cpp
    Source/UI/Arrangement/ArrangementClipVisuals.cpp
    Source/UI/Arrangement/ClipInspectorPanel.h
    Source/UI/Arrangement/ClipInspectorPanel.cpp
    Source/UI/Arrangement/ClipWarpEditor.h
    Source/UI/Arrangement/ClipWarpEditor.cpp
    Source/UI/Detail/DetailPanelStack.h
    Source/UI/Detail/DetailPanelStack.cpp
    Source/UI/Arrangement/TakeLaneComponent.h
    Source/UI/Arrangement/TakeLaneComponent.cpp
    Source/UI/Arrangement/TimelineTypes.h
    Source/UI/Arrangement/TimelineOpenGLRenderer.h
    Source/UI/Arrangement/TimelineOpenGLRenderer.cpp
    Source/UI/AppLookAndFeel.h
    Source/UI/AppLookAndFeel.cpp
    Source/UI/Settings/PreferencesDialog.h
    Source/UI/Settings/PreferencesDialog.cpp
    Source/UI/Transport/TransportBar.h
    Source/UI/Transport/TransportBar.cpp
    Source/UI/Mixer/MixerPanel.h
    Source/UI/Mixer/MixerPanel.cpp
    Source/UI/Mixer/ChannelStrip.h
    Source/UI/Mixer/ChannelStrip.cpp
    Source/UI/Plugins/PluginBrowser.h
    Source/UI/Plugins/PluginBrowser.cpp
    Source/UI/Plugins/NativePluginBrowserTab.h
    Source/UI/Plugins/NativePluginBrowserTab.cpp
    Source/UI/Plugins/NativePluginEditor.h
    Source/UI/Plugins/NativePluginEditor.cpp
    Source/UI/Instruments/DrumPadComponent.h
    Source/UI/Instruments/DrumPadComponent.cpp
    Source/UI/Instruments/DrumRackEditor.h
    Source/UI/Instruments/DrumRackEditor.cpp
    Source/UI/Instruments/SamplerEditor.h
    Source/UI/Instruments/SamplerEditor.cpp
    Source/UI/Instruments/SamplerWaveformComponent.h
    Source/UI/Instruments/SamplerWaveformComponent.cpp
    Source/UI/Instruments/SynthEditor.h
    Source/UI/Instruments/SynthEditor.cpp
    Source/UI/Instruments/SynthModMatrixPanel.h
    Source/UI/Instruments/SynthModMatrixPanel.cpp
    Source/UI/Effects/EffectControlComponents.h
    Source/UI/Effects/EffectControlComponents.cpp
    Source/UI/Effects/EqualiserCurveComponent.h
    Source/UI/Effects/EqualiserCurveComponent.cpp
    Source/UI/Effects/EqualiserEditor.h
    Source/UI/Effects/EqualiserEditor.cpp
    Source/UI/Effects/CompressorEditor.h
    Source/UI/Effects/CompressorEditor.cpp
    Source/UI/Effects/DelayReverbEditor.h
    Source/UI/Effects/DelayReverbEditor.cpp
    Source/UI/Effects/SaturationEditor.h
    Source/UI/Effects/SaturationEditor.cpp
    Source/UI/Effects/MultibandDynamicsEditor.h
    Source/UI/Effects/MultibandDynamicsEditor.cpp
    Source/UI/Plugins/PluginPickerDialog.h
    Source/UI/Plugins/PluginPickerDialog.cpp
    Source/UI/Plugins/PresetBrowserPanel.h
    Source/UI/Plugins/PresetBrowserPanel.cpp
    Source/UI/Plugins/PluginSlotComponent.h
    Source/UI/Plugins/PluginSlotComponent.cpp
    Source/UI/Plugins/PluginTrayComponent.h
    Source/UI/Plugins/PluginTrayComponent.cpp
    Source/UI/Plugins/MultiOutputConfigDialog.h
    Source/UI/Plugins/MultiOutputConfigDialog.cpp
    Source/UI/Routing/SidechainMenu.h
    Source/UI/Routing/SidechainMenu.cpp
    Source/UI/Routing/SidechainMatrixPanel.h
    Source/UI/Routing/SidechainMatrixPanel.cpp
    Source/UI/Plugins/PluginWindow.h
    Source/UI/Plugins/PluginWindow.cpp
    Source/UI/Midi/PianoRollEditor.h
    Source/UI/Midi/PianoRollEditor.cpp
    Source/UI/Midi/MidiLaneViewport.h
    Source/UI/Midi/MidiControllerLaneComponent.h
    Source/UI/Midi/MidiControllerLaneComponent.cpp
    Source/UI/Midi/MidiLaneEditor.h
    Source/UI/Midi/MidiLaneEditor.cpp
    Source/UI/Automation/AutomationLaneComponent.h
    Source/UI/Automation/AutomationLaneComponent.cpp
    Source/UI/Automation/AutomationPanel.h
    Source/UI/Automation/AutomationPanel.cpp
    Source/UI/Browser/BrowserPanel.h
    Source/UI/Browser/BrowserPanel.cpp
    Source/UI/Browser/SampleBrowserTab.h
    Source/UI/Browser/SampleBrowserTab.cpp
    Source/UI/Browser/PlacesBrowserTab.h
    Source/UI/Browser/PlacesBrowserTab.cpp
    Source/UI/Browser/ClipsBrowserTab.h
    Source/UI/Browser/ClipsBrowserTab.cpp
    Source/UI/Browser/GroovesBrowserTab.h
    Source/UI/Browser/GroovesBrowserTab.cpp
    Source/UI/Session/ClipSlotComponent.h
    Source/UI/Session/ClipSlotComponent.cpp
    Source/UI/Session/SceneLaunchColumn.h
    Source/UI/Session/SceneLaunchColumn.cpp
    Source/UI/Session/SessionGridComponent.h
    Source/UI/Session/SessionGridComponent.cpp
    Source/UI/Session/SessionSlotTypes.h
    Source/UI/Session/SessionViewComponent.h
    Source/UI/Session/SessionViewComponent.cpp
    Source/UI/Session/PerformanceMacroPanel.h
    Source/UI/Session/PerformanceMacroPanel.cpp
)

target_compile_definitions(SkeletonHive PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_LADSPA=1
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:SkeletonHive,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
)

target_link_libraries(SkeletonHive PRIVATE
    aubio
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_opengl
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(SkeletonHive PRIVATE "-latomic")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(SkeletonHive PRIVATE -Wall -Wextra)
elseif(MSVC)
    target_compile_options(SkeletonHive PRIVATE /W4)
endif()

target_include_directories(SkeletonHive PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)
