QT = compositor compositor-private core-private gui-private

equals(QT_MAJOR_VERSION,5) {
    lessThan(QT_MINOR_VERSION, 8) {
        QT += platformsupport-private
    }
    else {
        QT += egl_support-private
    }
}

OTHER_FILES += wayland-egl.json

SOURCES += \
    main.cpp

include(../../../../hardwareintegration/compositor/wayland-egl/wayland-egl.pri)

PLUGIN_TYPE = wayland-graphics-integration-server
load(qt_plugin)
