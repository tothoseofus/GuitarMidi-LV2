
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

# Select the appropriate plugin description.
if(WITH_AUDIO_OUTPUT)
    set(PLUGIN_TTL_SOURCE
        "${CMAKE_SOURCE_DIR}/guitarmidi-audiooutput.ttl"
    )
else()
    set(PLUGIN_TTL_SOURCE
        "${CMAKE_SOURCE_DIR}/guitarmidi.ttl"
    )
endif()



# Prepare plugin metadata based on native UI availability.
# BUILD_UI=OFF removes X11 UI declarations.
# MOD GUI support is retained for all platforms.

if(NOT BUILD_UI)
    file(READ "${PLUGIN_TTL_SOURCE}" PLUGIN_CONTENT)

    # Normalise line endings.
    string(REPLACE "\r\n" "\n" PLUGIN_CONTENT "${PLUGIN_CONTENT}")

    # Remove the X11 UI definition.
    string(REGEX REPLACE
        "<http://github[.]com/geraldmwangi/GuitarMidi-LV2#ui>[ \t\n]+a ui:X11UI[^\n]*\n[^\n]*\n[^\n]*"
        ""
        PLUGIN_CONTENT
        "${PLUGIN_CONTENT}"
    )

    # Remove the plugin's reference to the X11 UI.
    string(REGEX REPLACE
        "[ \t]*ui:ui[ \t]+<http://github[.]com/geraldmwangi/GuitarMidi-LV2#ui>[ \t]*;[ \t]*\n"
        ""
        PLUGIN_CONTENT
        "${PLUGIN_CONTENT}"
    )

    # Remove the X11 UI port notification statement.
    string(REGEX REPLACE
        "<http://github[.]com/geraldmwangi/GuitarMidi-LV2#ui>[ \t\n]+ui:portNotification[ \t]*\\[[^\n]*\n[^\n]*\n[^\n]*\n[^\n]*\n[^\n]*"
        ""
        PLUGIN_CONTENT
        "${PLUGIN_CONTENT}"
    )

    # Prevent packaging if an X11 UI reference remains.
    if(PLUGIN_CONTENT MATCHES "GuitarMidi-LV2#ui")
        message(FATAL_ERROR
            "X11 UI metadata remains despite BUILD_UI=OFF"
        )
    endif()

    file(WRITE
        "${CMAKE_CURRENT_BINARY_DIR}/guitarmidi.ttl"
        "${PLUGIN_CONTENT}"
    )

else()
    # Preserve original metadata when native UI is enabled.
    configure_file(
        "${PLUGIN_TTL_SOURCE}"
        "${CMAKE_CURRENT_BINARY_DIR}/guitarmidi.ttl"
        COPYONLY
    )
endif()



# Prepare the manifest for the selected platform and UI configuration.

file(READ
    "${CMAKE_SOURCE_DIR}/manifest.ttl"
    MANIFEST_CONTENT
)

# Windows uses a DLL rather than a Linux shared library.
if(WIN32)
    if(NOT MANIFEST_CONTENT MATCHES "guitarmidi[.]so")
        message(FATAL_ERROR
            "Expected guitarmidi.so in manifest.ttl"
        )
    endif()

    string(REPLACE
        "guitarmidi.so"
        "guitarmidi.dll"
        MANIFEST_CONTENT
        "${MANIFEST_CONTENT}"
    )
endif()

# Remove X11 UI declarations when the native UI is disabled.
if(NOT BUILD_UI)
    string(REGEX REPLACE
        "[^\r\n]*<http://github[.]com/geraldmwangi/GuitarMidi-LV2#ui>[^\r\n]*\r?\n?"
        ""
        MANIFEST_CONTENT
        "${MANIFEST_CONTENT}"
    )
endif()

file(WRITE
    "${CMAKE_CURRENT_BINARY_DIR}/manifest.ttl"
    "${MANIFEST_CONTENT}"
)


# Install metadata and trained TensorFlow Lite model.
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/manifest.ttl"
    "${CMAKE_CURRENT_BINARY_DIR}/guitarmidi.ttl"
    "${CMAKE_SOURCE_DIR}/python/neuralnetmodelling/guitarmidi.tflite"
    DESTINATION guitarmidi.lv2
)



# Install MOD GUI resources for MOD Desktop and MODEP.
# Use the existing piStomp GUI as the shared source.

set(MODGUI_SOURCE
    "${CMAKE_SOURCE_DIR}/hardware/moddevices/gui"
)

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

