// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wtexture.h"
#include "utils/wtools.h"

#include <qwtexture.h>

extern "C" {
#include <wlr/render/wlr_texture.h>
#define static
#include <wlr/render/gles2.h>
#undef static
#include <wlr/render/pixman.h>
#ifdef ENABLE_VULKAN_RENDER
#include <wlr/render/vulkan.h>
#endif
}

#include <private/qsgplaintexture_p.h>
#include <private/qrhi_p.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WTexturePrivate : public WObjectPrivate {
public:
    WTexturePrivate(WTexture *qq, QWTexture *handle);

    void init(QWTexture *handle);
    void updateGLTexture() {
        if (!window)
            return;

        wlr_gles2_texture_attribs attribs;
        wlr_gles2_texture_get_attribs(handle->handle(), &attribs);

        #define GL_TEXTURE_EXTERNAL_OES           0x8D65
        QQuickWindowPrivate::TextureFromNativeTextureFlags flags = attribs.target == GL_TEXTURE_EXTERNAL_OES
                                                                       ? QQuickWindowPrivate::NativeTextureIsExternalOES
                                                                       : QQuickWindowPrivate::TextureFromNativeTextureFlags {};
        texture->setTextureFromNativeTexture(QQuickWindowPrivate::get(window)->rhi,
                                             attribs.tex, 0, 0, QSize(handle->handle()->width, handle->handle()->height),
                                             {}, flags);

        texture->setHasAlphaChannel(attribs.has_alpha);
        texture->setTextureSize(QSize(handle->handle()->width, handle->handle()->height));
        texture->setOwnsTexture(false);
    }

#ifdef ENABLE_VULKAN_RENDER
    void updateVKTexture() {
        if (!window)
            return;

        wlr_vk_image_attribs attribs;
        wlr_vk_texture_get_image_attribs(handle->handle(), &attribs);

        texture->setTextureFromNativeTexture(QQuickWindowPrivate::get(window)->rhi,
                                             reinterpret_cast<quintptr>(attribs.image),
                                             attribs.layout, attribs.format,
                                             QSize(handle->handle()->width, handle->handle()->height),
                                             {}, {});
        texture->setHasAlphaChannel(wlr_vk_texture_has_alpha(handle->handle()));
        texture->setTextureSize(QSize(handle->handle()->width, handle->handle()->height));
    }
#endif

    void updateImage() {
        auto image = wlr_pixman_texture_get_image(handle->handle());
        texture->setImage(WTools::fromPixmanImage(image));
    }

    W_DECLARE_PUBLIC(WTexture)

    QWTexture *handle;
    WTexture::Type type;

    QScopedPointer<QSGPlainTexture> texture;
    void(WTexturePrivate::*onWlrTextureChanged)();

    QQuickWindow *window = nullptr;
};

WTexturePrivate::WTexturePrivate(WTexture *qq, QWTexture *handle)
    : WObjectPrivate(qq)
    , handle(handle)
    , onWlrTextureChanged(nullptr)
{
    if (handle)
        init(handle);
}

void WTexturePrivate::init(QWTexture *handle)
{
    auto gpuTexture = new QSGPlainTexture();
    gpuTexture->setOwnsTexture(false);
    texture.reset(gpuTexture);

    if (wlr_texture_is_gles2(handle->handle())) {
        type = WTexture::Type::GLTexture;
        onWlrTextureChanged = &WTexturePrivate::updateGLTexture;
    } else if (wlr_texture_is_pixman(handle->handle())) {
        type = WTexture::Type::Image;
        onWlrTextureChanged = &WTexturePrivate::updateImage;
    }
#ifdef ENABLE_VULKAN_RENDER
    else if (wlr_texture_is_vk(handle->handle())) {
        type = WTexture::Type::VKTexture;
        onWlrTextureChanged = &WTexturePrivate::updateVKTexture;
    }
#endif
    else {
        type = WTexture::Type::Unknow;
    }
}

WTexture::WTexture(QWTexture *handle)
    : WObject(*new WTexturePrivate(this, handle))
{

}

QWTexture *WTexture::handle() const
{
    W_DC(WTexture);
    return d->handle;
}

void WTexture::setHandle(QWTexture *handle)
{
    W_D(WTexture);

    auto new_handle = reinterpret_cast<QWTexture*>(handle);

    if (Q_UNLIKELY(!new_handle)) {
        d->handle = nullptr;
        return;
    }

    if (Q_UNLIKELY(!d->handle)) {
        d->init(new_handle);
    }

    d->handle = new_handle;

    if (Q_LIKELY(d->onWlrTextureChanged))
        (d->*(d->onWlrTextureChanged))();
}

WTexture::Type WTexture::type() const
{
    W_DC(WTexture);
    return d->type;
}

QSize WTexture::size() const
{
    W_DC(WTexture);
    return QSize(d->handle->handle()->width, d->handle->handle()->height);
}

QSGTexture *WTexture::getSGTexture(QQuickWindow *window)
{
    W_D(WTexture);

    const auto oldWindow = d->window;
    d->window = window;
    if (Q_UNLIKELY(!d->texture || window != oldWindow)) {
        if (Q_LIKELY(d->onWlrTextureChanged))
            (d->*(d->onWlrTextureChanged))();
    }
    return d->texture.get();
}

WAYLIB_SERVER_END_NAMESPACE
