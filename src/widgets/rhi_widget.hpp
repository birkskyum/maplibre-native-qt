/*
 * Copyright (C) 2024 MapLibre contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <QMapLibreWidgets/Export>

#include <QMapLibre/Map>
#include <QMapLibre/Settings>

#include "render_surface.hpp"

#include <QtWidgets/QWidget>
#include <QtCore/QPointer>

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <rhi/qrhi.h>
#include <QtGui/qsurfaceformat.h>
#include <QtGui/qevent.h>
#endif

#include <memory>

QT_BEGIN_NAMESPACE
class QResizeEvent;
class QMouseEvent;
class QWheelEvent;
class QPaintEvent;
QT_END_NAMESPACE

namespace QMapLibre {

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
class Q_MAPLIBRE_WIDGETS_EXPORT RhiWidget : public QWidget, public RenderSurface {
    Q_OBJECT

public:
    explicit RhiWidget(const Settings &settings, QWidget *parent = nullptr);
    ~RhiWidget() override;

    Map *map();

    // RenderSurface implementation
    QSize surfaceSize() const override;
    qreal devicePixelRatio() const override;
    quint32 defaultFramebufferObject() const override;
    void beginFrame() override;
    void endFrame() override;
    QWidget *ownerWidget() const override { return const_cast<RhiWidget *>(this); }

signals:
    void onMouseDoubleClickEvent(QMapLibre::Coordinate coordinate);
    void onMousePressEvent(QMapLibre::Coordinate coordinate);
    void onMouseReleaseEvent(QMapLibre::Coordinate coordinate);
    void onMouseMoveEvent(QMapLibre::Coordinate coordinate);

protected:
    void resizeEvent(QResizeEvent *ev) override;
    void showEvent(QShowEvent *ev) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    void paintEvent(QPaintEvent *ev) override;

private:
    void initRhiIfNeeded();
    void releaseRhi();
    void recreateSwapChain();

    // Input helpers
    void forwardMouseEvent(QMouseEvent *event);
    void forwardWheelEvent(QWheelEvent *event);

    Settings m_settings;
    std::unique_ptr<Map> m_map;

    // QRhi objects
    std::unique_ptr<QRhi> m_rhi;
    QRhiSwapChain *m_swapchain = nullptr;
    QRhiRenderPassDescriptor *m_renderPassDesc = nullptr;
    QRhi::Implementation m_backendImpl = QRhi::Null;

    // For OpenGL backend
    const QRhiGles2NativeHandles *m_glesHandles = nullptr;

    QPointF m_lastPos;
};
#else
// Qt < 6.6 has no public QRhi. Provide a dummy typedef so that including
// this header will not break compilation, but fail at link-time if used.
class Q_MAPLIBRE_WIDGETS_EXPORT RhiWidget : public QWidget, public RenderSurface {
    Q_OBJECT
public:
    explicit RhiWidget(const Settings &, QWidget *parent = nullptr) : QWidget(parent) {
        Q_UNUSED(parent);
        qFatal("RhiWidget requires Qt 6.6 or newer");
    }
    ~RhiWidget() override = default;

    Map *map() { return nullptr; }

    // RenderSurface dummy implementation
    QSize surfaceSize() const override { return {}; }
    qreal devicePixelRatio() const override { return 1.0; }
    quint32 defaultFramebufferObject() const override { return 0; }
    void beginFrame() override {}
    void endFrame() override {}
};
#endif // Qt version check

} // namespace QMapLibre 