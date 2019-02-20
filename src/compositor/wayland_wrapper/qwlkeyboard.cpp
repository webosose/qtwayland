/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2013 Klarälvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Compositor.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwlkeyboard_p.h"

#include <QFile>
#include <QStandardPaths>
#include <QDebug>

#include "qwlcompositor_p.h"
#include "qwlsurface_p.h"

#include <fcntl.h>
#include <unistd.h>
#ifndef QT_NO_WAYLAND_XKB
#include <sys/mman.h>
#include <sys/types.h>
#endif

QT_BEGIN_NAMESPACE

namespace QtWayland {

#ifndef QT_NO_WAYLAND_XKB
class XKBKeymap
{
public:
    XKBKeymap();
    ~XKBKeymap();

    struct Data {
        xkb_context *context;
        xkb_keymap *map;
        int map_fd;
        size_t map_size;
        char *map_area;
    };

    static XKBKeymap *instance()
    {
        static XKBKeymap *s = NULL;
        if (!s)
            s = new XKBKeymap;
        return s;
    }

    static XKBKeymap::Data* createXKBKeymap(QWaylandKeymap keymap);

    const XKBKeymap::Data* defaultXKBKeymap() { return m_default_data; }

private:
    QWaylandKeymap m_default_keymap;
    XKBKeymap::Data *m_default_data;
};

XKBKeymap::XKBKeymap()
{
    QString ruleNames = QString(qgetenv("QT_WAYLAND_XKB_RULE_NAMES"));

    if (!ruleNames.isEmpty()) {
        // rules:model:layout:variant:options
        QStringList split = ruleNames.split(":");
        if (split.length() == 5) {
            qInfo() << "Using QT_WAYLAND_XKB_RULE_NAMES for default XKB keymap:" << ruleNames;
            m_default_keymap = QWaylandKeymap(split[2], QString(), QString(), split[1], split[0]);
        } else {
            qWarning("Error to parse QT_WAYLAND_XKB_RULE_NAMES. Default XKB keymap will be used.");
            m_default_keymap = QWaylandKeymap();
        }
    } else {
        qWarning("No QT_WAYLAND_XKB_RULE_NAMES set. Default XKB keymap will be used.");
        m_default_keymap = QWaylandKeymap();
    }

    m_default_data = XKBKeymap::createXKBKeymap(m_default_keymap);
}

static int createAnonymousFile(size_t size)
{
#ifndef NO_WEBOS_PLATFORM
    // See must be reverted in BHV-1362
    QString path = QString(qgetenv("XDG_RUNTIME_DIR"));
#else
    QString path = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
#endif
    if (path.isEmpty())
        return -1;

    QByteArray name = QFile::encodeName(path + QStringLiteral("/qtwayland-XXXXXX"));

    int fd = mkstemp(name.data());
    if (fd < 0)
        return -1;

    long flags = fcntl(fd, F_GETFD);
    if (flags == -1 || fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
        close(fd);
        fd = -1;
    }
    unlink(name.constData());

    if (fd < 0)
        return -1;

    if (ftruncate(fd, size) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

XKBKeymap::Data* XKBKeymap::createXKBKeymap(QWaylandKeymap keymap)
{
    xkb_context *context = xkb_context_new(static_cast<xkb_context_flags>(0));
    if (!context) {
        qWarning("Failed to create XKB context");
        return NULL;
    }

    xkb_keymap *map = NULL;
    int map_fd = -1;
    size_t map_size = 0;
    char *map_area = NULL;
    struct xkb_rule_names rule_names = { strdup(qPrintable(keymap.rules())),
                                         strdup(qPrintable(keymap.model())),
                                         strdup(qPrintable(keymap.layout())),
                                         strdup(qPrintable(keymap.variant())),
                                         strdup(qPrintable(keymap.options())) };

    map = xkb_keymap_new_from_names(context, &rule_names, static_cast<xkb_keymap_compile_flags>(0));
    if (map) {
        char *str = xkb_keymap_get_as_string(map, XKB_KEYMAP_FORMAT_TEXT_V1);
        if (str) {
            map_size = strlen(str) + 1;
            map_fd = createAnonymousFile(map_size);
            if (map_fd >= 0) {
                map_area = static_cast<char *>(mmap(0, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, map_fd, 0));
                if (map_area != MAP_FAILED)
                    strcpy(map_area, str);
            }
        }
    }

    free((char *)rule_names.rules);
    free((char *)rule_names.model);
    free((char *)rule_names.layout);
    free((char *)rule_names.variant);
    free((char *)rule_names.options);

    if (map_area) {
        XKBKeymap::Data *ret = new XKBKeymap::Data;
        ret->context = context;
        ret->map = map;
        ret->map_fd = map_fd;
        ret->map_size = map_size;
        ret->map_area = map_area;

        qDebug() << "Created XKB keymap:"
            << "fd:"        << map_fd
            << "size:"      << map_size
            << "layout:"    << keymap.layout()
            << "variant:"   << keymap.variant()
            << "options:"   << keymap.options()
            << "model:"     << keymap.model()
            << "rules:"     << keymap.rules();

        return ret;
    }

    qWarning("Failed to create XKB keymap");

    if (map_fd >= 0)
        close(map_fd);
    if (map)
        xkb_keymap_unref(map);
    if (context)
        xkb_context_unref(context);

    return NULL;
}

XKBKeymap::~XKBKeymap()
{
    if (m_default_data) {
        if (m_default_data->map_area)
            munmap(m_default_data->map_area, m_default_data->map_size);
        if (m_default_data->map_fd >= 0)
            close(m_default_data->map_fd);
        if (m_default_data->map)
            xkb_keymap_unref(m_default_data->map);
        if (m_default_data->context)
            xkb_context_unref(m_default_data->context);

        delete m_default_data;
    }
}
#endif

Keyboard::Keyboard(Compositor *compositor, InputDevice *seat)
    : QtWaylandServer::wl_keyboard()
    , m_compositor(compositor)
    , m_seat(seat)
    , m_grab(this)
    , m_focus()
    , m_focusResource()
    , m_keys()
    , m_modsDepressed()
    , m_modsLatched()
    , m_modsLocked()
    , m_group()
    , m_pendingKeymap(false)
    , m_pendingFocus(NULL)
#ifndef QT_NO_WAYLAND_XKB
    , m_keymap_size(0)
    , m_keymap_fd(-1)
    , m_keymap_area(0)
    , m_context(0)
    , m_state(0)
    , m_keymap_shared(false)
#endif
{
#ifndef QT_NO_WAYLAND_XKB
    initXKB();
#endif
    connect(&m_focusDestroyListener, &WlListener::fired, this, &Keyboard::focusDestroyed);
    connect(&m_pendingFocusDestroyListener, &WlListener::fired, this, &Keyboard::pendingFocusDestroyed);
}

Keyboard::~Keyboard()
{
#ifndef QT_NO_WAYLAND_XKB
    releaseXKB();
#endif
}

KeyboardGrabber::~KeyboardGrabber()
{
}

void Keyboard::startGrab(KeyboardGrabber *grab)
{
    m_grab = grab;
    m_grab->m_keyboard = this;
    m_grab->focused(m_focus);
}

void Keyboard::endGrab()
{
    m_grab = this;
    focused(m_pendingFocus);

    //Modifier state can be changed during grab status.
    //So send it again.
    m_grab->modifiers(wl_display_next_serial(m_compositor->wl_display()), m_modsDepressed,
                      m_modsLatched, m_modsLocked, m_group);
}

KeyboardGrabber *Keyboard::currentGrab() const
{
    return m_grab;
}

void Keyboard::checkFocusResource(wl_keyboard::Resource *keyboardResource)
{
    if (!keyboardResource || !m_focus)
        return;

    // this is already the current  resource, do no send enter twice
    if (m_focusResource == keyboardResource)
        return;

    // check if new wl_keyboard resource is from the client owning the focus surface
    struct ::wl_client *focusedClient = m_focus->resource()->client();
    if (focusedClient == keyboardResource->client()) {
        sendEnter(m_focus, keyboardResource);
        m_focusResource = keyboardResource;
    }
}

void Keyboard::sendEnter(Surface *surface, wl_keyboard::Resource *keyboardResource)
{
    uint32_t serial = wl_display_next_serial(m_compositor->wl_display());
    send_modifiers(keyboardResource->handle, serial, m_modsDepressed, m_modsLatched, m_modsLocked, m_group);
    send_enter(keyboardResource->handle, serial, surface->resource()->handle, QByteArray::fromRawData((char *)m_keys.data(), m_keys.size() * sizeof(uint32_t)));
}

void Keyboard::focused(Surface *surface)
{
    if (m_focus != surface) {
        if (m_focusResource) {
            uint32_t serial = wl_display_next_serial(m_compositor->wl_display());
            send_leave(m_focusResource->handle, serial, m_focus->resource()->handle);
        }
        m_focusDestroyListener.reset();
        if (surface) {
            m_focusDestroyListener.listenForDestruction(surface->resource()->handle);
        }
    }

    Resource *resource = surface ? resourceMap().value(surface->resource()->client()) : 0;

    if (resource && (m_focus != surface || m_focusResource != resource)) {
        sendEnter(surface, resource);
    }

    m_focusResource = resource;
    m_focus = surface;
    Q_EMIT focusChanged(m_focus);
}

void Keyboard::setFocus(Surface* surface)
{
    if (m_pendingFocus != surface) {
        m_pendingFocus = surface;
        m_pendingFocusDestroyListener.reset();

        if (surface)
            m_pendingFocusDestroyListener.listenForDestruction(surface->resource()->handle);
    }

    m_grab->focused(surface);
}

void Keyboard::setKeymap(const QWaylandKeymap &keymap)
{
    m_keymap = keymap;
    m_pendingKeymap = true;

    // If there is no key currently pressed, update right away the keymap
    // Otherwise, delay the update when keys are released
    // see http://lists.freedesktop.org/archives/wayland-devel/2013-October/011395.html
    if (m_keys.isEmpty()) {
        updateKeymap();
    }
}

void Keyboard::focusDestroyed(void *data)
{
    Q_UNUSED(data)
    m_focusDestroyListener.reset();

    m_focus = 0;
    m_focusResource = 0;
    qDebug() << "m_focus destroyed";
}

void Keyboard::pendingFocusDestroyed(void *data)
{
    Q_UNUSED(data)
    m_pendingFocusDestroyListener.reset();
    m_pendingFocus = NULL;
    qDebug() << "m_pendingFocus destroyed";
}

void Keyboard::sendKeyModifiers(wl_keyboard::Resource *resource, uint32_t serial)
{
    send_modifiers(resource->handle, serial, m_modsDepressed, m_modsLatched, m_modsLocked, m_group);
}

void Keyboard::sendKeyPressEvent(uint code, bool repeat)
{
    sendKeyEvent(code, WL_KEYBOARD_KEY_STATE_PRESSED, repeat);
}

void Keyboard::sendKeyReleaseEvent(uint code, bool repeat)
{
    sendKeyEvent(code, WL_KEYBOARD_KEY_STATE_RELEASED, repeat);
}

Surface *Keyboard::focus() const
{
    return m_focus;
}

QtWaylandServer::wl_keyboard::Resource *Keyboard::focusResource() const
{
    return m_focusResource;
}

void Keyboard::keyboard_bind_resource(wl_keyboard::Resource *resource)
{
#ifndef QT_NO_WAYLAND_XKB
    if (m_context) {
        send_keymap(resource->handle, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                    m_keymap_fd, m_keymap_size);
    }
    else
#endif
    {
        int null_fd = open("/dev/null", O_RDONLY);
        send_keymap(resource->handle, 0 /* WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP */,
                    null_fd, 0);
        close(null_fd);
    }

    checkFocusResource(resource);
}

void Keyboard::keyboard_destroy_resource(wl_keyboard::Resource *resource)
{
    if (m_focusResource == resource)
        m_focusResource = 0;
}

void Keyboard::keyboard_release(wl_keyboard::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void Keyboard::key(uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
    if (m_focusResource) {
        send_key(m_focusResource->handle, serial, time, key, state);
    }
}

void Keyboard::keyEvent(uint code, uint32_t state)
{
    uint key = code - 8;
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
#ifdef NO_WEBOS_PLATFORM
        // In webos, there is no scence to pass pressed keys when switching focus
        // Also, keymaps are always set before sendKeyEvent.
        m_keys << key;
#endif
    } else {
        for (int i = 0; i < m_keys.size(); ++i) {
            if (m_keys.at(i) == key) {
                m_keys.remove(i);
            }
        }
    }
}

void Keyboard::sendKeyEvent(uint code, uint32_t state, bool repeat)
{
    uint32_t time = m_compositor->currentTimeMsecs();
    uint32_t serial = wl_display_next_serial(m_compositor->wl_display());
    uint key = code - 8;
    m_grab->key(serial, time, key, state);

    // Was replaced from end of (void Keyboard::keyEvent(uint code, uint32_t state))
    updateModifierState(code, state, repeat);
}

void Keyboard::modifiers(uint32_t serial, uint32_t mods_depressed,
                         uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
    if (m_focusResource) {
        send_modifiers(m_focusResource->handle, serial, mods_depressed, mods_latched, mods_locked, group);
    }
}

void Keyboard::updateModifierState(uint code, uint32_t state, bool repeat)
{
#ifndef QT_NO_WAYLAND_XKB
    if (!m_context)
        return;

    // xkb needs to match a the series of calls a XKB_KEY_DOWN, needs to be matched with
    // XKB_KEY_UP, hence when repeating down update the state otherwise situations like
    // "stuck modifiers" may occur.
    if (repeat) {
        return;
    }
    xkb_state_update_key(m_state, code, state == WL_KEYBOARD_KEY_STATE_PRESSED ? XKB_KEY_DOWN : XKB_KEY_UP);

    uint32_t modsDepressed = xkb_state_serialize_mods(m_state, (xkb_state_component)XKB_STATE_DEPRESSED);
    uint32_t modsLatched = xkb_state_serialize_mods(m_state, (xkb_state_component)XKB_STATE_LATCHED);
    uint32_t modsLocked = xkb_state_serialize_mods(m_state, (xkb_state_component)XKB_STATE_LOCKED);
    uint32_t group = xkb_state_serialize_group(m_state, (xkb_state_component)XKB_STATE_EFFECTIVE);

    if (modsDepressed == m_modsDepressed
            && modsLatched == m_modsLatched
            && modsLocked == m_modsLocked
            && group == m_group)
        return;

    m_modsDepressed = modsDepressed;
    m_modsLatched = modsLatched;
    m_modsLocked = modsLocked;
    m_group = group;

    m_grab->modifiers(wl_display_next_serial(m_compositor->wl_display()), m_modsDepressed, m_modsLatched, m_modsLocked, m_group);
#else
    Q_UNUSED(code);
    Q_UNUSED(state);
    Q_UNUSED(repeat);
#endif
}

/* Copy the target Keybard's modifier state.
 * NOTE: This it value copy of the target modifier data.  So we should update xkb_state too.
 * */
void Keyboard::updateModifierState(Keyboard *refKeyboard)
{
    if (refKeyboard) {
        m_modsDepressed = xkb_state_serialize_mods(refKeyboard->m_state, (xkb_state_component)XKB_STATE_MODS_DEPRESSED);
        m_modsLatched   = xkb_state_serialize_mods(refKeyboard->m_state, (xkb_state_component)XKB_STATE_MODS_LATCHED);
        m_modsLocked    = xkb_state_serialize_mods(refKeyboard->m_state, (xkb_state_component)XKB_STATE_MODS_LOCKED);
        m_group         = xkb_state_serialize_group(refKeyboard->m_state, (xkb_state_component)XKB_STATE_EFFECTIVE);

        xkb_state_update_mask(m_state, m_modsDepressed, m_modsLatched, m_modsLocked, 0, 0, m_group);
        m_grab->modifiers(wl_display_next_serial(m_compositor->wl_display()), m_modsDepressed, m_modsLatched, m_modsLocked, m_group);
    }
}

void Keyboard::updateKeymap()
{
    // There must be no keys pressed when changing the keymap,
    // see http://lists.freedesktop.org/archives/wayland-devel/2013-October/011395.html
    if (!m_pendingKeymap || !m_keys.isEmpty())
        return;

    m_pendingKeymap = false;
#ifndef QT_NO_WAYLAND_XKB
    // Release old keymap
    releaseXKB();

    XKBKeymap::Data *data = XKBKeymap::instance()->createXKBKeymap(m_keymap);
    if (data) {
        m_context = data->context;
        m_keymap_fd = data->map_fd;
        m_keymap_size = data->map_size;
        m_keymap_area = data->map_area;
        m_state = xkb_state_new(data->map);
        m_keymap_shared = false;
        qInfo() << "New XKB keymap has been created," << this << "fd:" << m_keymap_fd << "size:" << m_keymap_size;
        delete data;
    } else {
        qWarning() << "Unable to update XKB keymap," << this;
        return;
    }

    foreach (Resource *res, resourceMap()) {
        send_keymap(res->handle, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, m_keymap_fd, m_keymap_size);
    }

    xkb_state_update_mask(m_state, 0, m_modsLatched, m_modsLocked, 0, 0, 0);
    if (m_focusResource)
        sendKeyModifiers(m_focusResource, wl_display_next_serial(m_compositor->wl_display()));
#endif
}

#ifndef QT_NO_WAYLAND_XKB
void Keyboard::initXKB()
{
    const XKBKeymap::Data *data = XKBKeymap::instance()->defaultXKBKeymap();

    if (data) {
        m_context = data->context;
        m_keymap_fd = data->map_fd;
        m_keymap_size = data->map_size;
        m_keymap_area = data->map_area;
        m_state = xkb_state_new(data->map);
        m_keymap_shared = true;
        qInfo() << "Using default XKB keymap," << this << "fd:" << m_keymap_fd << "size:" << m_keymap_size;
    }
}

void Keyboard::createXKBState(xkb_keymap *keymap)
{
    char *keymap_str = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
    if (!keymap_str) {
        qWarning("Failed to compile global XKB keymap");
        return;
    }

    m_keymap_size = strlen(keymap_str) + 1;
    if (m_keymap_fd >= 0)
        close(m_keymap_fd);
    m_keymap_fd = createAnonymousFile(m_keymap_size);
    if (m_keymap_fd < 0) {
        qWarning("Failed to create anonymous file of size %lu", static_cast<unsigned long>(m_keymap_size));
        return;
    }

    m_keymap_area = static_cast<char *>(mmap(0, m_keymap_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_keymap_fd, 0));
    if (m_keymap_area == MAP_FAILED) {
        close(m_keymap_fd);
        m_keymap_fd = -1;
        qWarning("Failed to map shared memory segment");
        return;
    }

    strcpy(m_keymap_area, keymap_str);
    free(keymap_str);

    if (m_state)
        xkb_state_unref(m_state);
    m_state = xkb_state_new(keymap);
}

void Keyboard::createXKBKeymap()
{
    if (!m_context)
        return;

    struct xkb_rule_names rule_names = { strdup(qPrintable(m_keymap.rules())),
                                         strdup(qPrintable(m_keymap.model())),
                                         strdup(qPrintable(m_keymap.layout())),
                                         strdup(qPrintable(m_keymap.variant())),
                                         strdup(qPrintable(m_keymap.options())) };
    struct xkb_keymap *keymap = xkb_keymap_new_from_names(m_context, &rule_names, static_cast<xkb_keymap_compile_flags>(0));

    if (keymap) {
        createXKBState(keymap);
        xkb_keymap_unref(keymap);
    } else {
        qWarning("Failed to load the '%s' XKB keymap.", qPrintable(m_keymap.layout()));
    }

    free((char *)rule_names.rules);
    free((char *)rule_names.model);
    free((char *)rule_names.layout);
    free((char *)rule_names.variant);
    free((char *)rule_names.options);
}

void Keyboard::releaseXKB()
{
    if (!m_keymap_shared) {
        // This map was created only for this device
        if (m_keymap_area)
            munmap(m_keymap_area, m_keymap_size);
        if (m_keymap_fd >= 0)
            close(m_keymap_fd);
    }
    if (m_state)
        xkb_state_unref(m_state);
}
#endif

} // namespace QtWayland

QT_END_NAMESPACE
