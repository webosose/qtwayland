TEMPLATE=subdirs

config_no_webos {
    SUBDIRS += client
}

#The compositor plugins are only useful with QtCompositor
contains(CONFIG, wayland-compositor) {
        SUBDIRS += compositor
}
