// Copyright (c) 2017-2019 The CIRCUIT developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zcrctcontroldialog.h"
#include "ui_zcrctcontroldialog.h"

#include "zcrct/accumulators.h"
#include "main.h"
#include "walletmodel.h"

using namespace std;
using namespace libzerocoin;

std::set<std::string> ZCrctControlDialog::setSelectedMints;
std::set<CMintMeta> ZCrctControlDialog::setMints;

bool CZCrctControlWidgetItem::operator<(const QTreeWidgetItem &other) const {
    int column = treeWidget()->sortColumn();
    if (column == ZCrctControlDialog::COLUMN_DENOMINATION || column == ZCrctControlDialog::COLUMN_VERSION || column == ZCrctControlDialog::COLUMN_CONFIRMATIONS)
        return data(column, Qt::UserRole).toLongLong() < other.data(column, Qt::UserRole).toLongLong();
    return QTreeWidgetItem::operator<(other);
}


ZCrctControlDialog::ZCrctControlDialog(QWidget *parent) :
    QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint),
    ui(new Ui::ZCrctControlDialog),
    model(0)
{
    ui->setupUi(this);
    setMints.clear();
    privacyDialog = (PrivacyDialog*)parent;

    // click on checkbox
    connect(ui->treeWidget, SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(updateSelection(QTreeWidgetItem*, int)));

    // push select/deselect all button
    connect(ui->pushButtonAll, SIGNAL(clicked()), this, SLOT(ButtonAllClicked()));
}

ZCrctControlDialog::~ZCrctControlDialog()
{
    delete ui;
}

void ZCrctControlDialog::setModel(WalletModel *model)
{
    this->model = model;
    updateList();
}


//Update the tree widget
void ZCrctControlDialog::updateList()
{
    // need to prevent the slot from being called each time something is changed
    ui->treeWidget->blockSignals(true);
    ui->treeWidget->clear();

    // add a top level item for each denomination
    QFlags<Qt::ItemFlag> flgTristate = Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsTristate;
    map<libzerocoin::CoinDenomination, int> mapDenomPosition;
    for (auto denom : libzerocoin::zerocoinDenomList) {
        CZCrctControlWidgetItem* itemDenom(new CZCrctControlWidgetItem);
        ui->treeWidget->addTopLevelItem(itemDenom);

        //keep track of where this is positioned in tree widget
        mapDenomPosition[denom] = ui->treeWidget->indexOfTopLevelItem(itemDenom);

        itemDenom->setFlags(flgTristate);
        itemDenom->setText(COLUMN_DENOMINATION, QString::number(denom));
        itemDenom->setData(COLUMN_DENOMINATION, Qt::UserRole, QVariant((qlonglong) denom));
    }

    // select all unused coins - including not mature and mismatching seed. Update status of coins too.
    std::set<CMintMeta> set;
    model->listZerocoinMints(set, true, false, true, true);
    this->setMints = set;

    //populate rows with mint info
    int nBestHeight = chainActive.Height();
    //map<CoinDenomination, int> mapMaturityHeight = GetMintMaturityHeight();
    for (const CMintMeta& mint : setMints) {
        // assign this mint to the correct denomination in the tree view
        libzerocoin::CoinDenomination denom = mint.denom;
        CZCrctControlWidgetItem *itemMint = new CZCrctControlWidgetItem(ui->treeWidget->topLevelItem(mapDenomPosition.at(denom)));

        // if the mint is already selected, then it needs to have the checkbox checked
        std::string strPubCoinHash = mint.hashPubcoin.GetHex();

        if (setSelectedMints.count(strPubCoinHash))
            itemMint->setCheckState(COLUMN_CHECKBOX, Qt::Checked);
        else
            itemMint->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);

        itemMint->setText(COLUMN_DENOMINATION, QString::number(mint.denom));
        itemMint->setData(COLUMN_DENOMINATION, Qt::UserRole, QVariant((qlonglong) denom));
        itemMint->setText(COLUMN_PUBCOIN, QString::fromStdString(strPubCoinHash));
        itemMint->setText(COLUMN_VERSION, QString::number(mint.nVersion));
        itemMint->setData(COLUMN_VERSION, Qt::UserRole, QVariant((qlonglong) mint.nVersion));

        int nConfirmations = (mint.nHeight ? nBestHeight - mint.nHeight : 0);
        if (nConfirmations < 0) {
            // Sanity check
            nConfirmations = 0;
        }

        itemMint->setText(COLUMN_CONFIRMATIONS, QString::number(nConfirmations));
        itemMint->setData(COLUMN_CONFIRMATIONS, Qt::UserRole, QVariant((qlonglong) nConfirmations));

        {
            LOCK(pwalletMain->zcrctTracker->cs_spendcache);

            CoinWitnessData *witnessData = pwalletMain->zcrctTracker->GetSpendCache(mint.hashStake);
            if (witnessData->nHeightAccStart > 0  && witnessData->nHeightAccEnd > 0) {
                int nPercent = std::max(0, std::min(100, (int)((double)(witnessData->nHeightAccEnd - witnessData->nHeightAccStart) / (double)(nBestHeight - witnessData->nHeightAccStart - 220) * 100)));
                QString percent = QString::number(nPercent) + QString("%");
                itemMint->setText(COLUMN_PRECOMPUTE, percent);
            } else {
                itemMint->setText(COLUMN_PRECOMPUTE, QString("0%"));
            }
        }

        // check for maturity
        // Always mature, public spends doesn't require any new accumulation.
        bool isMature = true;
        //if (mapMaturityHeight.count(mint.denom))
        //    isMature = mint.nHeight < mapMaturityHeight.at(denom);

        // disable selecting this mint if it is not spendable - also display a reason why
        bool fSpendable = isMature && nConfirmations >= Params().Zerocoin_MintRequiredConfirmations() && mint.isSeedCorrect;
        if(!fSpendable) {
            itemMint->setDisabled(true);
            itemMint->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);

            //if this mint is in the selection list, then remove it
            if (setSelectedMints.count(strPubCoinHash))
                setSelectedMints.erase(strPubCoinHash);

            string strReason = "";
            if(nConfirmations < Params().Zerocoin_MintRequiredConfirmations())
                strReason = strprintf("Needs %d more confirmations", Params().Zerocoin_MintRequiredConfirmations() - nConfirmations);
            else if (model->getEncryptionStatus() == WalletModel::EncryptionStatus::Locked)
                strReason = "Your wallet is locked. Impossible to precompute or spend zCRCT.";
            else if (!mint.isSeedCorrect)
                strReason = "The zCRCT seed used to mint this zCRCT is not the same as currently hold in the wallet";
            else
                strReason = strprintf("Needs %d more mints added to network", Params().Zerocoin_RequiredAccumulation());

            itemMint->setText(COLUMN_ISSPENDABLE, QString::fromStdString(strReason));
        } else {
            itemMint->setText(COLUMN_ISSPENDABLE, QString("Yes"));
        }
    }

    ui->treeWidget->blockSignals(false);
    updateLabels();
}

// Update the list when a checkbox is clicked
void ZCrctControlDialog::updateSelection(QTreeWidgetItem* item, int column)
{
    // only want updates from non top level items that are available to spend
    if (item->parent() && column == COLUMN_CHECKBOX && !item->isDisabled()){

        // see if this mint is already selected in the selection list
        std::string strPubcoin = item->text(COLUMN_PUBCOIN).toStdString();
        bool fSelected = setSelectedMints.count(strPubcoin);

        // set the checkbox to the proper state and add or remove the mint from the selection list
        if (item->checkState(COLUMN_CHECKBOX) == Qt::Checked) {
            if (fSelected) return;
            setSelectedMints.insert(strPubcoin);
        } else {
            if (!fSelected) return;
            setSelectedMints.erase(strPubcoin);
        }
        updateLabels();
    }
}

// Update the Quantity and Amount display
void ZCrctControlDialog::updateLabels()
{
    int64_t nAmount = 0;
    for (const CMintMeta& mint : setMints) {
        if (setSelectedMints.count(mint.hashPubcoin.GetHex()))
            nAmount += mint.denom;
    }

    //update this dialog's labels
    ui->labelZCrct_int->setText(QString::number(nAmount));
    ui->labelQuantity_int->setText(QString::number(setSelectedMints.size()));

    //update PrivacyDialog labels
    privacyDialog->setZCrctControlLabels(nAmount, setSelectedMints.size());
}

std::vector<CMintMeta> ZCrctControlDialog::GetSelectedMints()
{
    std::vector<CMintMeta> listReturn;
    for (const CMintMeta& mint : setMints) {
        if (setSelectedMints.count(mint.hashPubcoin.GetHex()))
            listReturn.emplace_back(mint);
    }

    return listReturn;
}

// select or deselect all of the mints
void ZCrctControlDialog::ButtonAllClicked()
{
    ui->treeWidget->blockSignals(true);
    Qt::CheckState state = Qt::Checked;
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++) {
        if(ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) != Qt::Unchecked) {
            state = Qt::Unchecked;
            break;
        }
    }

    //much quicker to start from scratch than to have QT go through all the objects and update
    ui->treeWidget->clear();

    if (state == Qt::Checked) {
        for(const CMintMeta& mint : setMints)
            setSelectedMints.insert(mint.hashPubcoin.GetHex());
    } else {
        setSelectedMints.clear();
    }

    updateList();
}