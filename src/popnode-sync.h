// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef POPNODE_SYNC_H
#define POPNODE_SYNC_H

#include "chain.h"
#include "net.h"

#include <univalue.h>

class CPopnodeSync;

static const int POPNODE_SYNC_FAILED          = -1;
static const int POPNODE_SYNC_INITIAL         = 0;
static const int POPNODE_SYNC_SPORKS          = 1;
static const int POPNODE_SYNC_LIST            = 2;
static const int POPNODE_SYNC_FINISHED        = 999;

static const int POPNODE_SYNC_TICK_SECONDS    = 6;
static const int POPNODE_SYNC_TIMEOUT_SECONDS = 30; // our blocks are 2.5 minutes so 30 seconds should be fine

static const int POPNODE_SYNC_ENOUGH_PEERS    = 6;

extern CPopnodeSync popnodeSync;

//
// CPopnodeSync : Sync popnode assets in stages
//

class CPopnodeSync
{
private:
    // Keep track of current asset
    int nRequestedPopnodeAssets;
    // Count peers we've requested the asset from
    int nRequestedPopnodeAttempt;

    // Time when current popnode asset sync started
    int64_t nTimeAssetSyncStarted;

    // Last time when we received some popnode asset ...
    int64_t nTimeLastPopnodeList;
    int64_t nTimeLastPaymentVote;
    // ... or failed
    int64_t nTimeLastFailure;

    // How many times we failed
    int nCountFailures;

    // Keep track of current block index
    const CBlockIndex *pCurrentBlockIndex;

    bool CheckNodeHeight(CNode* pnode, bool fDisconnectStuckNodes = false);
    void Fail();
    void ClearFulfilledRequests();

public:
    CPopnodeSync() { Reset(); }

    void AddedPopnodeList() { nTimeLastPopnodeList = GetTime(); }

    bool IsFailed() { return nRequestedPopnodeAssets == POPNODE_SYNC_FAILED; }
    bool IsBlockchainSynced(bool fBlockAccepted = false);
    bool IsPopnodeListSynced() { return nRequestedPopnodeAssets > POPNODE_SYNC_LIST; }
    bool IsSynced();/* { return nRequestedPopnodeAssets == POPNODE_SYNC_FINISHED; } */

    int GetAssetID() { return nRequestedPopnodeAssets; }
    int GetAttempt() { return nRequestedPopnodeAttempt; }
    std::string GetAssetName();
    std::string GetSyncStatus();

    void Reset();
    void SwitchToNextAsset();

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
    void ProcessTick();

    void UpdatedBlockTip(const CBlockIndex *pindex);
};

#endif
