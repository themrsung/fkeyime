# src/version.h: version, commit and issues URL, for the settings window and the TIP.
execute_process(COMMAND git rev-parse --short HEAD WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                OUTPUT_VARIABLE FKEYIME_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT FKEYIME_COMMIT)
    set(FKEYIME_COMMIT unknown)
endif()
configure_file(version.h.in ${PROJECT_BINARY_DIR}/generated/version.h @ONLY)

add_subdirectory(core)
if(WIN32)
    add_subdirectory(tip)
    add_subdirectory(settings)
endif()
