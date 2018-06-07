// Copyright (c) 2017-2018 The Popchain Core Developers

#include "activepopnode.h"
#include "popnode.h"
#include "popnode-sync.h"
#include "popnodeman.h"
#include "protocol.h"

extern CWallet* pwalletMain;

// Keep track of the active Popnode
CActivePopnode activePopnode;

void CActivePopnode::ManageState()
{
    LogPrint("popnode", "CActivePopnode::ManageState -- Start\n");
    if(!fPopNode) {
        LogPrint("popnode", "CActivePopnode::ManageState -- Not a popnode, returning\n");
        return;
    }

    if(Params().NetworkIDString() != CBaseChainParams::REGTEST && !popnodeSync.IsBlockchainSynced()) {
        nState = ACTIVE_POPNODE_SYNC_IN_PROCESS;
        LogPrintf("CActivePopnode::ManageState -- %s: %s\n", GetStateString(), GetStatus());
        return;
    }

    if(nState == ACTIVE_POPNODE_SYNC_IN_PROCESS) {
        nState = ACTIVE_POPNODE_INITIAL;
    }

    LogPrint("popnode", "CActivePopnode::ManageState -- status = %s, type = %s, pinger enabled = %d\n", GetStatus(), GetTypeString(), fPingerEnabled);

    if(eType == POPNODE_UNKNOWN) {
        ManageStateInitial();
    }

    if(eType == POPNODE_REMOTE) {
        ManageStateRemote();
    } else if(eType == POPNODE_LOCAL) {
        // Try Remote Start first so the started local popnode can be restarted without recreate popnode broadcast.
        ManageStateRemote();
        if(nState != ACTIVE_POPNODE_STARTED)
            ManageStateLocal();
    }

    SendPopnodePing();
}

std::string CActivePopnode::GetStateString() const
{
    switch (nState) {
        case ACTIVE_POPNODE_INITIAL:         return "INITIAL";
        case ACTIVE_POPNODE_SYNC_IN_PROCESS: return "SYNC_IN_PROCESS";
        case ACTIVE_POPNODE_INPUT_TOO_NEW:   return "INPUT_TOO_NEW";
        case ACTIVE_POPNODE_NOT_CAPABLE:     return "NOT_CAPABLE";
        case ACTIVE_POPNODE_STARTED:         return "STARTED";
        default:                                return "UNKNOWN";
    }
}

std::string CActivePopnode::GetStatus() const
{
    switch (nState) {
        case ACTIVE_POPNODE_INITIAL:         return "Node just started, not yet activated";
        case ACTIVE_POPNODE_SYNC_IN_PROCESS: return "Sync in progress. Must wait until sync is complete to start Popnode";
        case ACTIVE_POPNODE_INPUT_TOO_NEW:   return strprintf("Popnode input must have at least %d confirmations", Params().GetConsensus().nPopnodeMinimumConfirmations);
        case ACTIVE_POPNODE_NOT_CAPABLE:     return "Not capable popnode: " + strNotCapableReason;
        case ACTIVE_POPNODE_STARTED:         return "Popnode successfully started";
        default:                                return "Unknown";
    }
}

std::string CActivePopnode::GetTypeString() const
{
    std::string strType;
    switch(eType) {
    case POPNODE_UNKNOWN:
        strType = "UNKNOWN";
        break;
    case POPNODE_REMOTE:
        strType = "REMOTE";
        break;
    case POPNODE_LOCAL:
        strType = "LOCAL";
        break;
    default:
        strType = "UNKNOWN";
        break;
    }
    return strType;
}

bool CActivePopnode::SendPopnodePing()
{
    if(!fPingerEnabled) {
        LogPrint("popnode", "CActivePopnode::SendPopnodePing -- %s: popnode ping service is disabled, skipping...\n", GetStateString());
        return false;
    }

    if(!mnodeman.Has(vin)) {
        strNotCapableReason = "Popnode not in popnode list";
        nState = ACTIVE_POPNODE_NOT_CAPABLE;
        LogPrintf("CActivePopnode::SendPopnodePing -- %s: %s\n", GetStateString(), strNotCapableReason);
        return false;
    }

    CPopnodePing mnp(vin);
    if(!mnp.Sign(keyPopnode, pubKeyPopnode)) {
        LogPrintf("CActivePopnode::SendPopnodePing -- ERROR: Couldn't sign Popnode Ping\n");
        return false;
    }

    // Update lastPing for our popnode in Popnode list
    if(mnodeman.IsPopnodePingedWithin(vin, POPNODE_MIN_MNP_SECONDS, mnp.sigTime)) {
        LogPrintf("CActivePopnode::SendPopnodePing -- Too early to send Popnode Ping\n");
        return false;
    }

    mnodeman.SetPopnodeLastPing(vin, mnp);

    LogPrintf("CActivePopnode::SendPopnodePing -- Relaying ping, collateral=%s\n", vin.ToString());
    mnp.Relay();

    return true;
}

void CActivePopnode::ManageStateInitial()
{
    LogPrint("popnode", "CActivePopnode::ManageStateInitial -- status = %s, type = %s, pinger enabled = %d\n", GetStatus(), GetTypeString(), fPingerEnabled);

    // Check that our local network configuration is correct
    if (!fListen) {
        // listen option is probably overwritten by smth else, no good
        nState = ACTIVE_POPNODE_NOT_CAPABLE;
        strNotCapableReason = "Popnode must accept connections from outside. Make sure listen configuration option is not overwritten by some another parameter.";
        LogPrintf("CActivePopnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    bool fFoundLocal = false;
    {
        LOCK(cs_vNodes);

        // First try to find whatever local address is specified by externalip option
        fFoundLocal = GetLocal(service) && CPopnode::IsValidNetAddr(service);
        if(!fFoundLocal) {
            // nothing and no live connections, can't do anything for now
            if (vNodes.empty()) {
//LogPrintf("empty1234123412342134\n");                
				nState = ACTIVE_POPNODE_NOT_CAPABLE;
                strNotCapableReason = "Can't detect valid external address. Will retry when there are some connections available.";
                LogPrintf("CActivePopnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
                return;
            }
            // We have some peers, let's try to find our local address from one of them
            BOOST_FOREACH(CNode* pnode, vNodes) {
LogPrintf("fSuccessfullyConnected = %c, addr.IsIPv4 = %c\n", pnode->fSuccessfullyConnected ? 'y' : 'n', pnode->addr.IsIPv4() ? 'y' : 'n' );            
			    if (pnode->fSuccessfullyConnected && pnode->addr.IsIPv4()) {
                    fFoundLocal = GetLocal(service, &pnode->addr) && CPopnode::IsValidNetAddr(service);
LogPrintf("GetLocal() = %c, IsValidNetAddr = %c \n", GetLocal(service, &pnode->addr), CPopnode::IsValidNetAddr(service));
                    if(fFoundLocal) break;
                }
            }
        }
    }

    if(!fFoundLocal) {
//LogPrintf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n");
        nState = ACTIVE_POPNODE_NOT_CAPABLE;
        strNotCapableReason = "Can't detect valid external address. Please consider using the externalip configuration option if problem persists. Make sure to use IPv4 address only.";
        LogPrintf("CActivePopnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if(Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if(service.GetPort() != mainnetDefaultPort) {
            nState = ACTIVE_POPNODE_NOT_CAPABLE;
            strNotCapableReason = strprintf("Invalid port: %u - only %d is supported on mainnet.", service.GetPort(), mainnetDefaultPort);
            LogPrintf("CActivePopnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
    } else if(service.GetPort() == mainnetDefaultPort) {
        nState = ACTIVE_POPNODE_NOT_CAPABLE;
        strNotCapableReason = strprintf("Invalid port: %u - %d is only supported on mainnet.", service.GetPort(), mainnetDefaultPort);
        LogPrintf("CActivePopnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    LogPrintf("CActivePopnode::ManageStateInitial -- Checking inbound connection to '%s'\n", service.ToString());

    if(!ConnectNode((CAddress)service, NULL, true)) {
        nState = ACTIVE_POPNODE_NOT_CAPABLE;
        strNotCapableReason = "Could not connect to " + service.ToString();
        LogPrintf("CActivePopnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    // Default to REMOTE
    eType = POPNODE_REMOTE;

    const CAmount ct = Params().GetConsensus().colleteral;
    // Check if wallet funds are available
    if(!pwalletMain) {
        LogPrintf("CActivePopnode::ManageStateInitial -- %s: Wallet not available\n", GetStateString());
        return;
    }

    if(pwalletMain->IsLocked()) {
        LogPrintf("CActivePopnode::ManageStateInitial -- %s: Wallet is locked\n", GetStateString());
        return;
    }

    if(pwalletMain->GetBalance() < ct) {
        LogPrintf("CActivePopnode::ManageStateInitial -- %s: Wallet balance is < %lld PCH\n", GetStateString(), ct);
        return;
    }

    // Choose coins to use
    CPubKey pubKeyCollateral;
    CKey keyCollateral;

    // If collateral is found switch to LOCAL mode
    if(pwalletMain->GetPopnodeVinAndKeys(vin, pubKeyCollateral, keyCollateral)) {
        eType = POPNODE_LOCAL;
    }

    LogPrint("popnode", "CActivePopnode::ManageStateInitial -- End status = %s, type = %s, pinger enabled = %d\n", GetStatus(), GetTypeString(), fPingerEnabled);
}

void CActivePopnode::ManageStateRemote()
{
    LogPrint("popnode", "CActivePopnode::ManageStateRemote -- Start status = %s, type = %s, pinger enabled = %d, pubKeyPopnode.GetID() = %s\n", 
             GetStatus(), fPingerEnabled, GetTypeString(), pubKeyPopnode.GetID().ToString());

    mnodeman.CheckPopnode(pubKeyPopnode);
    popnode_info_t infoMn = mnodeman.GetPopnodeInfo(pubKeyPopnode);
    if(infoMn.fInfoValid) {
        if(infoMn.nProtocolVersion != PROTOCOL_VERSION) {
            nState = ACTIVE_POPNODE_NOT_CAPABLE;
            strNotCapableReason = "Invalid protocol version";
            LogPrintf("CActivePopnode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if(service != infoMn.addr) {
            nState = ACTIVE_POPNODE_NOT_CAPABLE;
            strNotCapableReason = "Specified IP doesn't match our external address.";
            LogPrintf("CActivePopnode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if(!CPopnode::IsValidStateForAutoStart(infoMn.nActiveState)) {
            nState = ACTIVE_POPNODE_NOT_CAPABLE;
            strNotCapableReason = strprintf("Popnode in %s state", CPopnode::StateToString(infoMn.nActiveState));
            LogPrintf("CActivePopnode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if(nState != ACTIVE_POPNODE_STARTED) {
            LogPrintf("CActivePopnode::ManageStateRemote -- STARTED!\n");
            vin = infoMn.vin;
            service = infoMn.addr;
            fPingerEnabled = true;
            nState = ACTIVE_POPNODE_STARTED;
        }
    }
    else {
        nState = ACTIVE_POPNODE_NOT_CAPABLE;
        strNotCapableReason = "Popnode not in popnode list";
        LogPrintf("CActivePopnode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
    }
}

void CActivePopnode::ManageStateLocal()
{
    LogPrint("popnode", "CActivePopnode::ManageStateLocal -- status = %s, type = %s, pinger enabled = %d\n", GetStatus(), GetTypeString(), fPingerEnabled);
    if(nState == ACTIVE_POPNODE_STARTED) {
        return;
    }

    // Choose coins to use
    CPubKey pubKeyCollateral;
    CKey keyCollateral;

    if(pwalletMain->GetPopnodeVinAndKeys(vin, pubKeyCollateral, keyCollateral)) {
        int nInputAge = GetInputAge(vin);
        if(nInputAge < Params().GetConsensus().nPopnodeMinimumConfirmations){
            nState = ACTIVE_POPNODE_INPUT_TOO_NEW;
            strNotCapableReason = strprintf(_("%s - %d confirmations"), GetStatus(), nInputAge);
            LogPrintf("CActivePopnode::ManageStateLocal -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }

        {
            LOCK(pwalletMain->cs_wallet);
            pwalletMain->LockCoin(vin.prevout);
        }

        CPopnodeBroadcast mnb;
        std::string strError;
        if(!CPopnodeBroadcast::Create(vin, service, keyCollateral, pubKeyCollateral, keyPopnode, pubKeyPopnode, strError, mnb)) {
            nState = ACTIVE_POPNODE_NOT_CAPABLE;
            strNotCapableReason = "Error creating mastenode broadcast: " + strError;
            LogPrintf("CActivePopnode::ManageStateLocal -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }

        fPingerEnabled = true;
        nState = ACTIVE_POPNODE_STARTED;

        //update to popnode list
        LogPrintf("CActivePopnode::ManageStateLocal -- Update Popnode List\n");
        mnodeman.UpdatePopnodeList(mnb);
        mnodeman.NotifyPopnodeUpdates();

        //send to all peers
        LogPrintf("CActivePopnode::ManageStateLocal -- Relay broadcast, vin=%s\n", vin.ToString());
        mnb.Relay();
    }
}
