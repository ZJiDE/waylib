// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wtypes.h>
#include <qwglobal.h>

#include <QObject>
#include <QSize>
#include <QPoint>
#include <QQmlEngine>
#include <QImage>

extern "C" {
#define static
#include <wlr/types/wlr_output.h>
#undef static
}

QT_BEGIN_NAMESPACE
class QScreen;
class QQuickWindow;
QT_END_NAMESPACE

QW_BEGIN_NAMESPACE
class QWRenderer;
class QWSwapchain;
class QWAllocator;
class QWOutput;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class QWlrootsScreen;
class QWlrootsIntegration;

class WOutputLayout;
class WCursor;
class WBackend;
class WServer;
class WOutputPrivate;
class WAYLIB_SERVER_EXPORT WOutput : public WWrapObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WOutput)
    Q_PROPERTY(bool enabled READ isEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QSize size READ effectiveSize NOTIFY effectiveSizeChanged)
    Q_PROPERTY(Transform orientation READ orientation NOTIFY orientationChanged)
    Q_PROPERTY(float scale READ scale NOTIFY scaleChanged)
    Q_PROPERTY(bool forceSoftwareCursor READ forceSoftwareCursor WRITE setForceSoftwareCursor NOTIFY forceSoftwareCursorChanged)
    QML_NAMED_ELEMENT(WaylandOutput)
    QML_UNCREATABLE("Can't create in qml")

public:
    enum Transform {
        Normal = WLR::Transform::Normal,
        R90 = WLR::Transform::R90,
        R180 = WLR::Transform::R180,
        R270 = WLR::Transform::R270,
        Flipped = WLR::Transform::Flipped,
        Flipped90 = WLR::Transform::Flipped90,
        Flipped180 = WLR::Transform::Flipped180,
        Flipped270 = WLR::Transform::Flipped270
    };
    Q_ENUM(Transform)

    explicit WOutput(QW_NAMESPACE::QWOutput *handle, WBackend *backend);
    ~WOutput();

    WBackend *backend() const;
    WServer *server() const;
    QW_NAMESPACE::QWRenderer *renderer() const;
    QW_NAMESPACE::QWSwapchain *swapchain() const;
    QW_NAMESPACE::QWAllocator *allocator() const;
    bool configureSwapchain(const QSize &size, uint32_t format,
                            QW_NAMESPACE::QWSwapchain **swapchain,
                            bool doTest = true);

    QW_NAMESPACE::QWOutput *handle() const;
    wlr_output *nativeHandle() const;

    static WOutput *fromHandle(const QW_NAMESPACE::QWOutput *handle);

    static WOutput *fromScreen(const QScreen *screen);

    Q_INVOKABLE QString name() const;
    bool isEnabled() const;
    QPoint position() const;
    QSize size() const;
    QSize transformedSize() const;
    QSize effectiveSize() const;
    Transform orientation() const;
    float scale() const;
    QImage::Format preferredReadFormat() const;

    void attach(QQuickWindow *window);
    QQuickWindow *attachedWindow() const;

    void setLayout(WOutputLayout *layout);
    WOutputLayout *layout() const;

    void addCursor(WCursor *cursor);
    void removeCursor(WCursor *cursor);
    QList<WCursor*> cursorList() const;

    Q_INVOKABLE bool setGammaLut(size_t ramp_size, uint16_t* r, uint16_t* g, uint16_t* b);
    Q_INVOKABLE bool enable(bool enabled);
    Q_INVOKABLE void enableAdaptiveSync(bool enabled);
    Q_INVOKABLE void setMode(wlr_output_mode *mode);
    Q_INVOKABLE void setCustomMode(const QSize &size, int32_t refresh);
    Q_INVOKABLE bool test();
    Q_INVOKABLE bool commit();
    Q_INVOKABLE void rollback();

    bool forceSoftwareCursor() const;
    void setForceSoftwareCursor(bool on);

Q_SIGNALS:
    void enabledChanged();
    void positionChanged(const QPoint &pos);
    void modeChanged();
    void transformedSizeChanged();
    void effectiveSizeChanged();
    void orientationChanged();
    void scaleChanged();
    void forceSoftwareCursorChanged();
    void bufferCommitted();

private:
    friend class QWlrootsIntegration;
    void setScreen(QWlrootsScreen *screen);
    QWlrootsScreen *screen() const;

    friend class WServer;
    friend class WServerPrivate;
};

WAYLIB_SERVER_END_NAMESPACE
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WOutput*)
