TEMPLATE = lib
TARGET = Qt5WaylandEglClientBufferIntegration

QT = waylandcompositor waylandcompositor-private
CONFIG += no_install_prl

include(../../../../hardwareintegration/compositor/wayland-egl/wayland-egl.pri)

headers.files = $$HEADERS
headers.path += $$[QT_INSTALL_HEADERS]/QtWaylandCompositor/$$MODULE_VERSION/QtWaylandCompositor/private

target.path = $$[QT_INSTALL_LIBS]

INSTALLS += headers target
