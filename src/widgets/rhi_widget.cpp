/*
 * Copyright (C) 2024 MapLibre contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "rhi_widget.hpp"

#include <QtCore/QCoreApplication>
#include <QtGui/QResizeEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QVBoxLayout>
#include <QtGui/QPaintEvent>

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)

using namespace QMapLibre;

namespace {
QRhi::Implementation backendFromEnv()
{
    const QByteArray env = qgetenv("MLN_BACKEND");
    if (env == "opengl" || env == "gles" || env == "gl")
        return QRhi::OpenGLES2;
    if (env == "metal")
        return QRhi::Metal;
    if (env == "vulkan")
        return QRhi::Vulkan;
    return QRhi::Null; // Let Qt decide
}
} // namespace

RhiWidget::RhiWidget(const Settings &settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
{
    setAttribute(Qt::WA_NativeWindow);  // Ensure we get a native window handle
    setFocusPolicy(Qt::StrongFocus);
}

RhiWidget::~RhiWidget()
{
    releaseRhi();
}

void RhiWidget::initRhiIfNeeded()
{
    if (m_rhi)
        return;

    // Create the QRhi
    QRhi::Implementation impl = backendFromEnv();
    m_backendImpl = impl;

    QRhi *rhi = nullptr;
    if (impl == QRhi::Null) {
        rhi = QRhi::create(QRhi::Null, nullptr, nullptr);
    } else {
        rhi = QRhi::create(impl, nullptr, nullptr);
    }

    if (!rhi) {
        qWarning("Failed to create QRhi – falling back to OpenGL QOpenGLWidget path");
        return; // We stay without RHI -> map cannot render
    }

    m_rhi.reset(rhi);

    // Create swapchain tied to the widget's window
    recreateSwapChain();

    // Instantiate the map after RHI is ready so we know DPR
    m_map = std::make_unique<Map>(nullptr, m_settings, surfaceSize(), devicePixelRatio());
    connect(m_map.get(), &Map::needsRendering, this, [this]() { update(); });

    // Default style & location as in GLWidget::initializeGL
    m_map->setCoordinateZoom(m_settings.defaultCoordinate(), m_settings.defaultZoom());
    if (!m_settings.styles().empty()) {
        m_map->setStyleUrl(m_settings.styles().front().url);
    } else if (!m_settings.providerStyles().empty()) {
        m_map->setStyleUrl(m_settings.providerStyles().front().url);
    }
}

void RhiWidget::releaseRhi()
{
    if (m_swapchain) {
        delete m_swapchain;
        m_swapchain = nullptr;
    }
    m_rhi.reset();
}

void RhiWidget::recreateSwapChain()
{
    if (!m_rhi)
        return;

    if (m_swapchain) {
        delete m_swapchain;
        m_swapchain = nullptr;
    }

    QRhiSwapChain *sc = m_rhi->newSwapChain();
    sc->setWindow(windowHandle());
    sc->setBufferCount(2);
    sc->setFormat(QRhiSwapChain::RGBA8);
    if (!sc->create()) {
        qWarning("Failed to create QRhiSwapChain");
        delete sc;
        return;
    }
    m_swapchain = sc;
    m_renderPassDesc = m_swapchain->newCompatibleRenderPassDescriptor();
    m_swapchain->setRenderPassDescriptor(m_renderPassDesc);

    // For OpenGL backend, retrieve native handles
    if (m_backendImpl == QRhi::OpenGLES2) {
        m_glesHandles = m_rhi->nativeInterface<QRhiGles2NativeHandles>();
    } else {
        m_glesHandles = nullptr;
    }
}

Map *RhiWidget::map()
{
    initRhiIfNeeded();
    return m_map.get();
}

// RenderSurface implementation
QSize RhiWidget::surfaceSize() const
{
    return size();
}

qreal RhiWidget::devicePixelRatio() const
{
    return QWidget::devicePixelRatioF();
}

quint32 RhiWidget::defaultFramebufferObject() const
{
    if (m_glesHandles)
        return m_glesHandles->defaultFramebufferObject;
    return 0;
}

void RhiWidget::beginFrame()
{
    if (!m_rhi)
        return;

    m_rhi->beginFrame(m_swapchain);
}

void RhiWidget::endFrame()
{
    if (!m_rhi)
        return;

    m_rhi->endFrame(m_swapchain);
}

void RhiWidget::resizeEvent(QResizeEvent *ev)
{
    QWidget::resizeEvent(ev);
    if (m_swapchain) {
        m_swapchain->setWindow(windowHandle());
        m_swapchain->resize();
    }
    if (m_map)
        m_map->resize(size());
}

void RhiWidget::showEvent(QShowEvent *ev)
{
    QWidget::showEvent(ev);
    initRhiIfNeeded();
}

void RhiWidget::paintEvent(QPaintEvent *ev)
{
    Q_UNUSED(ev);

    if (!m_rhi || !m_map)
        return;

    beginFrame();

    if (m_backendImpl == QRhi::OpenGLES2) {
        // Supply FBO information to MapLibre when on GL
        m_map->setOpenGLFramebufferObject(defaultFramebufferObject(), QSize(width() * devicePixelRatio(), height() * devicePixelRatio()));
    }

    m_map->render();

    endFrame();
}

// Input forwarding (very similar to GLWidget implementation)
void RhiWidget::forwardMouseEvent(QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF position = event->position();
#else
    const QPointF position = event->localPos();
#endif

    if (!m_map)
        return;

    if (event->type() == QEvent::MouseButtonPress) {
        emit onMousePressEvent(m_map->coordinateForPixel(position));
    } else if (event->type() == QEvent::MouseMove) {
        emit onMouseMoveEvent(m_map->coordinateForPixel(position));
    } else if (event->type() == QEvent::MouseButtonRelease) {
        emit onMouseReleaseEvent(m_map->coordinateForPixel(position));
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        emit onMouseDoubleClickEvent(m_map->coordinateForPixel(position));
    }

    // Reuse the logic from GLWidgetPrivate: simple implementation here
    constexpr double zoomInScale{2.0};
    constexpr double zoomOutScale{0.5};

    if (event->type() == QEvent::MouseButtonPress) {
        m_lastPos = position;
        if (event->buttons() == Qt::LeftButton && (event->modifiers() & Qt::ShiftModifier)) {
            m_map->pitchBy(5.0); // simplistic pitch increment
        }
    } else if (event->type() == QEvent::MouseMove) {
        const QPointF delta = position - m_lastPos;
        if (!delta.isNull()) {
            if (event->buttons() == Qt::LeftButton) {
                m_map->moveBy(delta);
            } else if (event->buttons() == Qt::RightButton) {
                m_map->rotateBy(m_lastPos, position);
            }
        }
        m_lastPos = position;
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        if (event->buttons() == Qt::LeftButton) {
            m_map->scaleBy(zoomInScale, position);
        } else if (event->buttons() == Qt::RightButton) {
            m_map->scaleBy(zoomOutScale, position);
        }
    }

    event->accept();
}

void RhiWidget::forwardWheelEvent(QWheelEvent *event)
{
    if (!m_map)
        return;

    if (event->angleDelta().y() == 0) {
        return;
    }

    constexpr float wheelConstant = 1200.f;

    float factor = static_cast<float>(event->angleDelta().y()) / wheelConstant;
    if (event->angleDelta().y() < 0) {
        factor = factor > -1 ? factor : 1 / factor;
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    m_map->scaleBy(1 + factor, event->position());
#else
    m_map->scaleBy(1 + factor, event->pos());
#endif
    event->accept();
}

// Reimplement QWidget input events and forward them

#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>

void RhiWidget::mousePressEvent(QMouseEvent *event) { forwardMouseEvent(event); }
void RhiWidget::mouseReleaseEvent(QMouseEvent *event) { forwardMouseEvent(event); }
void RhiWidget::mouseMoveEvent(QMouseEvent *event) { forwardMouseEvent(event); }
void RhiWidget::wheelEvent(QWheelEvent *event) { forwardWheelEvent(event); }

#endif // Qt version check 