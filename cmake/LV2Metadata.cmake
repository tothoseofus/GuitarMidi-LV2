
# Shared GuitarMidi LV2 metadata substitutions.
#
# WIN32 controls the platform binary filename.
# BUILD_UI controls the native X11 UI declarations.
# MOD web GUI metadata is shared across builds.

set(GUITARMIDI_PLUGIN_URI
    "http://github.com/geraldmwangi/GuitarMidi-LV2"
)

set(GUITARMIDI_UI_URI
    "${GUITARMIDI_PLUGIN_URI}#ui"
)

# --------------------------------------------
# Platform-specific binary filename
# --------------------------------------------

if(WIN32)
    set(MANIFEST_LV2_BINARY
        "lv2:binary <guitarmidi.dll> ;"
    )
else()
    set(MANIFEST_LV2_BINARY
        "lv2:binary <guitarmidi.so> ;"
    )
endif()

# --------------------------------------------
# Optional native X11 UI
# --------------------------------------------

set(MANIFEST_UI_DEFINITIONS "")
set(GUITARMIDI_UI_DEFINITION "")
set(GUITARMIDI_UI_REFERENCE "")
set(GUITARMIDI_UI_PORT_NOTIFICATION "")

if(BUILD_UI)

    set(MANIFEST_UI_DEFINITIONS
"<${GUITARMIDI_UI_URI}>
    a ui:X11UI ;
    ui:binary <guitarmidi_ui.so> ;
    rdfs:seeAlso <guitarmidi.ttl> ."
    )

    set(GUITARMIDI_UI_DEFINITION
"<${GUITARMIDI_UI_URI}>
    a ui:X11UI ;
    lv2:requiredFeature
        ui:idleInterface, ui:portMap, urid:map ;
    lv2:extensionData ui:idleInterface ."
    )

    set(GUITARMIDI_UI_REFERENCE
        "ui:ui <${GUITARMIDI_UI_URI}> ;"
    )

    set(GUITARMIDI_UI_PORT_NOTIFICATION
"<${GUITARMIDI_UI_URI}>
    ui:portNotification [
        ui:plugin <${GUITARMIDI_PLUGIN_URI}> ;
        lv2:symbol \"debugdata\" ;
        ui:protocol atom:eventTransfer
    ] ."
    )

endif()
