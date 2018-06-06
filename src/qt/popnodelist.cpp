#include "popnodelist.h"
#include "ui_popnodelist.h"

#include "activepopnode.h"
#include "clientmodel.h"
#include "init.h"
#include "guiutil.h"
#include "popnode-sync.h"
#include "popnodeconfig.h"
#include "popnodeman.h"
#include "sync.h"
#include "wallet/wallet.h"
#include "walletmodel.h"

#include <QTimer>
#include <QMessageBox>

CCriticalSection cs_popnodes;

PopnodeList::PopnodeList(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PopnodeList),
    clientModel(0),
    walletModel(0)
{
    ui->setupUi(this);

    ui->startButton->setEnabled(false);

    int columnAliasWidth = 100;
    int columnAddressWidth = 200;
    int columnProtocolWidth = 60;
    int columnStatusWidth = 80;
    int columnActiveWidth = 130;
    int columnLastSeenWidth = 130;

    ui->tableWidgetMyPopnodes->setColumnWidth(0, columnAliasWidth);
    ui->tableWidgetMyPopnodes->setColumnWidth(1, columnAddressWidth);
    ui->tableWidgetMyPopnodes->setColumnWidth(2, columnProtocolWidth);
    ui->tableWidgetMyPopnodes->setColumnWidth(3, columnStatusWidth);
    ui->tableWidgetMyPopnodes->setColumnWidth(4, columnActiveWidth);
    ui->tableWidgetMyPopnodes->setColumnWidth(5, columnLastSeenWidth);

    ui->tableWidgetPopnodes->setColumnWidth(0, columnAddressWidth);
    ui->tableWidgetPopnodes->setColumnWidth(1, columnProtocolWidth);
    ui->tableWidgetPopnodes->setColumnWidth(2, columnStatusWidth);
    ui->tableWidgetPopnodes->setColumnWidth(3, columnActiveWidth);
    ui->tableWidgetPopnodes->setColumnWidth(4, columnLastSeenWidth);

    ui->tableWidgetMyPopnodes->setContextMenuPolicy(Qt::CustomContextMenu);

    QAction *startAliasAction = new QAction(tr("Start alias"), this);
    contextMenu = new QMenu();
    contextMenu->addAction(startAliasAction);
    connect(ui->tableWidgetMyPopnodes, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(const QPoint&)));
    connect(startAliasAction, SIGNAL(triggered()), this, SLOT(on_startButton_clicked()));

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateNodeList()));
    connect(timer, SIGNAL(timeout()), this, SLOT(updateMyNodeList()));
    timer->start(1000);

    fFilterUpdated = false;
    nTimeFilterUpdated = GetTime();
    updateNodeList();
}

PopnodeList::~PopnodeList()
{
    delete ui;
}

void PopnodeList::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model) {
        // try to update list when popnode count changes
        connect(clientModel, SIGNAL(strPopnodesChanged(QString)), this, SLOT(updateNodeList()));
    }
}

void PopnodeList::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
}

void PopnodeList::showContextMenu(const QPoint &point)
{
    QTableWidgetItem *item = ui->tableWidgetMyPopnodes->itemAt(point);
    if(item) contextMenu->exec(QCursor::pos());
}

void PopnodeList::StartAlias(std::string strAlias)
{
    std::string strStatusHtml;
    strStatusHtml += "<center>Alias: " + strAlias;

    BOOST_FOREACH(CPopnodeConfig::CPopnodeEntry mne, popnodeConfig.getEntries()) {
        if(mne.getAlias() == strAlias) {
            std::string strError;
            CPopnodeBroadcast mnb;

            bool fSuccess = CPopnodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), strError, mnb);

            if(fSuccess) {
                strStatusHtml += "<br>Successfully started popnode.";
                mnodeman.UpdatePopnodeList(mnb);
                mnb.Relay();
                mnodeman.NotifyPopnodeUpdates();
            } else {
                strStatusHtml += "<br>Failed to start popnode.<br>Error: " + strError;
            }
            break;
        }
    }
    strStatusHtml += "</center>";

    QMessageBox msg;
    msg.setText(QString::fromStdString(strStatusHtml));
    msg.exec();

    updateMyNodeList(true);
}

void PopnodeList::StartAll(std::string strCommand)
{
    int nCountSuccessful = 0;
    int nCountFailed = 0;
    std::string strFailedHtml;

    BOOST_FOREACH(CPopnodeConfig::CPopnodeEntry mne, popnodeConfig.getEntries()) {
        std::string strError;
        CPopnodeBroadcast mnb;

        CTxIn txin = CTxIn(uint256S(mne.getTxHash()), uint32_t(atoi(mne.getOutputIndex().c_str())));
        CPopnode *pmn = mnodeman.Find(txin);

        if(strCommand == "start-missing" && pmn) continue;

        bool fSuccess = CPopnodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), strError, mnb);

        if(fSuccess) {
            nCountSuccessful++;
            mnodeman.UpdatePopnodeList(mnb);
            mnb.Relay();
            mnodeman.NotifyPopnodeUpdates();
        } else {
            nCountFailed++;
            strFailedHtml += "\nFailed to start " + mne.getAlias() + ". Error: " + strError;
        }
    }
    pwalletMain->Lock();

    std::string returnObj;
    returnObj = strprintf("Successfully started %d popnodes, failed to start %d, total %d", nCountSuccessful, nCountFailed, nCountFailed + nCountSuccessful);
    if (nCountFailed > 0) {
        returnObj += strFailedHtml;
    }

    QMessageBox msg;
    msg.setText(QString::fromStdString(returnObj));
    msg.exec();

    updateMyNodeList(true);
}

void PopnodeList::updateMyPopnodeInfo(QString strAlias, QString strAddr, CPopnode *pmn)
{
    LOCK(cs_mnlistupdate);
    bool fOldRowFound = false;
    int nNewRow = 0;

    for(int i = 0; i < ui->tableWidgetMyPopnodes->rowCount(); i++) {
        if(ui->tableWidgetMyPopnodes->item(i, 0)->text() == strAlias) {
            fOldRowFound = true;
            nNewRow = i;
            break;
        }
    }

    if(nNewRow == 0 && !fOldRowFound) {
        nNewRow = ui->tableWidgetMyPopnodes->rowCount();
        ui->tableWidgetMyPopnodes->insertRow(nNewRow);
    }

    QTableWidgetItem *aliasItem = new QTableWidgetItem(strAlias);
    QTableWidgetItem *addrItem = new QTableWidgetItem(pmn ? QString::fromStdString(pmn->addr.ToString()) : strAddr);
    QTableWidgetItem *protocolItem = new QTableWidgetItem(QString::number(pmn ? pmn->nProtocolVersion : -1));
    QTableWidgetItem *statusItem = new QTableWidgetItem(QString::fromStdString(pmn ? pmn->GetStatus() : "MISSING"));
    QTableWidgetItem *activeSecondsItem = new QTableWidgetItem(QString::fromStdString(DurationToDHMS(pmn ? (pmn->lastPing.sigTime - pmn->sigTime) : 0)));
    QTableWidgetItem *lastSeenItem = new QTableWidgetItem(QString::fromStdString(DateTimeStrFormat("%Y-%m-%d %H:%M", pmn ? pmn->lastPing.sigTime + QDateTime::currentDateTime().offsetFromUtc() : 0)));
    QTableWidgetItem *pubkeyItem = new QTableWidgetItem(QString::fromStdString(pmn ? CBitcoinAddress(pmn->pubKeyCollateralAddress.GetID()).ToString() : ""));

    ui->tableWidgetMyPopnodes->setItem(nNewRow, 0, aliasItem);
    ui->tableWidgetMyPopnodes->setItem(nNewRow, 1, addrItem);
    ui->tableWidgetMyPopnodes->setItem(nNewRow, 2, protocolItem);
    ui->tableWidgetMyPopnodes->setItem(nNewRow, 3, statusItem);
    ui->tableWidgetMyPopnodes->setItem(nNewRow, 4, activeSecondsItem);
    ui->tableWidgetMyPopnodes->setItem(nNewRow, 5, lastSeenItem);
    ui->tableWidgetMyPopnodes->setItem(nNewRow, 6, pubkeyItem);
}

void PopnodeList::updateMyNodeList(bool fForce)
{
    static int64_t nTimeMyListUpdated = 0;

    // automatically update my popnode list only once in MY_POPNODELIST_UPDATE_SECONDS seconds,
    // this update still can be triggered manually at any time via button click
    int64_t nSecondsTillUpdate = nTimeMyListUpdated + MY_POPNODELIST_UPDATE_SECONDS - GetTime();
    ui->secondsLabel->setText(QString::number(nSecondsTillUpdate));

    if(nSecondsTillUpdate > 0 && !fForce) return;
    nTimeMyListUpdated = GetTime();

    ui->tableWidgetPopnodes->setSortingEnabled(false);
    BOOST_FOREACH(CPopnodeConfig::CPopnodeEntry mne, popnodeConfig.getEntries()) {
        CTxIn txin = CTxIn(uint256S(mne.getTxHash()), uint32_t(atoi(mne.getOutputIndex().c_str())));
        CPopnode *pmn = mnodeman.Find(txin);

        updateMyPopnodeInfo(QString::fromStdString(mne.getAlias()), QString::fromStdString(mne.getIp()), pmn);
    }
    ui->tableWidgetPopnodes->setSortingEnabled(true);

    // reset "timer"
    ui->secondsLabel->setText("0");
}

void PopnodeList::updateNodeList()
{
    static int64_t nTimeListUpdated = GetTime();

    // to prevent high cpu usage update only once in POPNODELIST_UPDATE_SECONDS seconds
    // or POPNODELIST_FILTER_COOLDOWN_SECONDS seconds after filter was last changed
    int64_t nSecondsToWait = fFilterUpdated
                            ? nTimeFilterUpdated - GetTime() + POPNODELIST_FILTER_COOLDOWN_SECONDS
                            : nTimeListUpdated - GetTime() + POPNODELIST_UPDATE_SECONDS;

    if(fFilterUpdated) ui->countLabel->setText(QString::fromStdString(strprintf("Please wait... %d", nSecondsToWait)));
    if(nSecondsToWait > 0) return;

    nTimeListUpdated = GetTime();
    fFilterUpdated = false;

    TRY_LOCK(cs_popnodes, lockPopnodes);
    if(!lockPopnodes) return;

    QString strToFilter;
    ui->countLabel->setText("Updating...");
    ui->tableWidgetPopnodes->setSortingEnabled(false);
    ui->tableWidgetPopnodes->clearContents();
    ui->tableWidgetPopnodes->setRowCount(0);
    std::vector<CPopnode> vPopnodes = mnodeman.GetFullPopnodeVector();

    BOOST_FOREACH(CPopnode& mn, vPopnodes)
    {
        // populate list
        // Address, Protocol, Status, Active Seconds, Last Seen, Pub Key
        QTableWidgetItem *addressItem = new QTableWidgetItem(QString::fromStdString(mn.addr.ToString()));
        QTableWidgetItem *protocolItem = new QTableWidgetItem(QString::number(mn.nProtocolVersion));
        QTableWidgetItem *statusItem = new QTableWidgetItem(QString::fromStdString(mn.GetStatus()));
        QTableWidgetItem *activeSecondsItem = new QTableWidgetItem(QString::fromStdString(DurationToDHMS(mn.lastPing.sigTime - mn.sigTime)));
        QTableWidgetItem *lastSeenItem = new QTableWidgetItem(QString::fromStdString(DateTimeStrFormat("%Y-%m-%d %H:%M", mn.lastPing.sigTime + QDateTime::currentDateTime().offsetFromUtc())));
        QTableWidgetItem *pubkeyItem = new QTableWidgetItem(QString::fromStdString(CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString()));

        if (strCurrentFilter != "")
        {
            strToFilter =   addressItem->text() + " " +
                            protocolItem->text() + " " +
                            statusItem->text() + " " +
                            activeSecondsItem->text() + " " +
                            lastSeenItem->text() + " " +
                            pubkeyItem->text();
            if (!strToFilter.contains(strCurrentFilter)) continue;
        }

        ui->tableWidgetPopnodes->insertRow(0);
        ui->tableWidgetPopnodes->setItem(0, 0, addressItem);
        ui->tableWidgetPopnodes->setItem(0, 1, protocolItem);
        ui->tableWidgetPopnodes->setItem(0, 2, statusItem);
        ui->tableWidgetPopnodes->setItem(0, 3, activeSecondsItem);
        ui->tableWidgetPopnodes->setItem(0, 4, lastSeenItem);
        ui->tableWidgetPopnodes->setItem(0, 5, pubkeyItem);
    }

    ui->countLabel->setText(QString::number(ui->tableWidgetPopnodes->rowCount()));
    ui->tableWidgetPopnodes->setSortingEnabled(true);
}

void PopnodeList::on_filterLineEdit_textChanged(const QString &strFilterIn)
{
    strCurrentFilter = strFilterIn;
    nTimeFilterUpdated = GetTime();
    fFilterUpdated = true;
    ui->countLabel->setText(QString::fromStdString(strprintf("Please wait... %d", POPNODELIST_FILTER_COOLDOWN_SECONDS)));
}

void PopnodeList::on_startButton_clicked()
{
    // Find selected node alias
    QItemSelectionModel* selectionModel = ui->tableWidgetMyPopnodes->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();

    if(selected.count() == 0) return;

    QModelIndex index = selected.at(0);
    int nSelectedRow = index.row();
    std::string strAlias = ui->tableWidgetMyPopnodes->item(nSelectedRow, 0)->text().toStdString();

    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm popnode start"),
        tr("Are you sure you want to start popnode %1?").arg(QString::fromStdString(strAlias)),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if(retval != QMessageBox::Yes) return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    if(encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForMixingOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());

        if(!ctx.isValid()) return; // Unlock wallet was cancelled

        StartAlias(strAlias);
        return;
    }

    StartAlias(strAlias);
}

void PopnodeList::on_startAllButton_clicked()
{
    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm all popnodes start"),
        tr("Are you sure you want to start ALL popnodes?"),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if(retval != QMessageBox::Yes) return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    if(encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForMixingOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());

        if(!ctx.isValid()) return; // Unlock wallet was cancelled

        StartAll();
        return;
    }

    StartAll();
}

void PopnodeList::on_startMissingButton_clicked()
{

    if(!popnodeSync.IsPopnodeListSynced()) {
        QMessageBox::critical(this, tr("Command is not available right now"),
            tr("You can't use this command until popnode list is synced"));
        return;
    }

    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this,
        tr("Confirm missing popnodes start"),
        tr("Are you sure you want to start MISSING popnodes?"),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if(retval != QMessageBox::Yes) return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    if(encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForMixingOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());

        if(!ctx.isValid()) return; // Unlock wallet was cancelled

        StartAll("start-missing");
        return;
    }

    StartAll("start-missing");
}

void PopnodeList::on_tableWidgetMyPopnodes_itemSelectionChanged()
{
    if(ui->tableWidgetMyPopnodes->selectedItems().count() > 0) {
        ui->startButton->setEnabled(true);
    }
}

void PopnodeList::on_UpdateButton_clicked()
{
    updateMyNodeList(true);
}
