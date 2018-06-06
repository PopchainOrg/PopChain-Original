// Copyright (c) 2017-2018 The Popchain Core Developers


#include "activepopnode.h"
#include "checkpoints.h"
#include "main.h"
#include "popnode.h"
#include "popnode-payments.h"
#include "popnode-sync.h"
#include "popnodeman.h"
#include "netfulfilledman.h"
#include "spork.h"
#include "util.h"

class CPopnodeSync;
CPopnodeSync popnodeSync;

void ReleaseNodes(const std::vector<CNode*> &vNodesCopy)
{
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodesCopy)
        pnode->Release();
}

bool CPopnodeSync::IsSynced()
{
//	return true;
	return nRequestedPopnodeAssets == POPNODE_SYNC_FINISHED;
}

bool CPopnodeSync::CheckNodeHeight(CNode* pnode, bool fDisconnectStuckNodes)
{
    CNodeStateStats stats;
    if(!GetNodeStateStats(pnode->id, stats) || stats.nCommonHeight == -1 || stats.nSyncHeight == -1) return false; // not enough info about this peer

    // Check blocks and headers, allow a small error margin of 1 block
    if(pCurrentBlockIndex->nHeight - 1 > stats.nCommonHeight) {
        // This peer probably stuck, don't sync any additional data from it
        if(fDisconnectStuckNodes) {
            // Disconnect to free this connection slot for another peer.
            pnode->fDisconnect = true;
            LogPrintf("CPopnodeSync::CheckNodeHeight -- disconnecting from stuck peer, nHeight=%d, nCommonHeight=%d, peer=%d\n",
                        pCurrentBlockIndex->nHeight, stats.nCommonHeight, pnode->id);
        } else {
            LogPrintf("CPopnodeSync::CheckNodeHeight -- skipping stuck peer, nHeight=%d, nCommonHeight=%d, peer=%d\n",
                        pCurrentBlockIndex->nHeight, stats.nCommonHeight, pnode->id);
        }
        return false;
    }
    else if(pCurrentBlockIndex->nHeight < stats.nSyncHeight - 1) {
        // This peer announced more headers than we have blocks currently
        LogPrintf("CPopnodeSync::CheckNodeHeight -- skipping peer, who announced more headers than we have blocks currently, nHeight=%d, nSyncHeight=%d, peer=%d\n",
                    pCurrentBlockIndex->nHeight, stats.nSyncHeight, pnode->id);
        return false;
    }

    return true;
}

bool CPopnodeSync::IsBlockchainSynced(bool fBlockAccepted)
{
    static bool fBlockchainSynced = false;
    static int64_t nTimeLastProcess = GetTime();
    static int nSkipped = 0;
    static bool fFirstBlockAccepted = false;

    // if the last call to this function was more than 60 minutes ago (client was in sleep mode) reset the sync process
    if(GetTime() - nTimeLastProcess > 60*60) {
        Reset();
        fBlockchainSynced = false;
    }

    if(!pCurrentBlockIndex || !pindexBestHeader || fImporting || fReindex) return false;

    if(fBlockAccepted) {
        // this should be only triggered while we are still syncing
        if(!IsSynced()) {
            // we are trying to download smth, reset blockchain sync status
            if(fDebug) LogPrintf("CPopnodeSync::IsBlockchainSynced -- reset\n");
            fFirstBlockAccepted = true;
            fBlockchainSynced = false;
            nTimeLastProcess = GetTime();
            return false;
        }
    } else {
        // skip if we already checked less than 1 tick ago
        if(GetTime() - nTimeLastProcess < POPNODE_SYNC_TICK_SECONDS) {
            nSkipped++;
            return fBlockchainSynced;
        }
    }

    if(fDebug) LogPrintf("CPopnodeSync::IsBlockchainSynced -- state before check: %ssynced, skipped %d times\n", fBlockchainSynced ? "" : "not ", nSkipped);

    nTimeLastProcess = GetTime();
    nSkipped = 0;

    if(fBlockchainSynced) return true;

    if(fCheckpointsEnabled && pCurrentBlockIndex->nHeight < Checkpoints::GetTotalBlocksEstimate(Params().Checkpoints()))
        return false;

    std::vector<CNode*> vNodesCopy;
    {
        LOCK(cs_vNodes);
        vNodesCopy = vNodes;
        BOOST_FOREACH(CNode* pnode, vNodesCopy)
            pnode->AddRef();
    }

    // We have enough peers and assume most of them are synced
    if(vNodes.size() >= POPNODE_SYNC_ENOUGH_PEERS) {
        // Check to see how many of our peers are (almost) at the same height as we are
        int nNodesAtSameHeight = 0;
        BOOST_FOREACH(CNode* pnode, vNodesCopy)
        {
            // Make sure this peer is presumably at the same height
            if(!CheckNodeHeight(pnode)) continue;
            nNodesAtSameHeight++;
            // if we have decent number of such peers, most likely we are synced now
            if(nNodesAtSameHeight >= POPNODE_SYNC_ENOUGH_PEERS) {
                LogPrintf("CPopnodeSync::IsBlockchainSynced -- found enough peers on the same height as we are, done\n");
                fBlockchainSynced = true;
                ReleaseNodes(vNodesCopy);
                return true;
            }
        }
    }
    ReleaseNodes(vNodesCopy);

    // wait for at least one new block to be accepted
    if(!fFirstBlockAccepted) return false;

    // same as !IsInitialBlockDownload() but no cs_main needed here
    int64_t nMaxBlockTime = std::max(pCurrentBlockIndex->GetBlockTime(), pindexBestHeader->GetBlockTime());
    fBlockchainSynced = pindexBestHeader->nHeight - pCurrentBlockIndex->nHeight < 24 * 6 &&
                        GetTime() - nMaxBlockTime < Params().MaxTipAge();

    return fBlockchainSynced;
}

void CPopnodeSync::Fail()
{
    nTimeLastFailure = GetTime();
    nRequestedPopnodeAssets = POPNODE_SYNC_FAILED;
}

void CPopnodeSync::Reset()
{
    nRequestedPopnodeAssets = POPNODE_SYNC_INITIAL;
    nRequestedPopnodeAttempt = 0;
    nTimeAssetSyncStarted = GetTime();
    nTimeLastPopnodeList = GetTime();
    nTimeLastFailure = 0;
    nCountFailures = 0;
}

// Popchain DevTeam
std::string CPopnodeSync::GetAssetName()
{
    switch(nRequestedPopnodeAssets)
    {
        case(POPNODE_SYNC_INITIAL):      return "POPNODE_SYNC_INITIAL";
        case(POPNODE_SYNC_SPORKS):       return "POPNODE_SYNC_SPORKS";
        case(POPNODE_SYNC_LIST):         return "POPNODE_SYNC_LIST";
        case(POPNODE_SYNC_FAILED):       return "POPNODE_SYNC_FAILED";
        case POPNODE_SYNC_FINISHED:      return "POPNODE_SYNC_FINISHED";
        default:                            return "UNKNOWN";
    }
}

// Popchain DevTeam
void CPopnodeSync::SwitchToNextAsset()
{
    switch(nRequestedPopnodeAssets)
    {
        case(POPNODE_SYNC_FAILED):
            throw std::runtime_error("Can't switch to next asset from failed, should use Reset() first!");
            break;
        case(POPNODE_SYNC_INITIAL):
            ClearFulfilledRequests();
            nRequestedPopnodeAssets = POPNODE_SYNC_SPORKS;
            LogPrintf("CPopnodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName());
            break;
        case(POPNODE_SYNC_SPORKS):
            nTimeLastPopnodeList = GetTime();
            nRequestedPopnodeAssets = POPNODE_SYNC_LIST;
            LogPrintf("CPopnodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName());
            break;
        case(POPNODE_SYNC_LIST):
            LogPrintf("CPopnodeSync::SwitchToNextAsset -- Sync has finished\n");
            nRequestedPopnodeAssets = POPNODE_SYNC_FINISHED;
            uiInterface.NotifyAdditionalDataSyncProgressChanged(1);
            activePopnode.ManageState();
            TRY_LOCK(cs_vNodes, lockRecv);
            if(!lockRecv) return;

            BOOST_FOREACH(CNode* pnode, vNodes) {
                netfulfilledman.AddFulfilledRequest(pnode->addr, "full-sync");
            }
            break;
    }
    nRequestedPopnodeAttempt = 0;
    nTimeAssetSyncStarted = GetTime();
}

std::string CPopnodeSync::GetSyncStatus()
{
    switch (popnodeSync.nRequestedPopnodeAssets) {
        case POPNODE_SYNC_INITIAL:       return _("Synchronization pending...");
        case POPNODE_SYNC_SPORKS:        return _("Synchronizing sporks...");
        case POPNODE_SYNC_LIST:          return _("Synchronizing popnodes...");
        case POPNODE_SYNC_FAILED:        return _("Synchronization failed");
        case POPNODE_SYNC_FINISHED:      return _("Synchronization finished");
        default:                            return "";
    }
}

void CPopnodeSync::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if (strCommand == NetMsgType::SYNCSTATUSCOUNT) { //Sync status count

        //do not care about stats if sync process finished or failed
        if(IsSynced() || IsFailed()) return;

        int nItemID;
        int nCount;
        vRecv >> nItemID >> nCount;

        LogPrintf("SYNCSTATUSCOUNT -- got inventory count: nItemID=%d  nCount=%d  peer=%d\n", nItemID, nCount, pfrom->id);
    }
}

void CPopnodeSync::ClearFulfilledRequests()
{
    TRY_LOCK(cs_vNodes, lockRecv);
    if(!lockRecv) return;

    BOOST_FOREACH(CNode* pnode, vNodes)
    {
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "spork-sync");
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "popnode-list-sync");
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "full-sync");
    }
}

// Popchain DevTeam
void CPopnodeSync::ProcessTick()
{
    static int nTick = 0;
    if(nTick++ % POPNODE_SYNC_TICK_SECONDS != 0) return;
    if(!pCurrentBlockIndex) return;

    //the actual count of popnodes we have currently

    // RESET SYNCING INCASE OF FAILURE
    {
        if(IsSynced()) {}

        //try syncing again
        if(IsFailed()) {
            if(nTimeLastFailure + (1*60) < GetTime()) { // 1 minute cooldown after failed sync
                Reset();
            }
            return;
        }
    }

    // INITIAL SYNC SETUP / LOG REPORTING
    double nSyncProgress = double(nRequestedPopnodeAttempt + (nRequestedPopnodeAssets - 1) * 8) / (8*4);
    LogPrintf("CPopnodeSync::ProcessTick -- nTick %d nRequestedPopnodeAssets %d nRequestedPopnodeAttempt %d nSyncProgress %f\n", nTick, nRequestedPopnodeAssets, nRequestedPopnodeAttempt, nSyncProgress);
    uiInterface.NotifyAdditionalDataSyncProgressChanged(nSyncProgress);

    // sporks synced but blockchain is not, wait until we're almost at a recent block to continue
    if(Params().NetworkIDString() != CBaseChainParams::REGTEST &&
            !IsBlockchainSynced() && nRequestedPopnodeAssets > POPNODE_SYNC_SPORKS)
    {
        LogPrintf("CPopnodeSync::ProcessTick -- nTick %d nRequestedPopnodeAssets %d nRequestedPopnodeAttempt %d -- blockchain is not synced yet\n", nTick, nRequestedPopnodeAssets, nRequestedPopnodeAttempt);
        nTimeLastPopnodeList = GetTime();
        return;
    }

    if(nRequestedPopnodeAssets == POPNODE_SYNC_INITIAL ||
        (nRequestedPopnodeAssets == POPNODE_SYNC_SPORKS && IsBlockchainSynced()))
    {
        SwitchToNextAsset();
    }

    std::vector<CNode*> vNodesCopy;
    {
        LOCK(cs_vNodes);
        vNodesCopy = vNodes;
        BOOST_FOREACH(CNode* pnode, vNodesCopy)
            pnode->AddRef();
    }

    BOOST_FOREACH(CNode* pnode, vNodesCopy)
    {
        // QUICK MODE (REGTEST ONLY!)
        if(Params().NetworkIDString() == CBaseChainParams::REGTEST)
        {
            if(nRequestedPopnodeAttempt <= 2) {
                pnode->PushMessage(NetMsgType::GETSPORKS); //get current network sporks
            } else if(nRequestedPopnodeAttempt < 4) {
                mnodeman.DsegUpdate(pnode);
            } else if(nRequestedPopnodeAttempt < 6) {
            } else {
                nRequestedPopnodeAssets = POPNODE_SYNC_FINISHED;
            }
            nRequestedPopnodeAttempt++;
            ReleaseNodes(vNodesCopy);
            return;
        }

        // NORMAL NETWORK MODE - TESTNET/MAINNET
        {
            if(netfulfilledman.HasFulfilledRequest(pnode->addr, "full-sync")) {
                // we already fully synced from this node recently,
                // disconnect to free this connection slot for a new node
                pnode->fDisconnect = true;
                LogPrintf("CPopnodeSync::ProcessTick -- disconnecting from recently synced peer %d\n", pnode->id);
                continue;
            }

            // Make sure this peer is presumably at the same height
            if(!CheckNodeHeight(pnode, true)) continue;

            // SPORK : ALWAYS ASK FOR SPORKS AS WE SYNC (we skip this mode now)

            if(!netfulfilledman.HasFulfilledRequest(pnode->addr, "spork-sync")) {
                // only request once from each peer
                netfulfilledman.AddFulfilledRequest(pnode->addr, "spork-sync");
                // get current network sporks
                pnode->PushMessage(NetMsgType::GETSPORKS);
                LogPrintf("CPopnodeSync::ProcessTick -- nTick %d nRequestedPopnodeAssets %d -- requesting sporks from peer %d\n", nTick, nRequestedPopnodeAssets, pnode->id);
                continue; // always get sporks first, switch to the next node without waiting for the next tick
            }

            // MNLIST : SYNC POPNODE LIST FROM OTHER CONNECTED CLIENTS

            if(nRequestedPopnodeAssets == POPNODE_SYNC_LIST) {
                LogPrint("popnode", "CPopnodeSync::ProcessTick -- nTick %d nRequestedPopnodeAssets %d nTimeLastPopnodeList %lld GetTime() %lld diff %lld\n", nTick, nRequestedPopnodeAssets, nTimeLastPopnodeList, GetTime(), GetTime() - nTimeLastPopnodeList);
                // check for timeout first
                if(nTimeLastPopnodeList < GetTime() - POPNODE_SYNC_TIMEOUT_SECONDS) {
                    LogPrintf("CPopnodeSync::ProcessTick -- nTick %d nRequestedPopnodeAssets %d -- timeout\n", nTick, nRequestedPopnodeAssets);
                    if (nRequestedPopnodeAttempt == 0) {
                        LogPrintf("CPopnodeSync::ProcessTick -- ERROR: failed to sync %s\n", GetAssetName());
                        // there is no way we can continue without popnode list, fail here and try later
                        Fail();
                        ReleaseNodes(vNodesCopy);
                        return;
                    }
                    SwitchToNextAsset();
                    ReleaseNodes(vNodesCopy);
                    return;
                }

                // only request once from each peer
                if(netfulfilledman.HasFulfilledRequest(pnode->addr, "popnode-list-sync")) continue;
                netfulfilledman.AddFulfilledRequest(pnode->addr, "popnode-list-sync");

                nRequestedPopnodeAttempt++;

                mnodeman.DsegUpdate(pnode);

                ReleaseNodes(vNodesCopy);
                return; //this will cause each peer to get one request each six seconds for the various assets we need
            }
        }
    }
    // looped through all nodes, release them
    ReleaseNodes(vNodesCopy);
}


void CPopnodeSync::UpdatedBlockTip(const CBlockIndex *pindex)
{
    pCurrentBlockIndex = pindex;
}
