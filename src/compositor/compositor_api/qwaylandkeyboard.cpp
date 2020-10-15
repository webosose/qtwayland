/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Copyright (C) 2017 Klarälvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qtwaylandcompositorglobal_p.h"
#include "qwaylandkeyboard.h"
#include "qwaylandkeyboard_p.h"
#include <QtWaylandCompositor/QWaylandKeymap>
#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandSeat>
#include <QtWaylandCompositor/QWaylandClient>

#include <QtCore/QFile>
#include <QtCore/QStandardPaths>

#include <fcntl.h>
#include <unistd.h>
#if QT_CONFIG(xkbcommon)
#include <sys/mman.h>
#include <sys/types.h>
#include <qwaylandxkb_p.h>
#endif

QT_BEGIN_NAMESPACE

#if QT_CONFIG(xkbcommon)
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

    static XKBKeymap::Data* createXKBKeymap(const QWaylandKeymap *keymap);

    const XKBKeymap::Data* defaultXKBKeymap() { return m_default_data; }

private:
    QWaylandKeymap *m_default_keymap = nullptr;
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
            m_default_keymap = new QWaylandKeymap(split[2], QString(), QString(), split[1], split[0]);
        } else {
            qWarning("Error to parse QT_WAYLAND_XKB_RULE_NAMES. Default XKB keymap will be used.");
            m_default_keymap = new QWaylandKeymap();
        }
    } else {
        qWarning("No QT_WAYLAND_XKB_RULE_NAMES set. Default XKB keymap will be used.");
        m_default_keymap = new QWaylandKeymap();
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

XKBKeymap::Data* XKBKeymap::createXKBKeymap(const QWaylandKeymap *keymap)
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
    struct xkb_rule_names rule_names = { strdup(qPrintable(keymap->rules())),
                                         strdup(qPrintable(keymap->model())),
                                         strdup(qPrintable(keymap->layout())),
                                         strdup(qPrintable(keymap->variant())),
                                         strdup(qPrintable(keymap->options())) };

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
            free(str);
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
            << "layout:"    << keymap->layout()
            << "variant:"   << keymap->variant()
            << "options:"   << keymap->options()
            << "model:"     << keymap->model()
            << "rules:"     << keymap->rules();

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
    if (m_default_keymap)
        delete m_default_keymap;
}
#endif


QWaylandKeyboardPrivate::QWaylandKeyboardPrivate(QWaylandSeat *seat)
    : seat(seat)
{
}

QWaylandKeyboardPrivate::~QWaylandKeyboardPrivate()
{
#if QT_CONFIG(xkbcommon)
    releaseXKB();
#endif
}

QWaylandKeyboardPrivate *QWaylandKeyboardPrivate::get(QWaylandKeyboard *keyboard)
{
    return keyboard->d_func();
}

void QWaylandKeyboardPrivate::checkFocusResource(Resource *keyboardResource)
{
    if (!keyboardResource || !focus)
        return;

    // this is already the current  resource, do no send enter twice
    if (focusResource == keyboardResource)
        return;

    // check if new wl_keyboard resource is from the client owning the focus surface
    if (wl_resource_get_client(focus->resource()) == keyboardResource->client()) {
        sendEnter(focus, keyboardResource);
        focusResource = keyboardResource;
    }
}

void QWaylandKeyboardPrivate::sendEnter(QWaylandSurface *surface, Resource *keyboardResource)
{
    uint32_t serial = compositor()->nextSerial();
    send_modifiers(keyboardResource->handle, serial, modsDepressed, modsLatched, modsLocked, group);
    send_enter(keyboardResource->handle, serial, surface->resource(), QByteArray::fromRawData((char *)keys.data(), keys.size() * sizeof(uint32_t)));
}

void QWaylandKeyboardPrivate::focused(QWaylandSurface *surface)
{
    if (surface && surface->isCursorSurface())
        surface = nullptr;
    if (focus != surface) {
        if (focusResource) {
            uint32_t serial = compositor()->nextSerial();
            send_leave(focusResource->handle, serial, focus->resource());
        }
        focusDestroyListener.reset();
        if (surface)
            focusDestroyListener.listenForDestruction(surface->resource());
    }

    Resource *resource = surface ? resourceMap().value(surface->waylandClient()) : 0;

    if (resource && (focus != surface || focusResource != resource))
        sendEnter(surface, resource);

    focusResource = resource;
    focus = surface;
    Q_EMIT q_func()->focusChanged(focus);
}


void QWaylandKeyboardPrivate::keyboard_bind_resource(wl_keyboard::Resource *resource)
{
    // Send repeat information
    if (resource->version() >= WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION)
        send_repeat_info(resource->handle, repeatRate, repeatDelay);

#if QT_CONFIG(xkbcommon)
    if (xkb_context) {
        send_keymap(resource->handle, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                    keymap_fd, keymap_size);
    } else
#endif
    {
        int null_fd = open("/dev/null", O_RDONLY);
        send_keymap(resource->handle, 0 /* WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP */,
                    null_fd, 0);
        close(null_fd);
    }
    checkFocusResource(resource);
}

void QWaylandKeyboardPrivate::keyboard_destroy_resource(wl_keyboard::Resource *resource)
{
    if (focusResource == resource)
        focusResource = nullptr;
}

void QWaylandKeyboardPrivate::keyboard_release(wl_keyboard::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void QWaylandKeyboardPrivate::keyEvent(uint code, uint32_t state)
{
#if QT_CONFIG(xkbcommon)
    uint key = toWaylandXkbV1Key(code);
#else
    uint key = code;
#endif
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {

#ifdef NO_WEBOS_PLATFORM
        // In webos, there is no scence to pass pressed keys when switching focus
        // Also, keymaps are always set before sendKeyEvent.
        keys << key;
#endif
    } else {
        keys.removeAll(key);
    }
}

void QWaylandKeyboardPrivate::sendKeyEvent(uint code, uint32_t state, bool repeat)
{
    uint32_t time = compositor()->currentTimeMsecs();
    uint32_t serial = compositor()->nextSerial();
#if QT_CONFIG(xkbcommon)
    uint key = toWaylandXkbV1Key(code);
#else
    uint key = code;
#endif
    if (focusResource) {
        send_key(focusResource->handle, serial, time, key, state);
        updateModifierState(code, state, repeat);
    }
}

void QWaylandKeyboardPrivate::modifiers(uint32_t serial, uint32_t mods_depressed,
                         uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
    if (focusResource) {
        send_modifiers(focusResource->handle, serial, mods_depressed, mods_latched, mods_locked, group);
    }
}

#if QT_CONFIG(xkbcommon)
void QWaylandKeyboardPrivate::maybeUpdateXkbScanCodeTable()
{
    if (!scanCodesByQtKey.isEmpty() || !xkbState())
        return;

    if (xkb_keymap *keymap = xkb_state_get_keymap(xkb_state)) {
        xkb_keymap_key_for_each(keymap, [](xkb_keymap *keymap, xkb_keycode_t keycode, void *d){
            auto *scanCodesByQtKey = static_cast<QMap<ScanCodeKey, uint>*>(d);
            uint numLayouts = xkb_keymap_num_layouts_for_key(keymap, keycode);
            for (uint layout = 0; layout < numLayouts; ++layout) {
                const xkb_keysym_t *syms = nullptr;
                xkb_keymap_key_get_syms_by_level(keymap, keycode, layout, 0, &syms);
                if (!syms)
                    continue;

                Qt::KeyboardModifiers mods = {};
                int qtKey = QWaylandXkb::keysymToQtKey(syms[0], mods).first;
                if (qtKey != 0)
                    scanCodesByQtKey->insert({layout, qtKey}, keycode);
            }
        }, &scanCodesByQtKey);
    }
}
#endif

void QWaylandKeyboardPrivate::updateModifierState(uint code, uint32_t state, bool repeat)
{
#if QT_CONFIG(xkbcommon)
    if (!xkb_context)
        return;

    // xkb needs to match a the series of calls a XKB_KEY_DOWN, needs to be matched with
    // XKB_KEY_UP, hence when repeating down update the state otherwise situations like
    // "stuck modifiers" may occur.
    if (repeat) {
        return;
    }

    xkb_state_update_key(xkb_state, code, state == WL_KEYBOARD_KEY_STATE_PRESSED ? XKB_KEY_DOWN : XKB_KEY_UP);

    uint32_t modsDepressed = xkb_state_serialize_mods(xkb_state, (xkb_state_component)XKB_STATE_DEPRESSED);
    uint32_t modsLatched = xkb_state_serialize_mods(xkb_state, (xkb_state_component)XKB_STATE_LATCHED);
    uint32_t modsLocked = xkb_state_serialize_mods(xkb_state, (xkb_state_component)XKB_STATE_LOCKED);
    uint32_t group = xkb_state_serialize_group(xkb_state, (xkb_state_component)XKB_STATE_EFFECTIVE);

    if (this->modsDepressed == modsDepressed
            && this->modsLatched == modsLatched
            && this->modsLocked == modsLocked
            && this->group == group)
        return;

    this->modsDepressed = modsDepressed;
    this->modsLatched = modsLatched;
    this->modsLocked = modsLocked;
    this->group = group;

    modifiers(compositor()->nextSerial(), modsDepressed, modsLatched, modsLocked, group);
#else
    Q_UNUSED(code);
    Q_UNUSED(state);
    Q_UNUSED(repeat);
#endif
}

void QWaylandKeyboard::updateModifierState(QWaylandKeyboard *refKeyboard)
{
    Q_D(QWaylandKeyboard);
    d->updateModifierState(refKeyboard);

}
/* Copy the target Keybord's modifier state.
 * NOTE: This it value copy of the target modifier data.  So we should update xkb_state too.
 * */
void QWaylandKeyboardPrivate::updateModifierState(QWaylandKeyboard *refKeyboard)
{
#if QT_CONFIG(xkbcommon)

    if (refKeyboard) {
        modsDepressed = xkb_state_serialize_mods(refKeyboard->d_func()->xkb_state, (xkb_state_component)XKB_STATE_MODS_DEPRESSED);
        modsLatched   = xkb_state_serialize_mods(refKeyboard->d_func()->xkb_state, (xkb_state_component)XKB_STATE_MODS_LATCHED);
        modsLocked    = xkb_state_serialize_mods(refKeyboard->d_func()->xkb_state, (xkb_state_component)XKB_STATE_MODS_LOCKED);
        group         = xkb_state_serialize_group(refKeyboard->d_func()->xkb_state, (xkb_state_component)XKB_STATE_EFFECTIVE);

        xkb_state_update_mask(xkb_state, modsDepressed, modsLatched, modsLocked, 0, 0, group);
        modifiers(compositor()->nextSerial(), modsDepressed, modsLatched, modsLocked, group);
    }
#else
    Q_UNUSED(refKeyboard);
#endif
}

// If there is no key currently pressed, update the keymap right away.
// Otherwise, delay the update when keys are released
// see http://lists.freedesktop.org/archives/wayland-devel/2013-October/011395.html
void QWaylandKeyboardPrivate::maybeUpdateKeymap()
{
    // There must be no keys pressed when changing the keymap,
    // see http://lists.freedesktop.org/archives/wayland-devel/2013-October/011395.html
    if (!pendingKeymap || !keys.isEmpty() || !seat || !seat->keymap())
        return;

    pendingKeymap = false;
#if QT_CONFIG(xkbcommon)
    // Release old keymap
    releaseXKB();

    XKBKeymap::Data *data = XKBKeymap::instance()->createXKBKeymap(seat->keymap());
    if (data) {
        xkb_context = data->context;
        keymap_fd = data->map_fd;
        keymap_size = data->map_size;
        keymap_area = data->map_area;
        xkb_state = xkb_state_new(data->map);
        m_keymap_shared = false;
        qInfo() << "New XKB keymap has been created," << this << "fd:" << keymap_fd << "size:" << keymap_size;
        delete data;
    } else {
        qWarning() << "Unable to update XKB keymap," << this;
        return;
    }

    foreach (Resource *res, resourceMap()) {
        send_keymap(res->handle, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, keymap_fd, keymap_size);
    }

    xkb_state_update_mask(xkb_state, 0, modsLatched, modsLocked, 0, 0, 0);
    if (focusResource)
        send_modifiers(focusResource->handle,
                       compositor()->nextSerial(),
                       modsDepressed,
                       modsLatched,
                       modsLocked,
                       group);
#endif
}

#if QT_CONFIG(xkbcommon)
void QWaylandKeyboardPrivate::initXKB()
{
    const XKBKeymap::Data *data = XKBKeymap::instance()->defaultXKBKeymap();

    if (data) {
        xkb_context = data->context;
        keymap_fd = data->map_fd;
        keymap_size = data->map_size;
        keymap_area = data->map_area;
        xkb_state = xkb_state_new(data->map);
        m_keymap_shared = true;
        qInfo() << "Using default XKB keymap," << this << "fd:" << keymap_fd << "size:" << keymap_size;
    }
}


void QWaylandKeyboardPrivate::createXKBState(xkb_keymap *keymap)
{
    char *keymap_str = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
    if (!keymap_str) {
        qWarning("Failed to compile global XKB keymap");
        return;
    }

    keymap_size = strlen(keymap_str) + 1;
    if (keymap_fd >= 0)
        close(keymap_fd);
    keymap_fd = createAnonymousFile(keymap_size);
    if (keymap_fd < 0) {
        qWarning("Failed to create anonymous file of size %lu", static_cast<unsigned long>(keymap_size));
        return;
    }

    keymap_area = static_cast<char *>(mmap(nullptr, keymap_size, PROT_READ | PROT_WRITE, MAP_SHARED, keymap_fd, 0));
    if (keymap_area == MAP_FAILED) {
        close(keymap_fd);
        keymap_fd = -1;
        qWarning("Failed to map shared memory segment");
        return;
    }

    strcpy(keymap_area, keymap_str);
    free(keymap_str);

    if (xkb_state)
        xkb_state_unref(xkb_state);
    xkb_state = xkb_state_new(keymap);
}

uint QWaylandKeyboardPrivate::toWaylandXkbV1Key(const uint nativeScanCode)
{
    const uint offset = 8;
    Q_ASSERT(nativeScanCode >= offset);
    return nativeScanCode - offset;
}

void QWaylandKeyboardPrivate::createXKBKeymap()
{
    if (!xkb_context)
        return;

    auto keymap = seat->keymap();
    struct xkb_rule_names rule_names = { strdup(qPrintable(keymap->rules())),
                                         strdup(qPrintable(keymap->model())),
                                         strdup(qPrintable(keymap->layout())),
                                         strdup(qPrintable(keymap->variant())),
                                         strdup(qPrintable(keymap->options())) };
    struct xkb_keymap *xkbKeymap = xkb_keymap_new_from_names(xkb_context, &rule_names, static_cast<xkb_keymap_compile_flags>(0));

    if (xkbKeymap) {
        scanCodesByQtKey.clear();
        createXKBState(xkbKeymap);
        xkb_keymap_unref(xkbKeymap);
    } else {
        qWarning("Failed to load the '%s' XKB keymap.", qPrintable(keymap->layout()));
    }

    free((char *)rule_names.rules);
    free((char *)rule_names.model);
    free((char *)rule_names.layout);
    free((char *)rule_names.variant);
    free((char *)rule_names.options);
}

void QWaylandKeyboardPrivate::releaseXKB()
{
    if (!m_keymap_shared) {
        // This map was created only for this device
        if (keymap_area)
            munmap(keymap_area, keymap_size);
        if (keymap_fd >= 0)
            close(keymap_fd);
    }
    if (xkb_state)
        xkb_state_unref(xkb_state);
}
#endif

void QWaylandKeyboardPrivate::sendRepeatInfo()
{
    Q_FOREACH (Resource *resource, resourceMap()) {
        if (resource->version() >= WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION)
            send_repeat_info(resource->handle, repeatRate, repeatDelay);
    }
}

/*!
 * \class QWaylandKeyboard
 * \inmodule QtWaylandCompositor
 * \since 5.8
 * \brief The QWaylandKeyboard class represents a keyboard device.
 *
 * This class provides access to the keyboard device in a QWaylandSeat. It corresponds to
 * the Wayland interface wl_keyboard.
 */

/*!
 * Constructs a QWaylandKeyboard for the given \a seat and with the given \a parent.
 */
QWaylandKeyboard::QWaylandKeyboard(QWaylandSeat *seat, QObject *parent)
    : QWaylandObject(* new QWaylandKeyboardPrivate(seat), parent)
{
    Q_D(QWaylandKeyboard);
    connect(&d->focusDestroyListener, &QWaylandDestroyListener::fired, this, &QWaylandKeyboard::focusDestroyed);
    auto keymap = seat->keymap();
    connect(keymap, &QWaylandKeymap::layoutChanged, this, &QWaylandKeyboard::updateKeymap);
    connect(keymap, &QWaylandKeymap::variantChanged, this, &QWaylandKeyboard::updateKeymap);
    connect(keymap, &QWaylandKeymap::optionsChanged, this, &QWaylandKeyboard::updateKeymap);
    connect(keymap, &QWaylandKeymap::rulesChanged, this, &QWaylandKeyboard::updateKeymap);
    connect(keymap, &QWaylandKeymap::modelChanged, this, &QWaylandKeyboard::updateKeymap);
#if QT_CONFIG(xkbcommon)
    d->initXKB();
#endif
}

/*!
 * Returns the seat for this QWaylandKeyboard.
 */
QWaylandSeat *QWaylandKeyboard::seat() const
{
    Q_D(const QWaylandKeyboard);
    return d->seat;
}

/*!
 * Returns the compositor for this QWaylandKeyboard.
 */
QWaylandCompositor *QWaylandKeyboard::compositor() const
{
    Q_D(const QWaylandKeyboard);
    return d->seat->compositor();
}

/*!
 * \internal
 */
void QWaylandKeyboard::focusDestroyed(void *data)
{
    Q_UNUSED(data);
    Q_D(QWaylandKeyboard);
    d->focusDestroyListener.reset();

    d->focus = nullptr;
    d->focusResource = nullptr;
}

void QWaylandKeyboard::updateKeymap()
{
    Q_D(QWaylandKeyboard);
    d->pendingKeymap = true;
    d->maybeUpdateKeymap();
}

/*!
 * Returns the client that currently has keyboard focus.
 */
QWaylandClient *QWaylandKeyboard::focusClient() const
{
    Q_D(const QWaylandKeyboard);
    if (!d->focusResource)
        return nullptr;
    return QWaylandClient::fromWlClient(compositor(), d->focusResource->client());
}

/*!
 * Sends the current key modifiers to \a client with the given \a serial.
 */
void QWaylandKeyboard::sendKeyModifiers(QWaylandClient *client, uint serial)
{
    Q_D(QWaylandKeyboard);
    QtWaylandServer::wl_keyboard::Resource *resource = d->resourceMap().value(client->client());
    if (resource)
        d->send_modifiers(resource->handle, serial, d->modsDepressed, d->modsLatched, d->modsLocked, d->group);
}

/*!
 * Sends a key press event with the key \a code to the current keyboard focus.
 */
void QWaylandKeyboard::sendKeyPressEvent(uint code, bool repeat)
{
    Q_D(QWaylandKeyboard);
    d->sendKeyEvent(code, WL_KEYBOARD_KEY_STATE_PRESSED, repeat);
}

/*!
 * Sends a key release event with the key \a code to the current keyboard focus.
 */
void QWaylandKeyboard::sendKeyReleaseEvent(uint code, bool repeat)
{
    Q_D(QWaylandKeyboard);
    d->sendKeyEvent(code, WL_KEYBOARD_KEY_STATE_RELEASED, repeat);
}

/*!
 * Returns the current repeat rate.
 */
quint32 QWaylandKeyboard::repeatRate() const
{
    Q_D(const QWaylandKeyboard);
    return d->repeatRate;
}

/*!
 * Sets the repeat rate to \a rate.
 */
void QWaylandKeyboard::setRepeatRate(quint32 rate)
{
    Q_D(QWaylandKeyboard);

    if (d->repeatRate == rate)
        return;

    d->sendRepeatInfo();

    d->repeatRate = rate;
    Q_EMIT repeatRateChanged(rate);
}

/*!
 * Returns the current repeat delay.
 */
quint32 QWaylandKeyboard::repeatDelay() const
{
    Q_D(const QWaylandKeyboard);
    return d->repeatDelay;
}

/*!
 * Sets the repeat delay to \a delay.
 */
void QWaylandKeyboard::setRepeatDelay(quint32 delay)
{
    Q_D(QWaylandKeyboard);

    if (d->repeatDelay == delay)
        return;

    d->sendRepeatInfo();

    d->repeatDelay = delay;
    Q_EMIT repeatDelayChanged(delay);
}

/*!
 * Returns the currently focused surface.
 */
QWaylandSurface *QWaylandKeyboard::focus() const
{
    Q_D(const QWaylandKeyboard);
    return d->focus;
}

/*!
 * Sets the current focus to \a surface.
 */
void QWaylandKeyboard::setFocus(QWaylandSurface *surface)
{
    Q_D(QWaylandKeyboard);
    d->focused(surface);
}

/*!
 * \internal
 */
void QWaylandKeyboard::addClient(QWaylandClient *client, uint32_t id, uint32_t version)
{
    Q_D(QWaylandKeyboard);
    d->add(client->client(), id, qMin<uint32_t>(QtWaylandServer::wl_keyboard::interfaceVersion(), version));
}

uint QWaylandKeyboard::keyToScanCode(int qtKey) const
{
    uint scanCode = 0;
#if QT_CONFIG(xkbcommon)
    Q_D(const QWaylandKeyboard);
    const_cast<QWaylandKeyboardPrivate *>(d)->maybeUpdateXkbScanCodeTable();
    scanCode = d->scanCodesByQtKey.value({d->group, qtKey}, 0);
#else
    Q_UNUSED(qtKey);
#endif
    return scanCode;
}

QT_END_NAMESPACE
