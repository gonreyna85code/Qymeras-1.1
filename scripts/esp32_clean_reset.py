# PlatformIO post-upload hook (ESP32 only).
#
# After esptool flashes, its generic "Hard resetting via RTS pin..." leaves many
# ESP32 clones (Wemos D1 R32, NodeMCU-32S, etc.) stuck in a reset loop or in ROM
# download mode, so the device appears to "not boot". A clean reset with GPIO0
# held HIGH (normal boot from flash) boots the app reliably.
#
# Wired into [env:esp32_devkit] via `extra_scripts`. Not used by ESP8266.
Import("env")


def _clean_reset(port):
    import serial
    import time

    ser = serial.Serial(port, 115200, timeout=0.2)
    time.sleep(0.1)
    ser.dtr = False   # GPIO0 = HIGH (normal boot)
    ser.rts = True    # EN = LOW (chip held in reset)
    time.sleep(0.2)
    ser.rts = False   # EN = HIGH (chip starts, GPIO0 high -> boot from flash)
    time.sleep(0.3)
    ser.close()


def after_upload(source, target, env):
    port = env.subst("$UPLOAD_PORT")
    if not port:
        port = env.GetProjectOption("upload_port", "")
    if not port:
        print("[reset] no upload port found, skipping clean reset")
        return
    try:
        print("[reset] clean ESP32 reset (IO0 high) on %s" % port)
        _clean_reset(port)
        print("[reset] done - device booting")
    except Exception as e:
        print("[reset] WARNING: clean reset failed: %s" % e)


env.AddPostAction("upload", after_upload)