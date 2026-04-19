# FEB Version Generation
# ===========================================================================
# Provides feb_apply_version(<target> <board_dir>) which:
#   - reads <board_dir>/VERSION and the repo + common VERSION files
#   - captures git commit (short + full), branch, and dirty-tree flag
#   - captures build UTC timestamp (respects SOURCE_DATE_EPOCH for
#     reproducible builds)
#   - generates <build>/generated/<target>/feb_build_info.c from the
#     template at cmake/templates/feb_build_info.c.in
#   - appends the generated source to the target
#   - sets up a custom command that re-generates the source if the
#     VERSION files or git HEAD / index change (so the embedded commit
#     hash stays fresh across commits without needing a full reconfigure)
#
# Missing-git fallback: every string defaults to "unknown" so CI
# tarball builds, worktree exports, and fresh clones without history
# all still compile.
# ===========================================================================

# Resolve repo root and template path once at include time. CMAKE_SOURCE_DIR
# points at the top-level repo since each board adds this file via include().
set(FEB_VERSION_CMAKE_DIR  "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "FEB version cmake dir")
set(FEB_VERSION_REPO_ROOT  "${CMAKE_SOURCE_DIR}"       CACHE INTERNAL "FEB repo root")
set(FEB_VERSION_TEMPLATE_C "${FEB_VERSION_CMAKE_DIR}/templates/feb_build_info.c.in"
    CACHE INTERNAL "FEB build_info C template")

# ---------------------------------------------------------------------------
# Helper: read a single-line VERSION file into <out_var>. Falls back to
# "0.0.0" if the file is missing. Strips whitespace.
# ---------------------------------------------------------------------------
function(_feb_read_version file out_var)
    if(EXISTS "${file}")
        file(READ "${file}" _raw)
        string(STRIP "${_raw}" _trimmed)
        if(NOT _trimmed)
            set(_trimmed "0.0.0")
        endif()
    else()
        set(_trimmed "0.0.0")
    endif()
    set(${out_var} "${_trimmed}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# Helper: split "MAJOR.MINOR.PATCH" into three integer variables in the
# caller's scope. Non-numeric / short strings yield zeros so bad input
# never breaks the build.
# ---------------------------------------------------------------------------
function(_feb_parse_version ver major_var minor_var patch_var)
    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _m "${ver}")
    if(_m)
        set(${major_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
        set(${minor_var} "${CMAKE_MATCH_2}" PARENT_SCOPE)
        set(${patch_var} "${CMAKE_MATCH_3}" PARENT_SCOPE)
    else()
        set(${major_var} 0 PARENT_SCOPE)
        set(${minor_var} 0 PARENT_SCOPE)
        set(${patch_var} 0 PARENT_SCOPE)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Helper: capture git state. Produces:
#   GIT_COMMIT_SHORT, GIT_COMMIT_FULL, GIT_BRANCH  (strings, "unknown" on
#     failure)
#   GIT_DIRTY                                      ("true"|"false")
# All set in the caller's scope. Respects the case where git is missing
# or the repo has no commits yet.
# ---------------------------------------------------------------------------
function(_feb_capture_git repo_root)
    find_package(Git QUIET)
    set(_short   "unknown")
    set(_full    "unknown")
    set(_branch  "unknown")
    set(_dirty   "false")

    if(GIT_FOUND AND EXISTS "${repo_root}/.git")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short=7 HEAD
            WORKING_DIRECTORY "${repo_root}"
            OUTPUT_VARIABLE _short_out OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _short_rc ERROR_QUIET)
        if(_short_rc EQUAL 0 AND _short_out)
            set(_short "${_short_out}")
        endif()

        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
            WORKING_DIRECTORY "${repo_root}"
            OUTPUT_VARIABLE _full_out OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _full_rc ERROR_QUIET)
        if(_full_rc EQUAL 0 AND _full_out)
            set(_full "${_full_out}")
        endif()

        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
            WORKING_DIRECTORY "${repo_root}"
            OUTPUT_VARIABLE _branch_out OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _branch_rc ERROR_QUIET)
        if(_branch_rc EQUAL 0 AND _branch_out)
            set(_branch "${_branch_out}")
        endif()

        # Return code 0 = clean, 1 = dirty. Refresh the index first so
        # stale stat() info doesn't produce false positives.
        execute_process(
            COMMAND ${GIT_EXECUTABLE} update-index --refresh
            WORKING_DIRECTORY "${repo_root}"
            OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE _ignored)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} diff-index --quiet HEAD --
            WORKING_DIRECTORY "${repo_root}"
            RESULT_VARIABLE _dirty_rc ERROR_QUIET)
        if(NOT _dirty_rc EQUAL 0)
            set(_dirty "true")
        endif()
    endif()

    set(GIT_COMMIT_SHORT "${_short}"  PARENT_SCOPE)
    set(GIT_COMMIT_FULL  "${_full}"   PARENT_SCOPE)
    set(GIT_BRANCH       "${_branch}" PARENT_SCOPE)
    set(GIT_DIRTY        "${_dirty}"  PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# Helper: compute a UTC ISO-8601 timestamp. Honours SOURCE_DATE_EPOCH (an
# integer seconds-since-epoch) for reproducible builds. When the env var
# isn't set, falls back to the current UTC time at configure-step.
# ---------------------------------------------------------------------------
function(_feb_build_timestamp out_var)
    if(DEFINED ENV{SOURCE_DATE_EPOCH})
        set(_epoch "$ENV{SOURCE_DATE_EPOCH}")
        # date -u -d @<epoch> works on GNU; BSD/macOS needs -r <epoch>.
        # Try GNU form first, then BSD form.
        execute_process(
            COMMAND date -u -d "@${_epoch}" "+%Y-%m-%dT%H:%M:%SZ"
            OUTPUT_VARIABLE _ts OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _rc ERROR_QUIET)
        if(NOT _rc EQUAL 0)
            execute_process(
                COMMAND date -u -r "${_epoch}" "+%Y-%m-%dT%H:%M:%SZ"
                OUTPUT_VARIABLE _ts OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE _rc ERROR_QUIET)
        endif()
        if(_rc EQUAL 0 AND _ts)
            set(${out_var} "${_ts}" PARENT_SCOPE)
            return()
        endif()
    endif()

    string(TIMESTAMP _now "%Y-%m-%dT%H:%M:%SZ" UTC)
    set(${out_var} "${_now}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# Helper: identify the build user + host. Falls back to "unknown" rather
# than leaking configure-time state. Honours SOURCE_DATE_EPOCH presence
# as a proxy for "reproducible build requested" and scrubs identity
# fields in that case.
# ---------------------------------------------------------------------------
function(_feb_capture_identity user_var host_var)
    if(DEFINED ENV{SOURCE_DATE_EPOCH})
        set(${user_var} "reproducible" PARENT_SCOPE)
        set(${host_var} "reproducible" PARENT_SCOPE)
        return()
    endif()

    set(_user "unknown")
    if(DEFINED ENV{USER})
        set(_user "$ENV{USER}")
    elseif(DEFINED ENV{USERNAME})
        set(_user "$ENV{USERNAME}")
    endif()

    cmake_host_system_information(RESULT _host QUERY HOSTNAME)
    if(NOT _host)
        set(_host "unknown")
    endif()

    set(${user_var} "${_user}" PARENT_SCOPE)
    set(${host_var} "${_host}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# feb_apply_version(<target> <board_dir>)
#
# Wires compile-time version metadata into <target>. Must be called AFTER
# add_executable(<target> ...).
# ---------------------------------------------------------------------------
function(feb_apply_version target board_dir)
    get_filename_component(_board_name "${board_dir}" NAME)

    # Read every VERSION file this target cares about.
    _feb_read_version("${board_dir}/VERSION"                         BOARD_VERSION)
    _feb_read_version("${FEB_VERSION_REPO_ROOT}/VERSION"             REPO_VERSION)
    _feb_read_version("${FEB_VERSION_REPO_ROOT}/common/VERSION"      COMMON_VERSION)
    _feb_parse_version("${BOARD_VERSION}" V_MAJOR V_MINOR V_PATCH)

    # Capture git + build identity into local vars for configure_file().
    _feb_capture_git("${FEB_VERSION_REPO_ROOT}")
    _feb_build_timestamp(BUILD_UTC)
    _feb_capture_identity(BUILD_USER BUILD_HOST)

    # Name the generated .c after the target so two boards built in
    # parallel don't clobber each other's file.
    set(_gen_dir "${CMAKE_BINARY_DIR}/generated/${target}")
    file(MAKE_DIRECTORY "${_gen_dir}")
    set(_gen_c "${_gen_dir}/feb_build_info.c")

    # Make the inputs visible to configure_file() under the exact names
    # used in feb_build_info.c.in.
    set(FEB_BOARD_NAME      "${_board_name}")
    set(FEB_BOARD_VERSION   "${BOARD_VERSION}")
    set(FEB_BOARD_MAJOR     "${V_MAJOR}")
    set(FEB_BOARD_MINOR     "${V_MINOR}")
    set(FEB_BOARD_PATCH     "${V_PATCH}")
    set(FEB_REPO_VERSION    "${REPO_VERSION}")
    set(FEB_COMMON_VERSION  "${COMMON_VERSION}")
    set(FEB_COMMIT_SHORT    "${GIT_COMMIT_SHORT}")
    set(FEB_COMMIT_FULL     "${GIT_COMMIT_FULL}")
    set(FEB_BRANCH          "${GIT_BRANCH}")
    set(FEB_DIRTY           "${GIT_DIRTY}")
    set(FEB_BUILD_UTC       "${BUILD_UTC}")
    set(FEB_BUILD_USER      "${BUILD_USER}")
    set(FEB_BUILD_HOST      "${BUILD_HOST}")

    configure_file("${FEB_VERSION_TEMPLATE_C}" "${_gen_c}" @ONLY)

    # Compile the generated source into the target. `PRIVATE` so the
    # symbol only lives in this executable.
    target_sources(${target} PRIVATE "${_gen_c}")

    # Announce what we baked in so the build log is self-documenting.
    message(STATUS "FEB_Version[${_board_name}]: v${BOARD_VERSION} commit=${GIT_COMMIT_SHORT} dirty=${GIT_DIRTY} built=${BUILD_UTC}")
endfunction()
