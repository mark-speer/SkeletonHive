cmake_minimum_required(VERSION 3.22)

project(SkeletonHive VERSION 0.1.0 LANGUAGES C CXX)

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
set(SKELETONHIVE_ASIO_SDK_PATH "" CACHE PATH "Path to Steinberg ASIO SDK root (folder containing common/iasiodrv.h)")

# Fetch Tracktion Engine (includes JUCE as submodule when using git submodule)
set(TE_ADD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_CPM_DEVELOP ON CACHE BOOL "" FORCE)

# --- Tracktion Engine patch policy ---------------------------------------------
# SkeletonHive carries checked-in patches against Tracktion Engine (see
# cmake/patches/). They are applied by cmake/apply_tracktion_patches.cmake, which
# is idempotent (safe on re-configure) and FAILS THE CONFIGURE LOUDLY if a patch
# no longer applies - that only happens when the pinned commit below moves and the
# patched code changed upstream. Never hand-edit files under build/_deps: those
# edits are silently lost whenever FetchContent re-populates the tree.
#
# The EditSnapshot patch fixes an ACCESS_VIOLATION: EditSnapshot::refresh() called
# ProjectItem::getLength() without a null check, but file-based hosts (SkeletonHive)
# often have no TE ProjectItem for the edit file, so autosave via EditFileOperations
# crashed.
set(SKELETONHIVE_TE_EDITSNAPSHOT_REL
    "modules/tracktion_engine/model/edit/tracktion_EditSnapshot.cpp")
set(SKELETONHIVE_TE_EDITSNAPSHOT_PATCH
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/tracktion_EditSnapshot_null_ProjectItem.patch")
set(SKELETONHIVE_TE_EDITSNAPSHOT_MARKER "pi != nullptr ? pi->getLength()")
set(SKELETONHIVE_APPLY_TE_PATCHES
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_tracktion_patches.cmake")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/tracktion_engine/CMakeLists.txt")
    add_subdirectory(external/tracktion_engine)
    set(SKELETONHIVE_TRACKTION_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/tracktion_engine")

    # FetchContent's PATCH_COMMAND is not available for the vendored escape hatch,
    # so apply the same patch here (idempotent + fail-loud) against the local tree.
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DPATCH_FILE=${SKELETONHIVE_TE_EDITSNAPSHOT_PATCH}"
            "-DTARGET_FILE=${SKELETONHIVE_TE_EDITSNAPSHOT_REL}"
            "-DPATCH_MARKER=${SKELETONHIVE_TE_EDITSNAPSHOT_MARKER}"
            "-DTE_SRC_DIR=${SKELETONHIVE_TRACKTION_SRC_DIR}"
            -P "${SKELETONHIVE_APPLY_TE_PATCHES}"
        RESULT_VARIABLE _sh_te_patch_rv)
    if(NOT _sh_te_patch_rv EQUAL 0)
        message(FATAL_ERROR "SkeletonHive: failed to patch vendored Tracktion Engine (see above).")
    endif()
    unset(_sh_te_patch_rv)
else()
    include(FetchContent)
    # Skip JUCE submodule (SSH); Tracktion fetches JUCE via CPM when JUCE_CPM_DEVELOP is ON.
    # GIT_TAG is pinned to a specific commit so the checked-in patches have a stable
    # target; a floating branch could move the patched code and make the patch a
    # silent no-op. GIT_SHALLOW must be OFF to reliably fetch a pinned commit SHA.
    FetchContent_Declare(
        tracktion_engine
        GIT_REPOSITORY https://github.com/Tracktion/tracktion_engine.git
        GIT_TAG        53a32a4dcc2d73186b6cee4600b5dd51c65a0cae
        GIT_SHALLOW    FALSE
        GIT_SUBMODULES ""
        PATCH_COMMAND  "${CMAKE_COMMAND}"
            "-DPATCH_FILE=${SKELETONHIVE_TE_EDITSNAPSHOT_PATCH}"
            "-DTARGET_FILE=${SKELETONHIVE_TE_EDITSNAPSHOT_REL}"
            "-DPATCH_MARKER=${SKELETONHIVE_TE_EDITSNAPSHOT_MARKER}"
            -P "${SKELETONHIVE_APPLY_TE_PATCHES}"
    )
    FetchContent_MakeAvailable(tracktion_engine)
    FetchContent_GetProperties(tracktion_engine)
    set(SKELETONHIVE_TRACKTION_SRC_DIR "${tracktion_engine_SOURCE_DIR}")
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

# Neural Amp Modeler Core (MIT) + Eigen (header-only; pin to Core's submodule commit)
FetchContent_Declare(
    eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG        87300c93cae6a8afd9a4f8aa8d9d5c5324cf02e1
)
FetchContent_GetProperties(eigen)
if(NOT eigen_POPULATED)
    FetchContent_Populate(eigen)
endif()

FetchContent_Declare(
    nam_core_src
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG        v0.5.2
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES ""
)
FetchContent_GetProperties(nam_core_src)
if(NOT nam_core_src_POPULATED)
    FetchContent_Populate(nam_core_src)
endif()

# Always refresh overlays so NAM patches are not stuck on a stale FetchContent tree.
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/external/nam/CMakeLists.txt"
     DESTINATION "${nam_core_src_SOURCE_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/external/nam/nam_ensure_parsers.cpp"
     DESTINATION "${nam_core_src_SOURCE_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/external/nam/model_config.h"
     DESTINATION "${nam_core_src_SOURCE_DIR}/NAM")
set(EIGEN_INCLUDE_DIR "${eigen_SOURCE_DIR}" CACHE PATH "" FORCE)
set(SKELETONHIVE_NAM_ENSURE_PARSERS_SRC "${nam_core_src_SOURCE_DIR}/nam_ensure_parsers.cpp" CACHE FILEPATH "" FORCE)

# Ensure parsers are registered inside get_dsp() (same singleton as parse).
set(_nam_get_dsp_cpp "${nam_core_src_SOURCE_DIR}/NAM/get_dsp.cpp")
file(READ "${_nam_get_dsp_cpp}" _nam_get_dsp_src)
if(NOT _nam_get_dsp_src MATCHES "nam_ensure_builtin_parsers_registered")
    set(_nam_get_dsp_patched FALSE)
    foreach(_nl "\r\n" "\n")
        set(_old "std::unique_ptr<DSP> get_dsp(dspData& conf)${_nl}{")
        set(_new "extern \"C\" void nam_ensure_builtin_parsers_registered();${_nl}${_nl}std::unique_ptr<DSP> get_dsp(dspData& conf)${_nl}{${_nl}  nam_ensure_builtin_parsers_registered();")
        string(FIND "${_nam_get_dsp_src}" "${_old}" _nam_find_pos)
        if(NOT _nam_find_pos EQUAL -1)
            string(REPLACE "${_old}" "${_new}" _nam_get_dsp_src "${_nam_get_dsp_src}")
            set(_nam_get_dsp_patched TRUE)
            break()
        endif()
    endforeach()
    if(NOT _nam_get_dsp_patched)
        message(FATAL_ERROR "nam_core: failed to patch get_dsp(dspData&) for parser registration")
    endif()
    file(WRITE "${_nam_get_dsp_cpp}" "${_nam_get_dsp_src}")
endif()
unset(_nam_get_dsp_src)
unset(_nam_get_dsp_cpp)
unset(_nam_get_dsp_patched)

if(NOT TARGET nam_core)
    add_subdirectory(${nam_core_src_SOURCE_DIR} ${nam_core_src_BINARY_DIR} EXCLUDE_FROM_ALL)
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
    Source/Engine/Effects/NamPlugin.h
    Source/Engine/Effects/NamPlugin.cpp
    Source/Engine/Effects/NamCoreBootstrap.h
    Source/Engine/Effects/NamCoreBootstrap.cpp
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
    Source/Engine/TrackMonitorRouting.h
    Source/Engine/TrackMonitorRouting.cpp
    Source/Engine/TrackOutputRouting.h
    Source/Engine/TrackOutputRouting.cpp
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
    Source/UI/Arrangement/TrackHeaderControls.h
    Source/UI/Arrangement/TrackHeaderControls.cpp
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
    Source/UI/Effects/NamEditor.h
    Source/UI/Effects/NamEditor.cpp
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
    Source/UI/Plugins/SandboxEmbeddedEditor.h
    Source/UI/Plugins/SandboxEmbeddedEditor.cpp
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
    EIGEN_MAX_ALIGN_BYTES=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:SkeletonHive,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
)

if(WIN32)
    target_compile_definitions(SkeletonHive PRIVATE JUCE_ASIO=1)

    if(SKELETONHIVE_ASIO_SDK_PATH)
        set(asioSdkCommon "${SKELETONHIVE_ASIO_SDK_PATH}/common")

        if(EXISTS "${asioSdkCommon}/iasiodrv.h")
            message(STATUS "SkeletonHive: using external ASIO SDK at ${SKELETONHIVE_ASIO_SDK_PATH}")
            target_compile_definitions(SkeletonHive PRIVATE JUCE_ASIO_USE_EXTERNAL_SDK=1)
            target_include_directories(SkeletonHive PRIVATE "${asioSdkCommon}")
        else()
            message(WARNING "SkeletonHive: SKELETONHIVE_ASIO_SDK_PATH is set but ${asioSdkCommon}/iasiodrv.h was not found; using JUCE bundled ASIO headers.")
        endif()
    else()
        message(STATUS "SkeletonHive: ASIO enabled (using JUCE bundled ASIO headers)")
    endif()
endif()

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

# Pull every nam_core object so WaveNet/LSTM/etc. static registrars are not stripped.
if(MSVC)
    target_link_options(SkeletonHive PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:nam_core>")
    target_link_libraries(SkeletonHive PRIVATE nam_core)
elseif(APPLE)
    target_link_libraries(SkeletonHive PRIVATE
        "-Wl,-force_load"
        nam_core
    )
else()
    target_link_libraries(SkeletonHive PRIVATE
        "-Wl,--whole-archive"
        nam_core
        "-Wl,--no-whole-archive"
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(SkeletonHive PRIVATE "-latomic")
endif()

# Standalone NAM load smoke test (cmake --build --target nam_smoke_test)
add_executable(nam_smoke_test "${CMAKE_CURRENT_SOURCE_DIR}/external/nam/nam_smoke_test.cpp")
target_link_libraries(nam_smoke_test PRIVATE nam_core)
target_compile_features(nam_smoke_test PRIVATE cxx_std_20)
if(MSVC)
    target_link_options(nam_smoke_test PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:nam_core>")
elseif(APPLE)
    target_link_libraries(nam_smoke_test PRIVATE "-Wl,-force_load" nam_core)
else()
    target_link_libraries(nam_smoke_test PRIVATE "-Wl,--whole-archive" nam_core "-Wl,--no-whole-archive")
endif()
if(WIN32)
    target_compile_definitions(nam_smoke_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(SkeletonHive PRIVATE -Wall -Wextra)
elseif(MSVC)
    target_compile_options(SkeletonHive PRIVATE /W4)
endif()

target_include_directories(SkeletonHive PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)
