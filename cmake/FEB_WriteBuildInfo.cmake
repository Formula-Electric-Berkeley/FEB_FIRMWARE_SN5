foreach(_required FEB_VERSION_IN FEB_VERSION_OUT FEB_REPO_ROOT FEB_BOARD_NAME)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "FEB_WriteBuildInfo: ${_required} is required")
    endif()
endforeach()

set(FEB_COMMIT_SHORT "unknown")
set(FEB_COMMIT_FULL  "unknown")
set(FEB_BRANCH       "unknown")
set(FEB_DIRTY        "false")
set(FEB_VERSION      "unknown")
set(FEB_VERSION_YEAR  0)
set(FEB_VERSION_MONTH 0)
set(FEB_VERSION_DAY   0)

find_package(Git QUIET)

function(_feb_git out_var)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} ${ARGN}
        WORKING_DIRECTORY "${FEB_REPO_ROOT}"
        OUTPUT_VARIABLE _out OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _rc ERROR_QUIET)
    if(_rc EQUAL 0)
        set(${out_var} "${_out}" PARENT_SCOPE)
    else()
        set(${out_var} "" PARENT_SCOPE)
    endif()
endfunction()

if(GIT_FOUND AND EXISTS "${FEB_REPO_ROOT}/.git")
    _feb_git(_short rev-parse --short=7 HEAD)
    _feb_git(_full rev-parse HEAD)
    _feb_git(_branch rev-parse --abbrev-ref HEAD)
    _feb_git(_commit_epoch show -s --format=%ct HEAD)

    if(_short)
        set(FEB_COMMIT_SHORT "${_short}")
    endif()
    if(_full)
        set(FEB_COMMIT_FULL "${_full}")
    endif()
    if(_branch)
        set(FEB_BRANCH "${_branch}")
    endif()

    _feb_git(_ignored update-index -q --refresh)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} diff-index --quiet HEAD --
        WORKING_DIRECTORY "${FEB_REPO_ROOT}"
        RESULT_VARIABLE _dirty_rc ERROR_QUIET)
    if(NOT _dirty_rc EQUAL 0)
        set(FEB_DIRTY "true")
    endif()

    if(_commit_epoch AND _short)
        set(_saved_sde "$ENV{SOURCE_DATE_EPOCH}")
        set(ENV{SOURCE_DATE_EPOCH} "${_commit_epoch}")
        string(TIMESTAMP _date "%Y.%m.%d" UTC)
        string(TIMESTAMP _y "%Y" UTC)
        string(TIMESTAMP _m "%m" UTC)
        string(TIMESTAMP _d "%d" UTC)
        set(ENV{SOURCE_DATE_EPOCH} "${_saved_sde}")

        set(FEB_VERSION "${_date}+${_short}")
        if(FEB_DIRTY)
            string(APPEND FEB_VERSION "-dirty")
        endif()
        math(EXPR FEB_VERSION_YEAR  "${_y}")
        math(EXPR FEB_VERSION_MONTH "${_m}")
        math(EXPR FEB_VERSION_DAY   "${_d}")
    endif()
endif()

configure_file("${FEB_VERSION_IN}" "${FEB_VERSION_OUT}" @ONLY)

if(FEB_VERSION_ANNOUNCE)
    message(STATUS "FEB_Version[${FEB_BOARD_NAME}]: ${FEB_VERSION} (${FEB_BRANCH}) built=${FEB_BUILD_UTC}")
endif()
