// Copyright (C) 2023 MapLibre contributors

// SPDX-License-Identifier: MIT

#ifndef WINDOW_H
#define WINDOW_H

#include <QMapLibreWidgets/GLWidget>
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0) && !defined(__EMSCRIPTEN__)
#include <QMapLibreWidgets/rhi_widget.hpp>
#define USE_RHI_WIDGET 1
#endif

#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

class MainWindow;

class Window : public QWidget {
    Q_OBJECT

public:
    explicit Window(MainWindow *mainWindow);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void dockUndock();

private:
#ifdef USE_RHI_WIDGET
    std::unique_ptr<QMapLibre::RhiWidget> m_glWidget{};
#else
    std::unique_ptr<QMapLibre::GLWidget> m_glWidget{};
#endif
    std::unique_ptr<QVBoxLayout> m_layout{};
    std::unique_ptr<QPushButton> m_buttonDock{};
    MainWindow *m_mainWindowRef{};
};

#endif
