// Copyright (C) 2023-2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxwaylandsurfaceitem.h"
#include "wsurfaceitem_p.h"
#include "wxwaylandsurface.h"
#include "wxwayland.h"

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXWaylandSurfaceItemPrivate : public WSurfaceItemPrivate
{
    Q_DECLARE_PUBLIC(WXWaylandSurfaceItem)
public:
    void configureSurfaceSize(const QSize &newSize);
    void configureSurfacePosition(const QPoint &newPosition);
    QSize expectSurfaceSize() const;
    QPoint explicitSurfacePosition() const;
    static inline WXWaylandSurfaceItemPrivate *get(WXWaylandSurfaceItem *qq) {
        return qq->d_func();
    }

public:
    QPointF surfacePosition;
    QPointer<WXWaylandSurfaceItem> parentSurfaceItem;
    QSize minimumSize;
    QSize maximumSize;
};

void WXWaylandSurfaceItemPrivate::configureSurfaceSize(const QSize &newSize)
{
    Q_Q(WXWaylandSurfaceItem);
    if (!q->isVisible())
        return;
    q->xwaylandSurface()->configureSize(newSize.width(), newSize.height());
    q->updateSurfaceState();
}

void WXWaylandSurfaceItemPrivate::configureSurfacePosition(const QPoint &newPosition)
{
    Q_Q(WXWaylandSurfaceItem);
    if (!q->isVisible())
        return;
    q->xwaylandSurface()->configurePosition(newPosition.x(), newPosition.y());
    q->updateSurfaceState();
}


QSize WXWaylandSurfaceItemPrivate::expectSurfaceSize() const
{
    const Q_Q(WXWaylandSurfaceItem);
    using enum WXWaylandSurfaceItem::ResizeMode;

    if (q->resizeMode() == SizeFromSurface) {
        const bool useRequestSize = !q->xwaylandSurface()->isBypassManager()
            && q->xwaylandSurface()->requestConfigureFlags()
                   .testAnyFlags(WXWaylandSurface::XCB_CONFIG_WINDOW_SIZE);
        return useRequestSize
            ? q->xwaylandSurface()->requestConfigureGeometry().size()
            : q->xwaylandSurface()->geometry().size();
    } else if (q->resizeMode() == SizeToSurface) {
        return q->getContentSize().toSize();
    }

    return q->xwaylandSurface()->geometry().size();
}

QPoint WXWaylandSurfaceItemPrivate::explicitSurfacePosition() const
{
    const Q_Q(WXWaylandSurfaceItem);

    QPointF pos = surfacePosition;
    const qreal ssr = parentSurfaceItem ? parentSurfaceItem->surfaceSizeRatio() : 1.0;
    const auto pt = q->parentItem();
    if (pt && !qFuzzyCompare(ssr, 1.0)) {
        const QPointF poffset(parentSurfaceItem->leftPadding(), parentSurfaceItem->topPadding());
        pos = pt->mapToItem(parentSurfaceItem, pos) - poffset;
        pos = pt->mapFromItem(parentSurfaceItem, pos * ssr + poffset);
    }

    return (pos + QPointF(q->leftPadding(), q->topPadding())).toPoint();
}

WXWaylandSurfaceItem::WXWaylandSurfaceItem(QQuickItem *parent)
    : WSurfaceItem(*new WXWaylandSurfaceItemPrivate(), parent)
{

}

WXWaylandSurfaceItem::~WXWaylandSurfaceItem()
{

}

WXWaylandSurface *WXWaylandSurfaceItem::xwaylandSurface() const
{
    return qobject_cast<WXWaylandSurface*>(shellSurface());
}

bool WXWaylandSurfaceItem::setShellSurface(WToplevelSurface *surface)
{
    Q_D(WXWaylandSurfaceItem);

    if (!WSurfaceItem::setShellSurface(surface))
        return false;

    if (surface) {
        Q_ASSERT(surface->surface());
        surface->safeConnect(&WXWaylandSurface::surfaceChanged, this, [this] {
            WSurfaceItem::setSurface(xwaylandSurface()->surface());
        });

        auto updateGeometry = [this, d] {
            const auto rm = resizeMode();
            if (rm != SizeFromSurface)
                return;
            if (!isVisible())
                return;

            updateSurfaceState();

            if (rm == SizeFromSurface) {
                resize(rm);
            }
        };
        xwaylandSurface()->safeConnect(&WXWaylandSurface::requestConfigure, this, [this] {
            if (xwaylandSurface()->requestConfigureFlags().testAnyFlags(
                    WXWaylandSurface::ConfigureFlag::XCB_CONFIG_WINDOW_POSITION)) {
                Q_EMIT implicitPositionChanged();
            }
        });
        xwaylandSurface()->safeConnect(&WXWaylandSurface::geometryChanged, this, updateGeometry);
        connect(this, &WXWaylandSurfaceItem::topPaddingChanged,
               this, &WXWaylandSurfaceItem::updatePosition, Qt::UniqueConnection);
        connect(this, &WXWaylandSurfaceItem::leftPaddingChanged,
               this, &WXWaylandSurfaceItem::updatePosition, Qt::UniqueConnection);
    }
    return true;
}

WXWaylandSurfaceItem *WXWaylandSurfaceItem::parentSurfaceItem() const
{
    const Q_D(WXWaylandSurfaceItem);

    return d->parentSurfaceItem;
}

void WXWaylandSurfaceItem::setParentSurfaceItem(WXWaylandSurfaceItem *newParentSurfaceItem)
{
    Q_D(WXWaylandSurfaceItem);

    if (d->parentSurfaceItem == newParentSurfaceItem)
        return;
    if (d->parentSurfaceItem) {
        d->parentSurfaceItem->disconnect(this);
    }

    d->parentSurfaceItem = newParentSurfaceItem;
    Q_EMIT parentSurfaceItemChanged();

    if (d->parentSurfaceItem)
        connect(d->parentSurfaceItem, &WSurfaceItem::surfaceSizeRatioChanged, this, &WXWaylandSurfaceItem::updatePosition);
    updatePosition();
}

QSize WXWaylandSurfaceItem::maximumSize() const
{
    const Q_D(WXWaylandSurfaceItem);

    return d->maximumSize;
}

QSize WXWaylandSurfaceItem::minimumSize() const
{
    const Q_D(WXWaylandSurfaceItem);

    return d->minimumSize;
}

void WXWaylandSurfaceItem::moveTo(const QPointF &pos, bool configSurface)
{
    Q_D(WXWaylandSurfaceItem);
    if (d->surfacePosition == pos)
        return;
    d->surfacePosition = pos;
    if (configSurface)
        updatePosition();
}

QPointF WXWaylandSurfaceItem::implicitPosition() const
{
    const Q_D(WXWaylandSurfaceItem);

    auto xwaylandSurface = qobject_cast<WXWaylandSurface *>(d->shellSurface);

    const QPoint epos = xwaylandSurface->requestConfigureGeometry().topLeft();
    QPointF pos = epos;

    const qreal ssr = d->parentSurfaceItem ? d->parentSurfaceItem->surfaceSizeRatio() : 1.0;
    const auto pt = parentItem();
    if (pt && !qFuzzyCompare(ssr, 1.0)) {
        auto *pd = WXWaylandSurfaceItemPrivate::get(d->parentSurfaceItem);
        const auto pepos = pd->surfacePosition;
        pos = pepos + (epos - pepos) / ssr;
    }

    return pos - QPointF(leftPadding(), topPadding());
}


void WXWaylandSurfaceItem::onSurfaceCommit()
{
    Q_D(WXWaylandSurfaceItem);
    WSurfaceItem::onSurfaceCommit();

    QSize minSize = xwaylandSurface()->minSize();
    if (!minSize.isValid())
        minSize = QSize(0, 0);

    QSize maxSize = xwaylandSurface()->maxSize();
    if (maxSize.isValid())
        maxSize = QSize(INT_MAX, INT_MAX);

    if (d->minimumSize != minSize) {
        d->minimumSize = minSize;
        Q_EMIT minimumSizeChanged();
    }

    if (d->maximumSize != maxSize) {
        d->maximumSize = maxSize;
        Q_EMIT maximumSizeChanged();
    }
}

void WXWaylandSurfaceItem::initSurface()
{
    Q_D(WXWaylandSurfaceItem);
    WSurfaceItem::initSurface();
    Q_ASSERT(xwaylandSurface());
    connect(xwaylandSurface(), &WWrapObject::aboutToBeInvalidated,
            this, &WXWaylandSurfaceItem::releaseResources);
}

bool WXWaylandSurfaceItem::doResizeSurface(const QSize &newSize)
{
    Q_D(WXWaylandSurfaceItem);

    d->configureSurfaceSize(newSize);
    return true;
}

QRectF WXWaylandSurfaceItem::getContentGeometry() const
{
    return xwaylandSurface()->getContentGeometry();
}

QSizeF WXWaylandSurfaceItem::getContentSize() const
{
    return (size() - QSizeF(leftPadding() + rightPadding(), topPadding() + bottomPadding())) * surfaceSizeRatio();
}

void WXWaylandSurfaceItem::updatePosition()
{
    Q_D(WXWaylandSurfaceItem);
    d->configureSurfacePosition(d->explicitSurfacePosition());
}

WAYLIB_SERVER_END_NAMESPACE
