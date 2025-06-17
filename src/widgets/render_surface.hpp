// Copyright (C) 2024 MapLibre contributors
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <QtCore/QSize>
#include <QtCore/qglobal.h>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

namespace QMapLibre {

class RenderSurface {
public:
    virtual ~RenderSurface() = default;

    virtual QSize surfaceSize() const = 0;
    virtual qreal devicePixelRatio() const = 0;
    virtual quint32 defaultFramebufferObject() const = 0;
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual QWidget *ownerWidget() const { return nullptr; }
};

}