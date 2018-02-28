/****************************************************************************
**
** Copyright (C) 2014 Jolla Ltd, author: <giulio.camuffo@jollamobile.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QSGTexture>
#include <QOpenGLTexture>
#include <QQuickWindow>
#include <QDebug>
#include <QQmlPropertyMap>

#include "qwaylandquicksurface.h"
#include "qwaylandquickcompositor.h"
#include "qwaylandsurfaceitem.h"
#include <QtCompositor/qwaylandbufferref.h>
#include <QtCompositor/private/qwaylandsurface_p.h>
#include <QtCompositor/private/qwaylandtexturebufferattacher_p.h>

QT_BEGIN_NAMESPACE

class QWaylandQuickSurfacePrivate : public QWaylandSurfacePrivate
{
public:
    QWaylandQuickSurfacePrivate(wl_client *client, quint32 id, int version, QWaylandQuickCompositor *c, QWaylandQuickSurface *surf)
        : QWaylandSurfacePrivate(client, id, version, c, surf)
        , compositor(c)
        , useTextureAlpha(true)
        , windowPropertyMap(new QQmlPropertyMap)
        , clientRenderingEnabled(true)
    {

    }

    ~QWaylandQuickSurfacePrivate()
    {
        windowPropertyMap->deleteLater();
        // buffer is deleted automatically by ~Surface(), since it is the assigned attacher
    }

    void surface_commit(Resource *resource) Q_DECL_OVERRIDE
    {
        QWaylandSurfacePrivate::surface_commit(resource);

        compositor->update();
    }

    QWaylandQuickCompositor *compositor;
    bool useTextureAlpha;
    QQmlPropertyMap *windowPropertyMap;
    bool clientRenderingEnabled;
};

QWaylandQuickSurface::QWaylandQuickSurface(wl_client *client, quint32 id, int version, QWaylandQuickCompositor *compositor)
                    : QWaylandSurface(new QWaylandQuickSurfacePrivate(client, id, version, compositor, this))
{
    Q_D(QWaylandQuickSurface);
    QWaylandSurface::setBufferAttacher(new QWaylandTextureBufferAttacher(this));

    QQuickWindow *window = static_cast<QQuickWindow *>(compositor->window());
    connect(window, &QQuickWindow::beforeSynchronizing, this, &QWaylandQuickSurface::updateTexture, Qt::DirectConnection);
    connect(window, &QQuickWindow::sceneGraphInvalidated, this, &QWaylandQuickSurface::invalidateTexture, Qt::DirectConnection);
    connect(this, &QWaylandSurface::windowPropertyChanged, d->windowPropertyMap, &QQmlPropertyMap::insert);
    connect(d->windowPropertyMap, &QQmlPropertyMap::valueChanged, this, &QWaylandSurface::setWindowProperty);

}

QWaylandQuickSurface::~QWaylandQuickSurface()
{

}

QWaylandTextureBufferAttacher *QWaylandQuickSurface::textureBufferAttacher() const
{
    return static_cast<QWaylandTextureBufferAttacher*>(bufferAttacher());
}

QSGTexture *QWaylandQuickSurface::texture() const
{
    return textureBufferAttacher()->texture();
}

bool QWaylandQuickSurface::useTextureAlpha() const
{
    Q_D(const QWaylandQuickSurface);
    return d->useTextureAlpha;
}

void QWaylandQuickSurface::setUseTextureAlpha(bool useTextureAlpha)
{
    Q_D(QWaylandQuickSurface);
    if (d->useTextureAlpha != useTextureAlpha) {
        d->useTextureAlpha = useTextureAlpha;
        emit useTextureAlphaChanged();
        emit configure(textureBufferAttacher()->currentBuffer());
    }
}

QObject *QWaylandQuickSurface::windowPropertyMap() const
{
    Q_D(const QWaylandQuickSurface);
    return d->windowPropertyMap;
}


void QWaylandQuickSurface::updateTexture()
{
    QWaylandTextureBufferAttacher *attacher = textureBufferAttacher();
    const bool update = attacher->isDirty();
    if (update)
        attacher->updateTexture();
    foreach (QWaylandSurfaceView *view, views())
        static_cast<QWaylandSurfaceItem *>(view)->updateTexture();
}

void QWaylandQuickSurface::invalidateTexture()
{
    textureBufferAttacher()->invalidateTexture();
    foreach (QWaylandSurfaceView *view, views())
        static_cast<QWaylandSurfaceItem *>(view)->updateTexture();
}

bool QWaylandQuickSurface::clientRenderingEnabled() const
{
    Q_D(const QWaylandQuickSurface);
    return d->clientRenderingEnabled;
}

void QWaylandQuickSurface::setClientRenderingEnabled(bool enabled)
{
    Q_D(QWaylandQuickSurface);
    if (d->clientRenderingEnabled != enabled) {
        d->clientRenderingEnabled = enabled;

        sendOnScreenVisibilityChange(enabled);

        emit clientRenderingEnabledChanged();
    }
}

QWaylandSurfaceItem *QWaylandQuickSurface::surfaceItem() {
    return views().isEmpty() ? NULL : static_cast<QWaylandSurfaceItem*>(views().first());
}

QT_END_NAMESPACE
