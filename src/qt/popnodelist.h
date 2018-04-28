#ifndef POPNODELIST_H
#define POPNODELIST_H

#include "popnode.h"
#include "platformstyle.h"
#include "sync.h"
#include "util.h"

#include <QMenu>
#include <QTimer>
#include <QWidget>

#define MY_POPNODELIST_UPDATE_SECONDS                 60
#define POPNODELIST_UPDATE_SECONDS                    15
#define POPNODELIST_FILTER_COOLDOWN_SECONDS            3

namespace Ui {
    class PopnodeList;
}

class ClientModel;
class WalletModel;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Popnode Manager page widget */
class PopnodeList : public QWidget
{
    Q_OBJECT

public:
    explicit PopnodeList(const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~PopnodeList();

    void setClientModel(ClientModel *clientModel);
    void setWalletModel(WalletModel *walletModel);
    void StartAlias(std::string strAlias);
    void StartAll(std::string strCommand = "start-all");

private:
    QMenu *contextMenu;
    int64_t nTimeFilterUpdated;
    bool fFilterUpdated;

public Q_SLOTS:
    void updateMyPopnodeInfo(QString strAlias, QString strAddr, CPopnode *pmn);
    void updateMyNodeList(bool fForce = false);
    void updateNodeList();

Q_SIGNALS:

private:
    QTimer *timer;
    Ui::PopnodeList *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;
    CCriticalSection cs_mnlistupdate;
    QString strCurrentFilter;

private Q_SLOTS:
    void showContextMenu(const QPoint &);
    void on_filterLineEdit_textChanged(const QString &strFilterIn);
    void on_startButton_clicked();
    void on_startAllButton_clicked();
    void on_startMissingButton_clicked();
    void on_tableWidgetMyPopnodes_itemSelectionChanged();
    void on_UpdateButton_clicked();
};
#endif // POPNODELIST_H
