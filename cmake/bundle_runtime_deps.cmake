# Copies TARGET_FILE and its non-system runtime DLLs into DEST_DIR.
# Invoked as a POST_BUILD step; see CMakeLists.txt.
#
# Required variables (passed via -D):
#   TARGET_FILE  - path to the built executable
#   DEST_DIR     - directory to place the exe + DLLs in
#   SEARCH_DIRS  - semicolon-separated list of dirs to resolve bare DLL names against
#
# Optional variables:
#   SHARE_DIR    - toolchain's share/ dir (e.g. .../mingw64/share); if set,
#                  glib's compiled GSettings schemas are copied alongside the
#                  exe so GTK can find them at their DLL-relative default
#                  search path - without this, bundled GTK dialogs (file
#                  chooser, etc.) fail with "No GSettings schemas are
#                  installed on the system" once the exe is moved out of the
#                  toolchain's own bin/ directory.

if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

file(MAKE_DIRECTORY "${DEST_DIR}")
file(COPY "${TARGET_FILE}" DESTINATION "${DEST_DIR}")

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${TARGET_FILE}"
    RESOLVED_DEPENDENCIES_VAR resolved_deps
    UNRESOLVED_DEPENDENCIES_VAR unresolved_deps
    DIRECTORIES ${SEARCH_DIRS}
    PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
    # Anything that resolves into the Windows install (System32, SysWOW64,
    # WinSxS, ...) is an OS-provided DLL and must not be bundled.
    POST_EXCLUDE_REGEXES ".*[Ww][Ii][Nn][Dd][Oo][Ww][Ss].*"
)

foreach(dep IN LISTS resolved_deps)
    file(COPY "${dep}" DESTINATION "${DEST_DIR}")
endforeach()

if(unresolved_deps)
    message(WARNING "bundle_runtime_deps: could not resolve: ${unresolved_deps}")
endif()

if(SHARE_DIR AND EXISTS "${SHARE_DIR}/glib-2.0/schemas/gschemas.compiled")
    file(COPY "${SHARE_DIR}/glib-2.0/schemas/gschemas.compiled"
        DESTINATION "${DEST_DIR}/share/glib-2.0/schemas")
endif()
