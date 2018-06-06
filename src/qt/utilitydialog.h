// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef BITCOIN_QT_UTILITYDIALOG_H
#define BITCOIN_QT_UTILITYDIALOG_H

#include <QDialog>
#include <QObject>

class BitcoinGUI;
class ClientModel;

namespace Ui {
    class HelpMessageDialog;
}

/** "Help message" dialog box */
class HelpMessageDialog : public QDialog
{
    Q_OBJECT

public:
    enum HelpMode {
        about,
        cmdline,
        pshelp
    };

    explicit HelpMessageDialog(QWidget *parent, HelpMode helpMode);
    ~HelpMessageDialog();

    void printToConsole();
    void showOrPrint();

private:
    Ui::HelpMessageDialog *ui;
    QString text;

private Q_SLOTS:
    void on_okButton_accepted();
};


/** "Shutdown" window */
class ShutdownWindow : public QWidget
{
    Q_OBJECT

public:
    ShutdownWindow(QWidget *parent=0, Qt::WindowFlags f=0);
    static void showShutdownWindow(BitcoinGUI *window);

protected:
    void closeEvent(QCloseEvent *event);
};

#endif // BITCOIN_QT_UTILITYDIALOG_H
