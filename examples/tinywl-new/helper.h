// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "qmlengine.h"

#include <wglobal.h>
#include <wqmlcreator.h>
#include <wseat.h>

#include <QObject>
#include <QQmlApplicationEngine>

Q_MOC_INCLUDE(<wtoplevelsurface.h>)
Q_MOC_INCLUDE("surfacewrapper.h")

QT_BEGIN_NAMESPACE
class QQuickItem;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WOutputRenderWindow;
class WOutputLayout;
class WCursor;
class WBackend;
class WOutputItem;
class WOutputViewport;
class WOutputLayer;
class WOutput;
class WXWayland;
class WInputMethodHelper;
class WXdgDecorationManager;
class WSocket;
class WSurface;
class WToplevelSurface;
class WSurfaceItem;
class WForeignToplevel;
WAYLIB_SERVER_END_NAMESPACE

QW_BEGIN_NAMESPACE
class qw_renderer;
class qw_allocator;
class qw_compositor;
QW_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

class Output;
class SurfaceWrapper;
class Workspace;
class RootSurfaceContainer;
class LayerSurfaceContainer;
class Helper : public WSeatEventFilter
{
    friend class RootSurfaceContainer;
    Q_OBJECT
    Q_PROPERTY(bool socketEnabled READ socketEnabled WRITE setSocketEnabled NOTIFY socketEnabledChanged FINAL)
    Q_PROPERTY(SurfaceWrapper* activatedSurface READ activatedSurface NOTIFY activatedSurfaceChanged FINAL)
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Helper(QObject *parent = nullptr);
    ~Helper();

    static Helper *instance();

    QmlEngine *qmlEngine() const;
    WOutputRenderWindow *window() const;
    void init();

    bool socketEnabled() const;
    void setSocketEnabled(bool newSocketEnabled);

    void activeSurface(SurfaceWrapper *wrapper, Qt::FocusReason reason);

public Q_SLOTS:
    void activeSurface(SurfaceWrapper *wrapper);

signals:
    void socketEnabledChanged();
    void keyboardFocusSurfaceChanged();
    void activatedSurfaceChanged();

private:
    void allowNonDrmOutputAutoChangeMode(WOutput *output);
    void enableOutput(WOutput *output);

    int indexOfOutput(WOutput *output) const;
    Output *getOutput(WOutput *output) const;
    Output *outputAt(WCursor *cursor) const;
    void setPrimaryOutput(Output *output);

    void setOutputProxy(Output *output);

    void destroySurface(WSurface *surface);
    void updateLayerSurfaceContainer(SurfaceWrapper *surface);
    void updateSurfaceOwnsOutput(SurfaceWrapper *surface);
    void updateSurfacesOwnsOutput();
    void updateSurfaceOutputs(SurfaceWrapper *surface);

    SurfaceWrapper *keyboardFocusSurface() const;
    void setKeyboardFocusSurface(SurfaceWrapper *newActivateSurface, Qt::FocusReason reason);
    SurfaceWrapper *activatedSurface() const;
    void setActivatedSurface(SurfaceWrapper *newActivateSurface);

    void setCursorPosition(const QPointF &position);
    void ensureCursorPositionValid();
    void ensureSurfaceNormalPositionValid(SurfaceWrapper *surface);

    void stopMoveResize();
    void startMove(SurfaceWrapper *surface, int serial);
    void startResize(SurfaceWrapper *surface, Qt::Edges edges, int serial);
    void cancelMoveResize(SurfaceWrapper *surface);

    bool startDemoClient();

    bool beforeDisposeEvent(WSeat *seat, QWindow *watched, QInputEvent *event) override;
    bool afterHandleEvent(WSeat *seat, WSurface *watched, QObject *surfaceItem, QObject *, QInputEvent *event) override;
    bool unacceptedEvent(WSeat *, QWindow *, QInputEvent *event) override;

    static Helper *m_instance;

    // qtquick helper
    WOutputRenderWindow *m_renderWindow = nullptr;
    WOutputLayout *m_outputLayout = nullptr;

    // wayland helper
    WServer *m_server = nullptr;
    WSocket *m_socket = nullptr;
    WCursor *m_cursor = nullptr;
    WSeat *m_seat = nullptr;
    WBackend *m_backend = nullptr;
    qw_renderer *m_renderer = nullptr;
    qw_allocator *m_allocator = nullptr;

    // protocols
    qw_compositor *m_compositor = nullptr;
    WXWayland *m_xwayland = nullptr;
    WInputMethodHelper *m_inputMethodHelper = nullptr;
    WXdgDecorationManager *m_xdgDecorationManager = nullptr;
    WForeignToplevel *m_foreignToplevel = nullptr;

    // privaet data
    QList<Output*> m_outputList;
    QPointer<Output> m_primaryOutput;

    QPointer<SurfaceWrapper> m_keyboardFocusSurface;
    QPointer<SurfaceWrapper> m_activatedSurface;

    RootSurfaceContainer *m_surfaceContainer = nullptr;
    LayerSurfaceContainer *m_backgroundContainer = nullptr;
    LayerSurfaceContainer *m_bottomContainer = nullptr;
    Workspace *m_workspace = nullptr;
    LayerSurfaceContainer *m_topContainer = nullptr;
    LayerSurfaceContainer *m_overlayContainer = nullptr;

    // for move resize
    SurfaceWrapper *m_moveResizeSurface = nullptr;
};
