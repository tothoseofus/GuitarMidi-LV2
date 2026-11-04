
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


# Prepare the platform-specific plugin description.
# Linux retains the X11 UI; Windows excludes it.

if(NOT BUILD_UI)
    file(READ "${PLUGIN_TTL_SOURCE}" PLUGIN_CONTENT)

    # Normalise line endings before processing.
    string(REPLACE "\r\n" "\n" PLUGIN_CONTENT "${PLUGIN_CONTENT}")

    # Escape semicolons before converting text to a CMake list.
    string(REPLACE ";" "\\;" PLUGIN_CONTENT "${PLUGIN_CONTENT}")
    string(REPLACE "\n" ";" PLUGIN_LINES "${PLUGIN_CONTENT}")

    set(PLUGIN_CONTENT "")
    set(SKIP_X11_UI_BLOCK FALSE)

    foreach(LINE IN LISTS PLUGIN_LINES)

        # Skip entire Turtle statements describing the X11 UI.
        if(LINE MATCHES
           "^[ \t]*<http://github[.]com/geraldmwangi/GuitarMidi-LV2#ui>")
            set(SKIP_X11_UI_BLOCK TRUE)
        endif()

        if(SKIP_X11_UI_BLOCK)
            # A period terminates the current Turtle statement.
            if(LINE MATCHES "[.][ \t]*$")
                set(SKIP_X11_UI_BLOCK FALSE)
            endif()
            continue()
        endif()

        # Remove the plugin's reference to the X11 UI.
        if(LINE MATCHES "^[ \t]*ui:ui[ \t]+")
            continue()
        endif()

        string(APPEND PLUGIN_CONTENT "${LINE}\n")
    endforeach()

    if(SKIP_X11_UI_BLOCK)
        message(FATAL_ERROR
            "Unterminated X11 UI statement in ${PLUGIN_TTL_SOURCE}"
        )
    endif()

    file(WRITE
        "${CMAKE_CURRENT_BINARY_DIR}/guitarmidi.ttl"
        "${PLUGIN_CONTENT}"
    )

else()
    # Linux retains the original description and X11 UI.
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

