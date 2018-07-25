// Copyright (c) 2017-2018 The Popchain Core Developers 

#include "cdsannouncementinterface.h"
#include "popsend.h"
#include "instantx.h"
#include "popnodeman.h"
#include "popnode-payments.h"
#include "popnode-sync.h"

CDSAnnouncementInterface::CDSAnnouncementInterface()
{
}

CDSAnnouncementInterface::~CDSAnnouncementInterface()
{
}

// Popchain DevTeam

void CDSAnnouncementInterface::SyncTransaction(const CTransaction &tx, const CBlock *pblock)
{
    instantsend.SyncTransaction(tx, pblock);
}

void CDSAnnouncementInterface::UpdatedBlockTip(const CBlockIndex *pindex)
{
    mnodeman.UpdatedBlockTip(pindex);
    darkSendPool.UpdatedBlockTip(pindex);
    instantsend.UpdatedBlockTip(pindex);
    popnodeSync.UpdatedBlockTip(pindex);
}

