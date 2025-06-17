// Copyright (C) 2024 MapLibre contributors
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <QtCore/QSize>
#include <QtCore/qglobal.h>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

namespace QMapLibre {

//! \brief Small abstraction around the native drawing surface used by MapLibre.
//!
//! The current MapLibre Qt integration needs only two things:
//!   * the size/device-pixel-ratio of the surface, and
//!   * the native OpenGL framebuffer object (when the backend is OpenGL).
//!
//! Backends such as Metal or Vulkan do not (necessarily) expose an FBO.  In
//! that situation \c defaultFramebufferObject() returns 0 and the renderer is
//! expected to use a backend-specific path.
class RenderSurface {
public:
    virtual ~RenderSurface() = default;

    //! Returns the size in device-independent pixels.
    virtual QSize surfaceSize() const = 0;
    //! Returns the device pixel ratio of the surface (typically 1 or 2).
    virtual qreal devicePixelRatio() const = 0;

    //! Returns the OpenGL default FBO when available or 0 when not applicable.
    virtual quint32 defaultFramebufferObject() const = 0;

    //! Called before MapLibre starts issuing draw commands.
    virtual void beginFrame() = 0;
    //! Called when MapLibre has finished rendering.
    virtual void endFrame() = 0;

    //! Convenience helper to get the Qt widget that owns the surface – can be
    //! nullptr when the surface is not widget-backed.
    virtual QWidget *ownerWidget() const { return nullptr; }
};

} // namespace QMapLibre 