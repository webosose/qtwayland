TARGET = QtWaylandClient

QT += core-private gui-private

equals(QT_MAJOR_VERSION,5) {
    lessThan(QT_MINOR_VERSION, 8) {
        QT_FOR_PRIVATE += platformsupport-private
    }
    else {
        QT += egl_support-private
        QT += fontdatabase_support-private
        QT += eventdispatcher_support-private
        QT += theme_support-private
    }
}

MODULE=waylandclient
MODULE_PLUGIN_TYPES = wayland-graphics-integration-client wayland-decoration-client wayland-inputdevice-integration

load(qt_module)

CONFIG -= precompile_header
CONFIG -= create_cmake
CONFIG += link_pkgconfig qpa/genericunixfontdatabase wayland-scanner

!equals(QT_WAYLAND_GL_CONFIG, nogl) {
    DEFINES += QT_WAYLAND_GL_SUPPORT
}

config_xkbcommon {
    !contains(QT_CONFIG, no-pkg-config) {
        PKGCONFIG += xkbcommon
    } else {
        LIBS += -lxkbcommon
    }
} else {
    DEFINES += QT_NO_WAYLAND_XKB
}

!contains(QT_CONFIG, no-pkg-config) {
    PKGCONFIG += wayland-client wayland-cursor
    contains(QT_CONFIG, glib): PKGCONFIG_PRIVATE += glib-2.0
} else {
    LIBS += -lwayland-client -lwayland-cursor $$QT_LIBS_GLIB
}

INCLUDEPATH += $$PWD/../shared

WAYLANDCLIENTSOURCES += \
            ../3rdparty/protocol/wayland.xml \
            ../extensions/surface-extension.xml \
            ../extensions/sub-surface-extension.xml \
            ../extensions/output-extension.xml \
            ../extensions/touch-extension.xml \
            ../extensions/qtkey-extension.xml \
            ../extensions/windowmanager.xml \
            ../3rdparty/protocol/text.xml \
            ../3rdparty/protocol/xdg-shell.xml \

SOURCES +=  qwaylandintegration.cpp \
            qwaylandnativeinterface.cpp \
            qwaylandshmbackingstore.cpp \
            qwaylandinputdevice.cpp \
            qwaylandcursor.cpp \
            qwaylanddisplay.cpp \
            qwaylandwindow.cpp \
            qwaylandscreen.cpp \
            qwaylandshmwindow.cpp \
            qwaylandclipboard.cpp \
            qwaylanddnd.cpp \
            qwaylanddataoffer.cpp \
            qwaylanddatadevicemanager.cpp \
            qwaylanddatasource.cpp \
            qwaylandshellsurface.cpp \
            qwaylandwlshellsurface.cpp \
            qwaylandxdgshell.cpp \
            qwaylandxdgsurface.cpp \
            qwaylandextendedoutput.cpp \
            qwaylandextendedsurface.cpp \
            qwaylandsubsurface.cpp \
            qwaylandtouch.cpp \
            qwaylandqtkey.cpp \
            ../shared/qwaylandmimehelper.cpp \
            qwaylandabstractdecoration.cpp \
            qwaylanddecorationfactory.cpp \
            qwaylanddecorationplugin.cpp \
            qwaylandwindowmanagerintegration.cpp \
            qwaylandinputcontext.cpp \
            qwaylanddatadevice.cpp

HEADERS +=  qwaylandintegration_p.h \
            qwaylandnativeinterface_p.h \
            qwaylandcursor_p.h \
            qwaylanddisplay_p.h \
            qwaylandwindow_p.h \
            qwaylandscreen_p.h \
            qwaylandshmbackingstore_p.h \
            qwaylandinputdevice_p.h \
            qwaylandbuffer_p.h \
            qwaylandshmwindow_p.h \
            qwaylandclipboard_p.h \
            qwaylanddnd_p.h \
            qwaylanddataoffer_p.h \
            qwaylanddatadevicemanager_p.h \
            qwaylanddatasource_p.h \
            qwaylandshellsurface_p.h \
            qwaylandwlshellsurface_p.h \
            qwaylandxdgshell_p.h \
            qwaylandxdgsurface_p.h \
            qwaylandextendedoutput_p.h \
            qwaylandextendedsurface_p.h \
            qwaylandsubsurface_p.h \
            qwaylandtouch_p.h \
            qwaylandqtkey_p.h \
            ../shared/qwaylandmimehelper.h \
            qwaylandabstractdecoration_p.h \
            qwaylanddecorationfactory_p.h \
            qwaylanddecorationplugin_p.h \
            qwaylandwindowmanagerintegration_p.h \
            qwaylandinputcontext_p.h \
            qwaylanddatadevice_p.h \
            qtwaylandclienttracer.h

lttng {
    DEFINES += HAS_LTTNG
    SOURCES +=  pmtrace_qtwaylandclient_provider.c
    HEADERS +=  pmtrace_qtwaylandclient_provider.h
    !contains(QT_CONFIG, no-pkg-config) {
        CONFIG += link_pkgconfig
        PKGCONFIG += lttng-ust
    } else {
        LIBS += -llttng-ust
    }
}

#exports hardwareintegration to compile wayland plugin outside
include(../hardwareintegration/client/wayland-egl/wayland-egl.pri)

include(hardwareintegration/hardwareintegration.pri)
include(shellintegration/shellintegration.pri)
include(inputdeviceintegration/inputdeviceintegration.pri)
