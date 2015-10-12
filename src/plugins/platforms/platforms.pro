TEMPLATE=subdirs
CONFIG+=ordered
QT_FOR_CONFIG += waylandclient-private

config_no_webos {
    SUBDIRS += qwayland-generic
}

qtConfig(wayland-egl): \
    config_no_webos {
        SUBDIRS += qwayland-egl
    }

#The following integrations are only useful with QtWaylandCompositor
qtConfig(wayland-brcm): \
    SUBDIRS += qwayland-brcm-egl

qtConfig(xcomposite-egl): \
    SUBDIRS += qwayland-xcomposite-egl
qtConfig(xcomposite-glx): \
    SUBDIRS += qwayland-xcomposite-glx
