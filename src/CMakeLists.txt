
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(${CMAKE_BINARY_DIR}/core/include)
include_directories(include)
include_directories(${CMAKE_SOURCE_DIR}/core/include)

# Plugin source files
set(SRC
    guitarmidi.cpp
    midioutput.cpp
    lv2logger.cpp
)

# Build the LV2 plugin
add_library(guitarmidi SHARED ${SRC})

target_link_libraries(guitarmidi PUBLIC
    guitarmidi_core
)

# Produce guitarmidi.so on Linux and guitarmidi.dll on Windows
set_target_properties(guitarmidi PROPERTIES
    PREFIX ""
    OUTPUT_NAME "guitarmidi"
)

# Install the plugin binary into the LV2 bundle.
# RUNTIME is required for Windows DLLs.
# LIBRARY is required for Linux shared libraries.
install(TARGETS guitarmidi
    RUNTIME DESTINATION guitarmidi.lv2
    LIBRARY DESTINATION guitarmidi.lv2
)

# --------------------------------------------------
# Select the plugin description template
# --------------------------------------------------

if(WITH_AUDIO_OUTPUT)
    set(PLUGIN_TTL_SOURCE
        "${CMAKE_SOURCE_DIR}/guitarmidi-audiooutput.ttl.in"
    )
else()
    set(PLUGIN_TTL_SOURCE
        "${CMAKE_SOURCE_DIR}/guitarmidi.ttl.in"
    )
endif()

# --------------------------------------------------
# Generate LV2 metadata from shared TTL templates
# --------------------------------------------------

# Platform-specific substitutions are defined here.
# WIN32 controls the plugin binary filename.
# BUILD_UI controls the native X11 UI declarations.
# MOD GUI metadata is common to all platforms.
include("${CMAKE_SOURCE_DIR}/cmake/LV2Metadata.cmake")

# Generate a final .ttl file from a .ttl.in template.
function(generate_lv2_metadata SOURCE_FILE OUTPUT_FILE)

    if(NOT EXISTS "${SOURCE_FILE}")
        message(FATAL_ERROR
            "LV2 metadata template not found: ${SOURCE_FILE}"
        )
    endif()

    file(READ "${SOURCE_FILE}" TTL_CONTENT)

    foreach(PLACEHOLDER IN ITEMS
        MANIFEST_LV2_BINARY
        MANIFEST_UI_DEFINITIONS
        GUITARMIDI_UI_DEFINITION
        GUITARMIDI_UI_REFERENCE
        GUITARMIDI_UI_PORT_NOTIFICATION
    )
        string(REPLACE
            "#<${PLACEHOLDER}>"
            "${${PLACEHOLDER}}"
            TTL_CONTENT
            "${TTL_CONTENT}"
        )
    endforeach()

    # Fail if any metadata placeholders remain unresolved.
    if(TTL_CONTENT MATCHES "#<[A-Z_]+>")
        message(FATAL_ERROR
            "Unresolved LV2 metadata placeholder in ${SOURCE_FILE}"
        )
    endif()

    file(WRITE
        "${OUTPUT_FILE}"
        "${TTL_CONTENT}"
    )

endfunction()

# Generate the platform-specific manifest.
generate_lv2_metadata(
    "${CMAKE_SOURCE_DIR}/manifest.ttl.in"
    "${CMAKE_CURRENT_BINARY_DIR}/manifest.ttl"
)

# Generate the selected plugin description.
# Native X11 UI metadata is included only when BUILD_UI=ON.
generate_lv2_metadata(
    "${PLUGIN_TTL_SOURCE}"
    "${CMAKE_CURRENT_BINARY_DIR}/guitarmidi.ttl"
)

# --------------------------------------------------
# Install generated LV2 metadata and model
# --------------------------------------------------

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/manifest.ttl"
    "${CMAKE_CURRENT_BINARY_DIR}/guitarmidi.ttl"
    "${CMAKE_SOURCE_DIR}/python/neuralnetmodelling/guitarmidi.tflite"
    DESTINATION guitarmidi.lv2
)

# --------------------------------------------------
# Install MOD GUI resources
# --------------------------------------------------

# The existing piStomp MOD GUI is the shared source
# for Windows, Linux and ARM64 builds.

set(MODGUI_SOURCE
    "${CMAKE_SOURCE_DIR}/hardware/moddevices/gui"
)

# The shared manifest references modgui.ttl, so the
# MOD GUI resources must exist for every build.

if(NOT EXISTS "${MODGUI_SOURCE}/modgui.ttl")
    message(FATAL_ERROR
        "MOD GUI metadata missing: ${MODGUI_SOURCE}/modgui.ttl"
    )
endif()

if(NOT IS_DIRECTORY "${MODGUI_SOURCE}/modgui")
    message(FATAL_ERROR
        "MOD GUI resources missing: ${MODGUI_SOURCE}/modgui"
    )
endif()

install(FILES
    "${MODGUI_SOURCE}/modgui.ttl"
    DESTINATION guitarmidi.lv2
)

install(DIRECTORY
    "${MODGUI_SOURCE}/modgui/"
    DESTINATION guitarmidi.lv2/modgui
)
