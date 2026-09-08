#!/usr/bin/env python3
"""A minimal MPRIS v2 mock player for testing dock-lyrics.

Exposes org.mpris.MediaPlayer2.docklyricsmock with a playing Chinese song
so the dock plugin has something to react to. Starts in "Playing" state and
keeps advancing Position every 500 ms. A .lrc sidecar at
~/Music/晴天.lrc lets the plugin test the local-lyrics path too.

Usage: python3 mpris_mock.py
"""
import time
import os
from gi.repository import GLib
import dbus, dbus.service
import dbus.mainloop.glib

BUS_NAME = "org.mpris.MediaPlayer2.docklyricsmock"
OBJ_PATH = "/org/mpris/MediaPlayer2"


class MockPlayer(dbus.service.Object):
    def __init__(self, bus):
        super().__init__(bus, OBJ_PATH)
        self.status = "Playing"
        self.title = "晴天"
        self.artist = ["周杰伦"]
        self.album = "叶惠美"
        self.length = 269000000  # microseconds
        self.position = 18000000  # start near 00:18 (为你翘课的那一天)
        self._last = time.monotonic()
        self._timer = GLib.timeout_add(500, self._tick)

    # ---------- helpers ----------
    def _meta(self):
        import urllib.parse
        url = "file://" + os.path.join(os.path.expanduser("~"), "Music",
                           self.title + ".mp3")
        return dbus.Dictionary({
            "xesam:title": dbus.String(self.title),
            "xesam:artist": dbus.Array([dbus.String(a) for a in self.artist], signature="s"),
            "xesam:album": dbus.String(self.album),
            "mpris:length": dbus.Int64(self.length),
            "xesam:url": dbus.String(url),
        }, signature="sv")

    def _tick(self):
        now = time.monotonic()
        if self.status == "Playing":
            self.position += int((now - self._last) * 1_000_000)
            if self.position > self.length:
                self.position = 0
        self._last = now
        return True

    def _notify(self, changed):
        self.PropertiesChanged("org.mpris.MediaPlayer2.Player", changed, [])

    # ---------- org.mpris.MediaPlayer2.Player ----------
    @dbus.service.method("org.mpris.MediaPlayer2.Player", in_signature="", out_signature="")
    def PlayPause(self):
        self.status = "Paused" if self.status == "Playing" else "Playing"
        self._last = time.monotonic()
        self._notify({"PlaybackStatus": self.status})

    @dbus.service.method("org.mpris.MediaPlayer2.Player", in_signature="", out_signature="")
    def Play(self):
        if self.status != "Playing":
            self.status = "Playing"
            self._last = time.monotonic()
            self._notify({"PlaybackStatus": self.status})

    @dbus.service.method("org.mpris.MediaPlayer2.Player", in_signature="", out_signature="")
    def Pause(self):
        if self.status != "Paused":
            self.status = "Paused"
            self._last = time.monotonic()
            self._notify({"PlaybackStatus": self.status})

    @dbus.service.method("org.mpris.MediaPlayer2.Player", in_signature="", out_signature="")
    def Next(self):
        self.title = "稻香" if self.title == "晴天" else "晴天"
        self.album = "魔杰座"
        self.length = 223000000
        self.position = 0
        self._last = time.monotonic()
        self._notify({"Metadata": self._meta()})

    @dbus.service.method("org.mpris.MediaPlayer2.Player", in_signature="", out_signature="")
    def Previous(self):
        self.Next()

    @dbus.service.method("org.mpris.MediaPlayer2.Player", in_signature="x", out_signature="")
    def Seek(self, offset):
        # Real players announce the new absolute position through Seeked.
        self.position = max(0, min(self.length, self.position + int(offset)))
        self._last = time.monotonic()
        self.Seeked(self.position)
        self._notify({"Position": self.position})

    # ---------- org.mpris.MediaPlayer2 ----------
    @dbus.service.method("org.mpris.MediaPlayer2", in_signature="", out_signature="")
    def Raise(self):
        pass

    @dbus.service.method("org.mpris.MediaPlayer2", in_signature="", out_signature="")
    def Quit(self):
        pass

    # ---------- org.freedesktop.DBus.Properties ----------
    @dbus.service.method("org.freedesktop.DBus.Properties", in_signature="ss", out_signature="v")
    def Get(self, iface, prop):
        if iface == "org.mpris.MediaPlayer2.Player":
            if prop == "PlaybackStatus":
                return dbus.String(self.status)
            if prop == "Position":
                return dbus.Int64(self.position)
            if prop == "Metadata":
                return self._meta()
        if iface == "org.mpris.MediaPlayer2" and prop == "Identity":
            return dbus.String("Dock Lyrics Mock")
        raise dbus.exceptions.DBusException(
            "org.freedesktop.DBus.Error.UnknownProperty",
            "no such property %s on %s" % (prop, iface))

    @dbus.service.method("org.freedesktop.DBus.Properties", in_signature="s", out_signature="a{sv}")
    def GetAll(self, iface):
        if iface == "org.mpris.MediaPlayer2.Player":
            return dbus.Dictionary({
                "PlaybackStatus": dbus.String(self.status),
                "Position": dbus.Int64(self.position),
                "Metadata": self._meta(),
            }, signature="sv")
        if iface == "org.mpris.MediaPlayer2":
            return dbus.Dictionary({"Identity": dbus.String("Dock Lyrics Mock")}, signature="sv")
        return dbus.Dictionary({}, signature="sv")

    # ---------- signals ----------
    @dbus.service.signal("org.freedesktop.DBus.Properties", signature="sa{sv}as")
    def PropertiesChanged(self, interface_name, changed_properties, invalidated):
        pass

    @dbus.service.signal("org.mpris.MediaPlayer2.Player", signature="x")
    def Seeked(self, position):
        pass


def main():
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SessionBus()
    name = dbus.service.BusName(BUS_NAME, bus)
    assert name
    MockPlayer(bus)
    print("mock running:", BUS_NAME, flush=True)
    GLib.MainLoop().run()


if __name__ == "__main__":
    main()
