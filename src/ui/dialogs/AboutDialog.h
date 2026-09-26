#pragma once

#include <QDialog>

class QLabel;

// Simple about box built from compiled-in app info.
class AboutDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AboutDialog(QWidget *parent = nullptr);
};