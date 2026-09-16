#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "appcore.h"

// Creates the platform window shell hosting the given core:
// a DTK DMainWindow (titlebar menu, translucent background) when the
// runtime is deepin/UOS and the binary was built with DTK, or a plain
// QMainWindow (menu bar) everywhere else.
QMainWindow *createMainWindow(AppCore *core);

#endif // MAINWINDOW_H
