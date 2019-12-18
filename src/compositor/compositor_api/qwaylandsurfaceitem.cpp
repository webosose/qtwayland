/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#include "qwaylandsurfaceitem.h"
#include "qwaylandquicksurface.h"
#include <QtCompositor/qwaylandcompositor.h>
#include <QtCompositor/qwaylandinput.h>

#include <QtGui/QKeyEvent>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>

#include <QtQuick/QSGSimpleTextureNode>
#include <QtQuick/QQuickWindow>

#include <QtCore/QMutexLocker>
#include <QtCore/QMutex>
#include <QtCore/QRunnable>

#include <QThread>

QT_BEGIN_NAMESPACE

QMutex *QWaylandSurfaceItem::mutex = 0;

class QWaylandSurfaceTextureProvider : public QSGTextureProvider
{
public:
    QWaylandSurfaceTextureProvider() : t(0) { }
    ~QWaylandSurfaceTextureProvider() {
        delete t;
        t = 0;
        m_ref = QWaylandBufferRef();
    }

    QSGTexture *texture() const Q_DECL_OVERRIDE
    {
        if (t)
            t->setFiltering(smooth ? QSGTexture::Linear : QSGTexture::Nearest);
        return t;
    }

    void setBufferRef(QWaylandSurfaceItem *surfaceItem, const QWaylandBufferRef &buffer)
    {
        Q_ASSERT(QThread::currentThread() == thread());

        if (m_ref == buffer)
            return;

        m_ref = buffer;
        delete t;
        t = 0;

        if (m_ref && surfaceItem->window()) {
            if (m_ref.isShm()) {
                t = surfaceItem->window()->createTextureFromImage(m_ref.image());
            } else {
                QQuickWindow::CreateTextureOptions opt = 0;
                QWaylandQuickSurface *surface = static_cast<QWaylandQuickSurface *>(surfaceItem->surface());
                if (surface && surface->useTextureAlpha()) {
                    opt |= QQuickWindow::TextureHasAlphaChannel;
                }
                t = surfaceItem->window()->createTextureFromId(m_ref.createTexture(), surface->size(), opt);
            }
            t->bind();
        }
    }

    bool smooth;
    QSGTexture *t;
    QWaylandBufferRef m_ref;
};

class QWaylandSurfaceItemCleanup : public QRunnable
{
public:
    QWaylandSurfaceItemCleanup(QWaylandSurfaceTextureProvider *p)
        : provider(p)
    {}
    void run() override {
        delete provider;
    }
    QWaylandSurfaceTextureProvider *provider;
};


QWaylandSurfaceItem::QWaylandSurfaceItem(QWaylandQuickSurface *surface, QQuickItem *parent)
    : QQuickItem(parent)
    , QWaylandSurfaceView(surface)
    , m_provider(0)
    , m_paintEnabled(true)
    , m_touchEventsEnabled(false)
    , m_resizeSurfaceToItem(false)
    , m_newTexture(false)
    , m_window(nullptr)
{
    if (!mutex)
        mutex = new QMutex;

    setFlag(ItemHasContents);

    update();

    if (surface) {
        setWidth(surface->size().width() / surface->compositor()->devicePixelRatio());
        setHeight(surface->size().height() / surface->compositor()->devicePixelRatio());
    }

    setSmooth(true);

    setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton | Qt::RightButton |
        Qt::ExtraButton1 | Qt::ExtraButton2 | Qt::ExtraButton3 | Qt::ExtraButton4 |
        Qt::ExtraButton5 | Qt::ExtraButton6 | Qt::ExtraButton7 | Qt::ExtraButton8 |
        Qt::ExtraButton9 | Qt::ExtraButton10 | Qt::ExtraButton11 |
        Qt::ExtraButton12 | Qt::ExtraButton13);
    setAcceptHoverEvents(true);
    if (surface) {
        connect(surface, &QWaylandSurface::mapped, this, &QWaylandSurfaceItem::surfaceMapped);
        connect(surface, &QWaylandSurface::unmapped, this, &QWaylandSurfaceItem::surfaceUnmapped);
        connect(surface, &QWaylandSurface::surfaceDestroyed, this, &QWaylandSurfaceItem::surfaceDestroyed);
        connect(surface, &QWaylandSurface::parentChanged, this, &QWaylandSurfaceItem::parentChanged);
        connect(surface, &QWaylandSurface::sizeChanged, this, &QWaylandSurfaceItem::updateSize);
        connect(surface, &QWaylandSurface::configure, this, &QWaylandSurfaceItem::updateBuffer);
        connect(surface, &QWaylandSurface::redraw, this, &QQuickItem::update);

        connect(this, &QQuickItem::windowChanged, this, &QWaylandSurfaceItem::bindWindow);
    }
    connect(this, &QWaylandSurfaceItem::widthChanged, this, &QWaylandSurfaceItem::updateSurfaceSize);
    connect(this, &QWaylandSurfaceItem::heightChanged, this, &QWaylandSurfaceItem::updateSurfaceSize);

    m_yInverted = surface ? surface->isYInverted() : true;
    emit yInvertedChanged();
}

QWaylandSurfaceItem::~QWaylandSurfaceItem()
{
    QMutexLocker locker(mutex);
    if (m_provider)
        m_provider->deleteLater();
}

//This is called when the item is deref from window or suface is released.
void QWaylandSurfaceItem::releaseResources()
{
    if (!window()) {
        invalidateTexture();
        return;
    }

    if (m_provider) {
        window()->scheduleRenderJob(new QWaylandSurfaceItemCleanup(m_provider),
                                    QQuickWindow::AfterSynchronizingStage);
        m_provider = nullptr;
    }
}

void QWaylandSurfaceItem::invalidateTexture()
{
    if (m_provider) {
        delete m_provider;
        m_provider = nullptr;
    }
}

bool QWaylandSurfaceItem::isYInverted() const
{
    return m_yInverted;
}

QSGTextureProvider *QWaylandSurfaceItem::textureProvider() const
{
    if (!m_provider)
        m_provider = new QWaylandSurfaceTextureProvider();
    return m_provider;
}

void QWaylandSurfaceItem::mousePressEvent(QMouseEvent *event)
{
    if (!surface())
        return;

    if (!surface()->inputRegionContains(event->pos())) {
        event->ignore();
        return;
    }

    QWaylandInputDevice *inputDevice = compositor()->inputDeviceFor(event);
    if (inputDevice->mouseFocus() != this)
        inputDevice->setMouseFocus(this, event->localPos(), event->windowPos());
    inputDevice->sendMousePressEvent(event->button(), event->localPos(), event->windowPos());
}

void QWaylandSurfaceItem::mouseMoveEvent(QMouseEvent *event)
{
    if (surface()) {
        QWaylandInputDevice *inputDevice = compositor()->inputDeviceFor(event);
        inputDevice->sendMouseMoveEvent(this, event->localPos(), event->windowPos());
    }
}

void QWaylandSurfaceItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (surface()) {
        QWaylandInputDevice *inputDevice = compositor()->inputDeviceFor(event);
        inputDevice->sendMouseReleaseEvent(event->button(), event->localPos(), event->windowPos());
    }
}

void QWaylandSurfaceItem::hoverEnterEvent(QHoverEvent *event)
{
    if (surface()) {
        if (!surface()->inputRegionContains(event->pos())) {
            event->ignore();
            return;
        }
        QWaylandInputDevice *inputDevice = compositor()->inputDeviceFor(event);
        inputDevice->sendMouseMoveEvent(this, event->pos());
    }
}

void QWaylandSurfaceItem::hoverMoveEvent(QHoverEvent *event)
{
    if (surface()) {
        if (!surface()->inputRegionContains(event->pos())) {
            event->ignore();
            return;
        }
        QWaylandInputDevice *inputDevice = compositor()->inputDeviceFor(event);
        inputDevice->sendMouseMoveEvent(this, event->pos());
    }
}

void QWaylandSurfaceItem::wheelEvent(QWheelEvent *event)
{
    if (surface()) {
        if (!surface()->inputRegionContains(event->pos())) {
            event->ignore();
            return;
        }

        QWaylandInputDevice *inputDevice = compositor()->inputDeviceFor(event);
        inputDevice->sendMouseWheelEvent(event->orientation(), event->delta());
    }
}

void QWaylandSurfaceItem::keyPressEvent(QKeyEvent *event)
{
    if (surface()) {
        QWaylandInputDevice *inputDevice = compositor()->inputDeviceFor(event);
        inputDevice->sendFullKeyEvent(event);
    }
}

void QWaylandSurfaceItem::keyReleaseEvent(QKeyEvent *event)
{
    if (surface() && hasFocus()) {
        QWaylandInputDevice *inputDevice = compositor()->inputDeviceFor(event);
        inputDevice->sendFullKeyEvent(event);
    }
}

void QWaylandSurfaceItem::touchEvent(QTouchEvent *event)
{
    if (m_touchEventsEnabled) {
        QWaylandInputDevice *inputDevice = compositor()->inputDeviceFor(event);

        if (event->type() == QEvent::TouchBegin) {
            QQuickItem *grabber = window()->mouseGrabberItem();
            if (grabber != this)
                grabMouse();
        }

        QPoint pointPos;
        const QList<QTouchEvent::TouchPoint> &points = event->touchPoints();
        if (!points.isEmpty())
            pointPos = points.at(0).pos().toPoint();

        if (event->type() == QEvent::TouchBegin && !surface()->inputRegionContains(pointPos)) {
            event->ignore();
            return;
        }

        event->accept();
        if (inputDevice->mouseFocus() != this) {
            inputDevice->setMouseFocus(this, pointPos, pointPos);
        }
        inputDevice->sendFullTouchEvent(event);

        const bool isEnd = event->type() == QEvent::TouchEnd || event->type() == QEvent::TouchCancel;
        if (isEnd && window()->mouseGrabberItem() == this)
            ungrabMouse();
    } else {
        event->ignore();
    }
}

void QWaylandSurfaceItem::mouseUngrabEvent()
{
    if (surface()) {
        QTouchEvent e(QEvent::TouchCancel);
        touchEvent(&e);
    }
}

void QWaylandSurfaceItem::takeFocus(QWaylandInputDevice *device)
{
    setFocus(true);

    if (!surface())
        return;

    QWaylandInputDevice *target = device;
    if (!target) {
        target = compositor()->defaultInputDevice();
    }
    target->setKeyboardFocus(surface());
}

void QWaylandSurfaceItem::surfaceMapped()
{
    update();
}

void QWaylandSurfaceItem::surfaceUnmapped()
{
    update();
}

void QWaylandSurfaceItem::parentChanged(QWaylandSurface *newParent, QWaylandSurface *oldParent)
{
    Q_UNUSED(oldParent);

    if (newParent) {
        setPaintEnabled(true);
        setVisible(true);
        setOpacity(1);
        setEnabled(true);
    }
}

void QWaylandSurfaceItem::updateSize()
{
    if (surface()) {
        setSize(surface()->size() / surface()->compositor()->devicePixelRatio());
    }
}

void QWaylandSurfaceItem::updateSurfaceSize()
{
    if (surface() && m_resizeSurfaceToItem) {
        surface()->requestSize(QSize(width(), height()) * surface()->compositor()->devicePixelRatio());
    }
}

void QWaylandSurfaceItem::setPos(const QPointF &pos)
{
    setPosition(pos);
}

QPointF QWaylandSurfaceItem::pos() const
{
    return position();
}

/*!
    \qmlproperty bool QtWayland::QWaylandSurfaceItem::paintEnabled

    If this property is true, the \l item is hidden, though the texture
    will still be updated. As opposed to hiding the \l item by
    setting \l{Item::visible}{visible} to false, setting this property to true
    will not prevent mouse or keyboard input from reaching \l item.
*/
bool QWaylandSurfaceItem::paintEnabled() const
{
    return m_paintEnabled;
}

void QWaylandSurfaceItem::setPaintEnabled(bool enabled)
{
    m_paintEnabled = enabled;
    update();
}

void QWaylandSurfaceItem::updateBuffer(bool hasBuffer)
{
    Q_UNUSED(hasBuffer)

    bool inv = m_yInverted;
    m_yInverted = surface()->isYInverted();
    if (inv != m_yInverted)
        emit yInvertedChanged();

    m_newTexture = true;
}

void QWaylandSurfaceItem::beforeSync()
{
    updateTexture(false);
}

void QWaylandSurfaceItem::updateTexture(bool changed)
{
    Q_UNUSED(changed);

    QWaylandQuickSurface *sf = static_cast<QWaylandQuickSurface *>(surface());
    if (!sf) {
        releaseResources();
        //For triggering updatePaintNode so that invalidate old scengraph node
        update();
        return;
    }

    if (!m_provider)
        m_provider = new QWaylandSurfaceTextureProvider();

    m_provider->smooth = smooth();
    QSGTexture *old = m_provider->t;
    sf->advance();
    // 5.12 Style, the provider will hold the buffer reference to keep the texture valid
    m_provider->setBufferRef(this, sf->currentBuffer());

    if (m_newTexture || old != m_provider->t)
        emit m_provider->textureChanged();

    m_newTexture = false;
}

QSGNode *QWaylandSurfaceItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    bool mapped = surface() && surface()->isMapped();

    if (!mapped || !m_provider || !m_provider->t || !m_paintEnabled) {
        delete oldNode;
        return 0;
    }

    QSGSimpleTextureNode *node = static_cast<QSGSimpleTextureNode *>(oldNode);

    if (!node) {
        node = new QSGSimpleTextureNode();
        node->setFiltering(smooth() ? QSGTexture::Linear : QSGTexture::Nearest);
    }
    node->setTexture(m_provider->t);
    // Surface textures come by default with the OpenGL coordinate system, which is inverted relative
    // to the QtQuick one. So we're dealing with a double invertion here, and if isYInverted() returns
    // true it means it is NOT inverted relative to QtQuick, while if it returns false it means it IS.
    if (surface()->isYInverted()) {
        node->setRect(0, 0, width(), height());
    } else {
        node->setRect(0, height(), width(), -height());
    }

    return node;
}

void QWaylandSurfaceItem::setTouchEventsEnabled(bool enabled)
{
    if (m_touchEventsEnabled != enabled) {
        m_touchEventsEnabled = enabled;
        emit touchEventsEnabledChanged();
    }
}

void QWaylandSurfaceItem::setResizeSurfaceToItem(bool enabled)
{
    if (m_resizeSurfaceToItem != enabled) {
        m_resizeSurfaceToItem = enabled;
        emit resizeSurfaceToItemChanged();
    }
}

void QWaylandSurfaceItem::bindWindow(QQuickWindow *window)
{
    //This will not happen
    if (m_window == window)
        return;

    //Do nothing if new window is null, namely item is detached from window.
    //In current design, item should be connected to window so that
    //related surface handles requests from client, including to send buffer_release.
    if (!window)
        return;

    if (m_window) {
        disconnect(m_window, &QQuickWindow::beforeSynchronizing, this, &QWaylandSurfaceItem::beforeSync);
        disconnect(m_window, &QQuickWindow::sceneGraphInvalidated, this, &QWaylandSurfaceItem::invalidateTexture);
    }

    m_window = window;

    if (m_window) {
        connect(m_window, &QQuickWindow::beforeSynchronizing, this, &QWaylandSurfaceItem::beforeSync, Qt::DirectConnection);
        connect(m_window, &QQuickWindow::sceneGraphInvalidated, this, &QWaylandSurfaceItem::invalidateTexture, Qt::DirectConnection);
    }
}

QT_END_NAMESPACE
