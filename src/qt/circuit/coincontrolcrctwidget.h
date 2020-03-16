// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COINCONTROLCRCTWIDGET_H
#define COINCONTROLCRCTWIDGET_H

#include <QDialog>

namespace Ui {
class CoinControlCrctWidget;
}

class CoinControlCrctWidget : public QDialog
{
    Q_OBJECT

public:
    explicit CoinControlCrctWidget(QWidget *parent = nullptr);
    ~CoinControlCrctWidget();

private:
    Ui::CoinControlCrctWidget *ui;
};

#endif // COINCONTROLCRCTWIDGET_H
