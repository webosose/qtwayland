TEMPLATE = lib
TARGET = Qt5WaylandShellSurface

QT += waylandclient-private
CONFIG += no_install_prl

HEADERS = ../../../plugins/shellintegration/wl-shell/qwaylandwlshellsurface_p.h
SOURCES = ../../../plugins/shellintegration/wl-shell/qwaylandwlshellsurface.cpp

headers.files += $$HEADERS
headers.path += $$[QT_INSTALL_HEADERS]/QtWaylandClient/$$MODULE_VERSION/QtWaylandClient/private

target.path = $$[QT_INSTALL_LIBS]

INSTALLS += headers target
