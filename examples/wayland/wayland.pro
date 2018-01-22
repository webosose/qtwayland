TEMPLATE=subdirs

#Only build compositor examples if we are building
#the QtCompositor API and examples
contains(CONFIG, examples) {
contains(CONFIG, wayland-compositor) {
    SUBDIRS += qwindow-compositor

    qtHaveModule(quick) {
        SUBDIRS += qml-compositor
    }

    SUBDIRS += server-buffer
}
}
