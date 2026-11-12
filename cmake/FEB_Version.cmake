# Resolve repo root and template path once at include time. CMAKE_SOURCE_DIR
# points at the top-level repo since each board adds this file via include().
set(FEB_VERSION_CMAKE_DIR  "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "FEB version cmake dir")
set(FEB_VERSION_REPO_ROOT  "${CMAKE_SOURCE_DIR}"       CACHE INTERNAL "FEB repo root")
set(FEB_VERSION_TEMPLATE_C "${FEB_VERSION_CMAKE_DIR}/templates/feb_build_info.c.in"
    CACHE INTERNAL "FEB build_info C template")
set(FEB_VERSION_WRITER     "${FEB_VERSION_CMAKE_DIR}/FEB_WriteBuildInfo.cmake"
    CACHE INTERNAL "FEB build_info writer script")

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

    _feb_build_timestamp(BUILD_UTC)
    _feb_capture_identity(BUILD_USER BUILD_HOST)

    # Name the generated .c after the target so two boards built in
    # parallel don't clobber each other's file.
    set(_gen_dir "${CMAKE_BINARY_DIR}/generated/${target}")
    file(MAKE_DIRECTORY "${_gen_dir}")
    set(_gen_c "${_gen_dir}/feb_build_info.c")

    set(_writer_args
        -DFEB_VERSION_IN=${FEB_VERSION_TEMPLATE_C}
        -DFEB_VERSION_OUT=${_gen_c}
        -DFEB_REPO_ROOT=${FEB_VERSION_REPO_ROOT}
        -DFEB_BOARD_NAME=${_board_name}
        -DFEB_BUILD_UTC=${BUILD_UTC}
        -DFEB_BUILD_USER=${BUILD_USER}
        -DFEB_BUILD_HOST=${BUILD_HOST}
    )

    # Seed at configure time so the source exists before the first build.
    execute_process(
        COMMAND ${CMAKE_COMMAND} ${_writer_args} -DFEB_VERSION_ANNOUNCE=ON -P ${FEB_VERSION_WRITER}
        RESULT_VARIABLE _seed_rc)
    if(NOT _seed_rc EQUAL 0)
        message(FATAL_ERROR "FEB_Version[${_board_name}]: failed to generate ${_gen_c}")
    endif()

    set(_always "${_gen_dir}/feb_build_info.always")
    set_source_files_properties("${_always}" PROPERTIES SYMBOLIC TRUE)
    add_custom_command(
        OUTPUT "${_always}"
        BYPRODUCTS "${_gen_c}"
        COMMAND ${CMAKE_COMMAND} ${_writer_args} -P ${FEB_VERSION_WRITER}
        COMMENT "FEB_Version[${_board_name}]: checking git state"
        VERBATIM
    )

    # PRIVATE so the symbol only lives in this executable.
    target_sources(${target} PRIVATE "${_gen_c}" "${_always}")
endfunction()
