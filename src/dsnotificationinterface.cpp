// Copyright (c) 2017-2018 The Popchain Core Developers 

#include "dsnotificationinterface.h"
#include "darksend.h"
#include "instantx.h"
#include "popnodeman.h"
#include "popnode-payments.h"
#include "popnode-sync.h"

CDSNotificationInterface::CDSNotificationInterface()
{
}

CDSNotificationInterface::~CDSNotificationInterface()
{
}

// Popchain DevTeam
void CDSNotificationInterface::UpdatedBlockTip(const CBlockIndex *pindex)
{
    mnodeman.UpdatedBlockTip(pindex);
    darkSendPool.UpdatedBlockTip(pindex);
    instantsend.UpdatedBlockTip(pindex);
    popnodeSync.UpdatedBlockTip(pindex);
}

void CDSNotificationInterface::SyncTransaction(const CTransaction &tx, const CBlock *pblock)
{
    instantsend.SyncTransaction(tx, pblock);
}
