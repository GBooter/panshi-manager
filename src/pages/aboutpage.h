// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "basepage.h"
DWIDGET_USE_NAMESPACE

class AboutPage : public BasePage
{
    Q_OBJECT
public:
    explicit AboutPage(QWidget *parent = nullptr);
};
