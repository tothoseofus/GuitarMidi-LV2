GuitarMidi-LV2 MOD GUI — matched to supplied compiled TTL

This is a MOD-UI custom graphical interface, not a DSP/plugin rebuild.
Uses exactly the plugin URI and 8 control symbols defined in the uploaded guitarmidi.ttl.

INSTALL (from Windows or Linux):
1. Back up the current guitarmidi.lv2 directory on the piStomp.
2. Copy manifest.ttl, modgui.ttl, and the modgui/ folder from this archive
   into the existing /home/pistomp/.lv2/guitarmidi.lv2/ directory.
   Do not overwrite guitarmidi.so or guitarmidi.ttl.
3. Restart the actual piStomp MOD UI service: sudo systemctl restart mod-ui
   Then refresh your browser (Ctrl+Shift+R).
4. Check the GUI and verify control movements affect the plugin.

ROLLBACK: Restore the previous manifest.ttl, modgui.ttl and modgui/ folder
from the backup, then restart mod-ui.

NOTE: The installed developer manifest was used as the base because it is the
known working MODEP version; the ORIGINALmanifest.ttl also contains an X11 UI
reference to guitarmidi_ui.so. This MOD-only patch does not require that X11 UI.

MOD GUI symbols (NOT the LV2 numeric port indices):
0 gain, 1 expressivity, 2 onsetsmoothing, 3 onsetconfthr, 4 onsetenthr,
5 offsetsmoothing, 6 offsetconfthr, 7 offsetenthr.

This UI has not been tested inside MODEP. Knob sprites are 64-frame vertical strips.

BYPASS CONTROL (added in this version):
The header now displays a clearly labelled ON/BYPASS switch and status LED.
Both are wired to MOD-UI using native mod-role="bypass" and
mod-role="bypass-light" attributes. There is no extra LV2 port.
IMPORTANT: MOD bypass class .on means BYPASSED (plugin disabled);
.off means plugin ACTIVE. This is a MODEP bypass, not a new DSP control.
No changes are made to the existing .so, plugin TTL or port list.

FIXED: 64-frame knob sprites are now HORIZONTAL (4992x78), as required by MOD-UI.
FIXED: bypass-light .on now illuminates the status light; .off dims it.
The native bypass switch remains unchanged (.on = bypassed, .off = processing).
