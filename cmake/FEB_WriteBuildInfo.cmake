# FEB_WriteBuildInfo.cmake
# ===========================================================================
# Build-time helper invoked via `cmake -P` from the add_custom_command that
# regenerates <target>/feb_build_info.c whenever any VERSION file or the
# repo HEAD changes. Expects every variable normally passed to
# configure_file() by feb_apply_version() to be forwarded on the command
# line via -D (FEB_VERSION_IN / FEB_VERSION_OUT / the FEB_* fields).
#
# Keeping substitution here instead of at configure time is what makes the
# embedded commit hash stay fresh across `git pull` without a reconfigure.
# ===========================================================================

if(NOT DEFINED FEB_VERSION_IN OR NOT DEFINED FEB_VERSION_OUT)
    message(FATAL_ERROR "FEB_WriteBuildInfo: FEB_VERSION_IN and FEB_VERSION_OUT are required")
endif()

configure_file("${FEB_VERSION_IN}" "${FEB_VERSION_OUT}" @ONLY)
