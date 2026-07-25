# Idempotent, fail-loud application of SkeletonHive's Tracktion Engine patches.
#
# Invoked two ways:
#   1. As a FetchContent PATCH_COMMAND, with the working directory set to the
#      populated Tracktion source tree.
#   2. Directly via execute_process() for the external/tracktion_engine escape
#      hatch, in which case TE_SRC_DIR is passed explicitly.
#
# Required -D arguments:
#   PATCH_FILE   Absolute path to the .patch to apply.
#   TARGET_FILE  Patched file, relative to the Tracktion source root. Used to
#                detect an already-patched tree so re-configures are safe.
#   PATCH_MARKER Literal substring that is present only after the patch is
#                applied. Matched literally (not as a regex) so it survives
#                escaping through generator custom commands.
# Optional:
#   TE_SRC_DIR   Tracktion source root. Defaults to the current working dir
#                (correct for the PATCH_COMMAND case).

if(NOT DEFINED TE_SRC_DIR OR TE_SRC_DIR STREQUAL "")
    set(TE_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}")
endif()

set(_target_abs "${TE_SRC_DIR}/${TARGET_FILE}")

if(NOT EXISTS "${_target_abs}")
    message(FATAL_ERROR
        "SkeletonHive: cannot patch Tracktion Engine - target file missing:\n"
        "  ${_target_abs}\n"
        "The pinned Tracktion commit may have moved or restructured this file. "
        "Update cmake/patches and the GIT_TAG pin in CMakeLists.txt.")
endif()

file(READ "${_target_abs}" _target_src)
string(FIND "${_target_src}" "${PATCH_MARKER}" _marker_pos)
if(NOT _marker_pos EQUAL -1)
    message(STATUS "SkeletonHive: Tracktion patch already present in ${TARGET_FILE}")
    return()
endif()

find_program(GIT_EXECUTABLE git)
if(NOT GIT_EXECUTABLE)
    message(FATAL_ERROR "SkeletonHive: git not found; cannot apply Tracktion patch ${PATCH_FILE}")
endif()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --ignore-whitespace --whitespace=nowarn "${PATCH_FILE}"
    WORKING_DIRECTORY "${TE_SRC_DIR}"
    RESULT_VARIABLE _apply_rv
    ERROR_VARIABLE _apply_err
    OUTPUT_VARIABLE _apply_out)

if(NOT _apply_rv EQUAL 0)
    message(FATAL_ERROR
        "SkeletonHive: FAILED to apply Tracktion patch.\n"
        "  patch : ${PATCH_FILE}\n"
        "  tree  : ${TE_SRC_DIR}\n"
        "  git   : ${_apply_err}${_apply_out}\n"
        "This usually means the pinned Tracktion commit changed the patched code. "
        "Regenerate the patch against the pinned commit, or update the GIT_TAG pin.")
endif()

message(STATUS "SkeletonHive: applied Tracktion patch ${TARGET_FILE}")
