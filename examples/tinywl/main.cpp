// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helper.h"

#include <WServer>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QProcess>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>

#include <wquickbackend_p.h>
#include <qwbackend.h>
#include <qwdisplay.h>

inline QPointF getItemGlobalPosition(QQuickItem *item)
{
    auto parent = item->parentItem();
    return parent ? parent->mapToGlobal(item->position()) : item->position();
}

Helper::Helper(QObject *parent)
    : WSeatEventFilter(parent)
{

}

void Helper::stop()
{
    surfaceShellItem = nullptr;
    eventItem = nullptr;
    if (resizeEdgets != 0)
        surface->setResizeing(false);
    surface = nullptr;
    seat = nullptr;
}

void Helper::startMove(WXdgSurface *surface, QQuickItem *shell, QQuickItem *event, WSeat *seat, int serial)
{
    Q_UNUSED(serial)

    surfaceShellItem = shell;
    eventItem = event;
    this->surface = surface;
    this->seat = seat;
    resizeEdgets = {0};
    surfacePosOfStartMoveResize = getItemGlobalPosition(surfaceShellItem);
}

void Helper::startResize(WXdgSurface *surface, QQuickItem *shell, QQuickItem *event, WSeat *seat, Qt::Edges edge, int serial)
{
    Q_UNUSED(serial)
    Q_ASSERT(edge != 0);

    surfaceShellItem = shell;
    eventItem = event;
    this->surface = surface;
    this->seat = seat;
    surfacePosOfStartMoveResize = getItemGlobalPosition(surfaceShellItem);
    surfaceSizeOfstartMoveResize = surfaceShellItem->size();
    resizeEdgets = edge;

    surface->setResizeing(true);
}

bool Helper::startDemoClient(const QString &socket)
{
    QProcess waylandClientDemo;

    waylandClientDemo.setProgram("qml");
    waylandClientDemo.setArguments({SOURCE_DIR"/ClientWindow.qml", "-platform", "wayland"});

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WAYLAND_DISPLAY", socket);

    waylandClientDemo.setProcessEnvironment(env);
    return waylandClientDemo.startDetached();

    return false;
}

WSurface *Helper::getFocusSurfaceFrom(QObject *object)
{
    auto item = WSurfaceItem::fromFocusObject(object);
    return item ? item->surface() : nullptr;
}

bool Helper::eventFilter(WSeat *seat, QWindow *watched, QInputEvent *event)
{
    if (watched) {
        if (event->type() == QEvent::MouseButtonPress) {
            seat->setKeyboardFocusTarget(watched);
        } else if (event->type() == QEvent::MouseMove && !seat->focusWindow()) {
            seat->setKeyboardFocusTarget(watched);
        }
    }

    if (surfaceShellItem && seat == this->seat) {
        // for move resize
        if (Q_LIKELY(event->type() == QEvent::MouseMove)) {
            auto cursor = seat->cursor();
            Q_ASSERT(cursor);
            QMouseEvent *ev = static_cast<QMouseEvent*>(event);

            if (resizeEdgets == 0) {
                auto increment_pos = ev->globalPosition() - cursor->lastPressedPosition();
                auto new_pos = surfacePosOfStartMoveResize + surfaceShellItem->parentItem()->mapFromGlobal(increment_pos);
                surfaceShellItem->setPosition(new_pos);
            } else if (surface->isResizeing()) {
                auto increment_pos = surfaceShellItem->parentItem()->mapFromGlobal(ev->globalPosition() - cursor->lastPressedPosition());
                QRectF geo(surfacePosOfStartMoveResize, surfaceSizeOfstartMoveResize);

                if (resizeEdgets & Qt::LeftEdge)
                    geo.setLeft(geo.left() + increment_pos.x());
                if (resizeEdgets & Qt::TopEdge)
                    geo.setTop(geo.top() + increment_pos.y());

                if (resizeEdgets & Qt::RightEdge)
                    geo.setRight(geo.right() + increment_pos.x());
                if (resizeEdgets & Qt::BottomEdge)
                    geo.setBottom(geo.bottom() + increment_pos.y());

                if (surface->checkNewSize(geo.size().toSize())) {
                    surfaceShellItem->setPosition(geo.topLeft());
                    surfaceShellItem->setSize(geo.size());
                }
            }

            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            stop();
            // Don't continue delivery this mouse event, bacuse this event
            // is working for move-resize job.
            return true;
        }
    }

    return false;
}

bool Helper::eventFilter(WSeat *seat, WSurface *watched, QObject *surfaceItem, QInputEvent *event)
{
    Q_UNUSED(seat)

    if (event->type() == QEvent::MouseButtonPress) {
        // surfaceItem is qml type: XdgSurfaceItem
        auto xdgSurface = qvariant_cast<WXdgSurface*>(surfaceItem->property("surface"));
        if (!xdgSurface)
            return false;
        Q_ASSERT(xdgSurface->surface() == watched);
        if (!xdgSurface->doesNotAcceptFocus() && m_activateSurface != xdgSurface)
            if (auto item = qobject_cast<QQuickItem*>(surfaceItem))
                item->forceActiveFocus();
    } else if (event->type() == QEvent::MouseButtonRelease) {
        // surfaceItem is qml type: XdgSurfaceItem
        auto xdgSurface = qvariant_cast<WXdgSurface*>(surfaceItem->property("surface"));
        if (!xdgSurface)
            return false;
        Q_ASSERT(xdgSurface->surface() == watched);
        if (!xdgSurface->doesNotAcceptFocus())
            setActivateSurface(xdgSurface);
    }

    return false;
}

bool Helper::ignoredEventFilter(WSeat *seat, QWindow *watched, QInputEvent *event)
{
    Q_UNUSED(seat);

    if (watched && event->type() == QEvent::MouseButtonPress) {
        // clear focus
        if (auto item = qobject_cast<QQuickItem*>(watched->focusObject()))
            item->setFocus(false);
        setActivateSurface(nullptr);
    }

    return false;
}

WXdgSurface *Helper::activatedSurface() const
{
    return m_activateSurface;
}

void Helper::setActivateSurface(WXdgSurface *newActivate)
{
    if (m_activateSurface == newActivate)
        return;
    if (m_activateSurface)
        m_activateSurface->setActivate(false);
    m_activateSurface = newActivate;
    if (newActivate)
        newActivate->setActivate(true);
    Q_EMIT activatedSurfaceChanged();
}

int main(int argc, char *argv[]) {
//    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);

    WServer::initializeQPA();
    QQuickStyle::setStyle("Material");

    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine waylandEngine;
    waylandEngine.loadFromModule("Tinywl", "Main");
    WServer *server = waylandEngine.rootObjects().first()->findChild<WServer*>();
    Q_ASSERT(server);
    Q_ASSERT(server->isRunning());

    auto backend = server->findChild<WQuickBackend*>();
    Q_ASSERT(backend);

    // multi output
//    qobject_cast<QWMultiBackend*>(backend->backend())->forEachBackend([] (wlr_backend *backend, void *) {
//        if (auto x11 = QWX11Backend::from(backend))
//            x11->createOutput();
//    }, nullptr);


    return app.exec();
}
