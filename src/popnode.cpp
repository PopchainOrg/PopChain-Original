// Copyright (c) 2017-2018 The Popchain Core Developers


#include "activepopnode.h"
#include "consensus/validation.h"
#include "darksend.h"
#include "init.h"
#include "popnode.h"
#include "popnode-payments.h"
#include "popnode-sync.h"
#include "popnodeman.h"
#include "util.h"

#include <boost/lexical_cast.hpp>


CPopnode::CPopnode() :
    vin(),
    addr(),
    pubKeyCollateralAddress(),
    pubKeyPopnode(),
    lastPing(),
    vchSig(),
    sigTime(GetAdjustedTime()),
    nLastDsq(0),
    nTimeLastChecked(0),
    nTimeLastCheckedRegistered(0),
    nTimeLastPaid(0),
    nTimeLastWatchdogVote(0),
    nActiveState(POPNODE_ENABLED),
    nCacheCollateralBlock(0),
    nBlockLastPaid(0),
    nProtocolVersion(PROTOCOL_VERSION),
    nPoSeBanScore(0),
    nPoSeBanHeight(0),
    fAllowMixingTx(true),
    fUnitTest(false)
{}

CPopnode::CPopnode(CService addrNew, CTxIn vinNew, CPubKey pubKeyCollateralAddressNew, CPubKey pubKeyPopnodeNew, int nProtocolVersionIn) :
    vin(vinNew),
    addr(addrNew),
    pubKeyCollateralAddress(pubKeyCollateralAddressNew),
    pubKeyPopnode(pubKeyPopnodeNew),
    lastPing(),
    vchSig(),
    sigTime(GetAdjustedTime()),
    nLastDsq(0),
    nTimeLastChecked(0),
    nTimeLastCheckedRegistered(0),
    nTimeLastPaid(0),
    nTimeLastWatchdogVote(0),
    nActiveState(POPNODE_ENABLED),
    nCacheCollateralBlock(0),
    nBlockLastPaid(0),
    nProtocolVersion(nProtocolVersionIn),
    nPoSeBanScore(0),
    nPoSeBanHeight(0),
    fAllowMixingTx(true),
    fUnitTest(false)
{}

CPopnode::CPopnode(const CPopnode& other) :
    vin(other.vin),
    addr(other.addr),
    pubKeyCollateralAddress(other.pubKeyCollateralAddress),
    pubKeyPopnode(other.pubKeyPopnode),
    lastPing(other.lastPing),
    vchSig(other.vchSig),
    sigTime(other.sigTime),
    nLastDsq(other.nLastDsq),
    nTimeLastChecked(other.nTimeLastChecked),
    nTimeLastCheckedRegistered(other.nTimeLastCheckedRegistered),
    nTimeLastPaid(other.nTimeLastPaid),
    nTimeLastWatchdogVote(other.nTimeLastWatchdogVote),
    nActiveState(other.nActiveState),
    nCacheCollateralBlock(other.nCacheCollateralBlock),
    nBlockLastPaid(other.nBlockLastPaid),
    nProtocolVersion(other.nProtocolVersion),
    nPoSeBanScore(other.nPoSeBanScore),
    nPoSeBanHeight(other.nPoSeBanHeight),
    fAllowMixingTx(other.fAllowMixingTx),
    fUnitTest(other.fUnitTest)
{}

CPopnode::CPopnode(const CPopnodeBroadcast& mnb) :
    vin(mnb.vin),
    addr(mnb.addr),
    pubKeyCollateralAddress(mnb.pubKeyCollateralAddress),
    pubKeyPopnode(mnb.pubKeyPopnode),
    lastPing(mnb.lastPing),
    vchSig(mnb.vchSig),
    sigTime(mnb.sigTime),
    nLastDsq(0),
    nTimeLastChecked(0),
    nTimeLastCheckedRegistered(0),
    nTimeLastPaid(0),
    nTimeLastWatchdogVote(mnb.sigTime),
    nActiveState(mnb.nActiveState),
    nCacheCollateralBlock(0),
    nBlockLastPaid(0),
    nProtocolVersion(mnb.nProtocolVersion),
    nPoSeBanScore(0),
    nPoSeBanHeight(0),
    fAllowMixingTx(true),
    fUnitTest(false)
{}

//
// When a new popnode broadcast is sent, update our information
//
bool CPopnode::UpdateFromNewBroadcast(CPopnodeBroadcast& mnb)
{
    if(mnb.sigTime <= sigTime && !mnb.fRecovery) return false;

    pubKeyPopnode = mnb.pubKeyPopnode;
    sigTime = mnb.sigTime;
    vchSig = mnb.vchSig;
    nProtocolVersion = mnb.nProtocolVersion;
    addr = mnb.addr;
    nPoSeBanScore = 0;
    nPoSeBanHeight = 0;
    nTimeLastChecked = 0;
	nTimeLastCheckedRegistered = 0;
    int nDos = 0;
    if(mnb.lastPing == CPopnodePing() || (mnb.lastPing != CPopnodePing() && mnb.lastPing.CheckAndUpdate(this, true, nDos))) {
        lastPing = mnb.lastPing;
        mnodeman.mapSeenPopnodePing.insert(std::make_pair(lastPing.GetHash(), lastPing));
    }
    // if it matches our Popnode privkey...
    if(fPopNode && pubKeyPopnode == activePopnode.pubKeyPopnode) {
        nPoSeBanScore = -POPNODE_POSE_BAN_MAX_SCORE;
        if(nProtocolVersion == PROTOCOL_VERSION) {
            // ... and PROTOCOL_VERSION, then we've been remotely activated ...
            activePopnode.ManageState();
        } else {
            // ... otherwise we need to reactivate our node, do not add it to the list and do not relay
            // but also do not ban the node we get this message from
            LogPrintf("CPopnode::UpdateFromNewBroadcast -- wrong PROTOCOL_VERSION, re-activate your MN: message nProtocolVersion=%d  PROTOCOL_VERSION=%d\n", nProtocolVersion, PROTOCOL_VERSION);
            return false;
        }
    }
    return true;
}

//
// Deterministically calculate a given "score" for a Popnode depending on how close it's hash is to
// the proof of work for that block. The further away they are the better, the furthest will win the election
// and get paid this block
//
arith_uint256 CPopnode::CalculateScore(const uint256& blockHash)
{
    uint256 aux = ArithToUint256(UintToArith256(vin.prevout.hash) + vin.prevout.n);

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << blockHash;
    arith_uint256 hash2 = UintToArith256(ss.GetHash());

    CHashWriter ss2(SER_GETHASH, PROTOCOL_VERSION);
    ss2 << blockHash;
    ss2 << aux;
    arith_uint256 hash3 = UintToArith256(ss2.GetHash());

    return (hash3 > hash2 ? hash3 - hash2 : hash2 - hash3);
}

// Popchain DevTeam
void CPopnode::Check(bool fForce)
{
    LOCK(cs);

    if(ShutdownRequested()) return;

    if(!fForce && (GetTime() - nTimeLastChecked < POPNODE_CHECK_SECONDS)) return;
    nTimeLastChecked = GetTime();

    LogPrint("popnode", "CPopnode::Check -- Popnode %s is in %s state\n", vin.prevout.ToStringShort(), GetStateString());

    //once spent, stop doing the checks
    if(IsOutpointSpent() || IsRegistered()) return;

    int nHeight = 0;
    if(!fUnitTest) {
        TRY_LOCK(cs_main, lockMain);
        if(!lockMain) return;

        CCoins coins;
        if(!pcoinsTip->GetCoins(vin.prevout.hash, coins) ||
           (unsigned int)vin.prevout.n>=coins.vout.size() ||
           coins.vout[vin.prevout.n].IsNull()) {
            nActiveState = POPNODE_OUTPOINT_SPENT;
            LogPrint("popnode", "CPopnode::Check -- Failed to find Popnode UTXO, popnode=%s\n", vin.prevout.ToStringShort());
            return;
        }
		
		nHeight = chainActive.Height();
    }

    if(IsPoSeBanned()) {
        if(nHeight < nPoSeBanHeight) return; // too early?
        // Otherwise give it a chance to proceed further to do all the usual checks and to change its state.
        // Popnode still will be on the edge and can be banned back easily if it keeps ignoring mnverify
        // or connect attempts. Will require few mnverify messages to strengthen its position in mn list.
        LogPrintf("CPopnode::Check -- Popnode %s is unbanned and back in list now\n", vin.prevout.ToStringShort());
        DecreasePoSeBanScore();
    } else if(nPoSeBanScore >= POPNODE_POSE_BAN_MAX_SCORE) {
        nActiveState = POPNODE_POSE_BAN;
        // ban for the whole payment cycle
        nPoSeBanHeight = nHeight + mnodeman.size();
        LogPrintf("CPopnode::Check -- Popnode %s is banned till block %d now\n", vin.prevout.ToStringShort(), nPoSeBanHeight);
        return;
    }

    int nActiveStatePrev = nActiveState;
    bool fOurPopnode = fPopNode && activePopnode.pubKeyPopnode == pubKeyPopnode;

    bool fRequireUpdate = (fOurPopnode && nProtocolVersion < PROTOCOL_VERSION);

    if(fRequireUpdate) {
        nActiveState = POPNODE_UPDATE_REQUIRED;
        if(nActiveStatePrev != nActiveState) {
            LogPrint("popnode", "CPopnode::Check -- Popnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
        }
        return;
    }

    // keep old popnodes on start, give them a chance to receive updates...
    bool fWaitForPing = !popnodeSync.IsPopnodeListSynced() && !IsPingedWithin(POPNODE_MIN_MNP_SECONDS);

    //
    // REMOVE AFTER MIGRATION TO 12.1
    //
    // Old nodes don't send pings on dseg, so they could switch to one of the expired states
    // if we were offline for too long even if they are actually enabled for the rest
    // of the network. Postpone their check for POPNODE_MIN_MNP_SECONDS seconds.
    // This could be usefull for 12.1 migration, can be removed after it's done.
    static int64_t nTimeStart = GetTime();
    if(nProtocolVersion < 70204) {
        if(!popnodeSync.IsPopnodeListSynced()) nTimeStart = GetTime();
        fWaitForPing = GetTime() - nTimeStart < POPNODE_MIN_MNP_SECONDS;
    }
    //
    // END REMOVE
    //

    if(fWaitForPing && !fOurPopnode) {
        // ...but if it was already expired before the initial check - return right away
        if(IsExpired() || IsWatchdogExpired() || IsNewStartRequired()) {
            LogPrint("popnode", "CPopnode::Check -- Popnode %s is in %s state, waiting for ping\n", vin.prevout.ToStringShort(), GetStateString());
            return;
        }
    }

    // don't expire if we are still in "waiting for ping" mode unless it's our own popnode
    if(!fWaitForPing || fOurPopnode) {

        if(!IsPingedWithin(POPNODE_NEW_START_REQUIRED_SECONDS)) {
            nActiveState = POPNODE_NEW_START_REQUIRED;
            if(nActiveStatePrev != nActiveState) {
                LogPrint("popnode", "CPopnode::Check -- Popnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
            }
            return;
        }

        bool fWatchdogActive = popnodeSync.IsSynced() && mnodeman.IsWatchdogActive();
        bool fWatchdogExpired = (fWatchdogActive && ((GetTime() - nTimeLastWatchdogVote) > POPNODE_WATCHDOG_MAX_SECONDS));

        LogPrint("popnode", "CPopnode::Check -- outpoint=%s, nTimeLastWatchdogVote=%d, GetTime()=%d, fWatchdogExpired=%d\n",
                vin.prevout.ToStringShort(), nTimeLastWatchdogVote, GetTime(), fWatchdogExpired);

        if(fWatchdogExpired) {
            nActiveState = POPNODE_WATCHDOG_EXPIRED;
            if(nActiveStatePrev != nActiveState) {
                LogPrint("popnode", "CPopnode::Check -- Popnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
            }
            return;
        }

        if(!IsPingedWithin(POPNODE_EXPIRATION_SECONDS)) {
            nActiveState = POPNODE_EXPIRED;
            if(nActiveStatePrev != nActiveState) {
                LogPrint("popnode", "CPopnode::Check -- Popnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
            }
            return;
        }
    }

    if(lastPing.sigTime - sigTime < POPNODE_MIN_MNP_SECONDS) {
        nActiveState = POPNODE_PRE_ENABLED;
        if(nActiveStatePrev != nActiveState) {
            LogPrint("popnode", "CPopnode::Check -- Popnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
        }
        return;
    }

	if(GetTime() - nTimeLastCheckedRegistered > MNM_REGISTERED_CHECK_SECONDS)
	{
		nTimeLastCheckedRegistered = GetTime();
		//CPopnode mn(*this);
		if(!mnodeman.CheckActiveMaster(*this))
		{
			nActiveState = POPNODE_NO_REGISTERED;
			LogPrint("popnode", "CPopnode::Check -- Popnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
			return;
		}
	}

    nActiveState = POPNODE_ENABLED; // OK
    if(nActiveStatePrev != nActiveState) {
        LogPrint("popnode", "CPopnode::Check -- Popnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
    }
}

bool CPopnode::IsValidNetAddr()
{
    return IsValidNetAddr(addr);
}

bool CPopnode::IsValidNetAddr(CService addrIn)
{
    // TODO: regtest is fine with any addresses for now,
    // should probably be a bit smarter if one day we start to implement tests for this
    return Params().NetworkIDString() == CBaseChainParams::REGTEST ||
           Params().NetworkIDString() == CBaseChainParams::TESTNET ||
            (addrIn.IsIPv4() && IsReachable(addrIn) && addrIn.IsRoutable());
}

popnode_info_t CPopnode::GetInfo()
{
    popnode_info_t info;
    info.vin = vin;
    info.addr = addr;
    info.pubKeyCollateralAddress = pubKeyCollateralAddress;
    info.pubKeyPopnode = pubKeyPopnode;
    info.sigTime = sigTime;
    info.nLastDsq = nLastDsq;
    info.nTimeLastChecked = nTimeLastChecked;
    info.nTimeLastPaid = nTimeLastPaid;
    info.nTimeLastWatchdogVote = nTimeLastWatchdogVote;
    info.nActiveState = nActiveState;
    info.nProtocolVersion = nProtocolVersion;
    info.fInfoValid = true;
    return info;
}

std::string CPopnode::StateToString(int nStateIn)
{
    switch(nStateIn) {
        case POPNODE_PRE_ENABLED:            return "PRE_ENABLED";
        case POPNODE_ENABLED:                return "ENABLED";
        case POPNODE_EXPIRED:                return "EXPIRED";
        case POPNODE_OUTPOINT_SPENT:         return "OUTPOINT_SPENT";
        case POPNODE_UPDATE_REQUIRED:        return "UPDATE_REQUIRED";
        case POPNODE_WATCHDOG_EXPIRED:       return "WATCHDOG_EXPIRED";
        case POPNODE_NEW_START_REQUIRED:     return "NEW_START_REQUIRED";
        case POPNODE_POSE_BAN:               return "POSE_BAN";
		case POPNODE_NO_REGISTERED:          return "NO_REGISTERED";
        default:                                return "UNKNOWN";
    }
}

std::string CPopnode::GetStateString() const
{
    return StateToString(nActiveState);
}

std::string CPopnode::GetStatus() const
{
    // TODO: return smth a bit more human readable here
    return GetStateString();
}

int CPopnode::GetCollateralAge()
{
    int nHeight;
    {
        TRY_LOCK(cs_main, lockMain);
        if(!lockMain || !chainActive.Tip()) return -1;
        nHeight = chainActive.Height();
    }

    if (nCacheCollateralBlock == 0) {
        int nInputAge = GetInputAge(vin);
        if(nInputAge > 0) {
            nCacheCollateralBlock = nHeight - nInputAge;
        } else {
            return nInputAge;
        }
    }

    return nHeight - nCacheCollateralBlock;
}


bool CPopnodeBroadcast::Create(std::string strService, std::string strKeyPopnode, std::string strTxHash, std::string strOutputIndex, std::string& strErrorRet, CPopnodeBroadcast &mnbRet, bool fOffline)
{
    CTxIn txin;
    CPubKey pubKeyCollateralAddressNew;
    CKey keyCollateralAddressNew;
    CPubKey pubKeyPopnodeNew;
    CKey keyPopnodeNew;

    //need correct blocks to send ping
    if(!fOffline && !popnodeSync.IsBlockchainSynced()) {
        strErrorRet = "Sync in progress. Must wait until sync is complete to start Popnode";
        LogPrintf("CPopnodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    if(!darkSendSigner.GetKeysFromSecret(strKeyPopnode, keyPopnodeNew, pubKeyPopnodeNew)) {
        strErrorRet = strprintf("Invalid popnode key %s", strKeyPopnode);
        LogPrintf("CPopnodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    if(!pwalletMain->GetPopnodeVinAndKeys(txin, pubKeyCollateralAddressNew, keyCollateralAddressNew, strTxHash, strOutputIndex)) {
        strErrorRet = strprintf("Could not allocate txin %s:%s for popnode %s", strTxHash, strOutputIndex, strService);
        LogPrintf("CPopnodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    CService service = CService(strService);
    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if(Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if(service.GetPort() != mainnetDefaultPort) {
            strErrorRet = strprintf("Invalid port %u for popnode %s, only %d is supported on mainnet.", service.GetPort(), strService, mainnetDefaultPort);
            LogPrintf("CPopnodeBroadcast::Create -- %s\n", strErrorRet);
            return false;
        }
    } else if (service.GetPort() == mainnetDefaultPort) {
        strErrorRet = strprintf("Invalid port %u for popnode %s, %d is the only supported on mainnet.", service.GetPort(), strService, mainnetDefaultPort);
        LogPrintf("CPopnodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    return Create(txin, CService(strService), keyCollateralAddressNew, pubKeyCollateralAddressNew, keyPopnodeNew, pubKeyPopnodeNew, strErrorRet, mnbRet);
}

bool CPopnodeBroadcast::Create(CTxIn txin, CService service, CKey keyCollateralAddressNew, CPubKey pubKeyCollateralAddressNew, CKey keyPopnodeNew, CPubKey pubKeyPopnodeNew, std::string &strErrorRet, CPopnodeBroadcast &mnbRet)
{
    // wait for reindex and/or import to finish
    if (fImporting || fReindex) return false;

    LogPrint("popnode", "CPopnodeBroadcast::Create -- pubKeyCollateralAddressNew = %s, pubKeyPopnodeNew.GetID() = %s\n",
             CBitcoinAddress(pubKeyCollateralAddressNew.GetID()).ToString(),
             pubKeyPopnodeNew.GetID().ToString());


    CPopnodePing mnp(txin);
    if(!mnp.Sign(keyPopnodeNew, pubKeyPopnodeNew)) {
        strErrorRet = strprintf("Failed to sign ping, popnode=%s", txin.prevout.ToStringShort());
        LogPrintf("CPopnodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CPopnodeBroadcast();
        return false;
    }

    mnbRet = CPopnodeBroadcast(service, txin, pubKeyCollateralAddressNew, pubKeyPopnodeNew, PROTOCOL_VERSION);

    if(!mnbRet.IsValidNetAddr()) {
        strErrorRet = strprintf("Invalid IP address, popnode=%s", txin.prevout.ToStringShort());
        LogPrintf("CPopnodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CPopnodeBroadcast();
        return false;
    }

    mnbRet.lastPing = mnp;
    if(!mnbRet.Sign(keyCollateralAddressNew)) {
        strErrorRet = strprintf("Failed to sign broadcast, popnode=%s", txin.prevout.ToStringShort());
        LogPrintf("CPopnodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CPopnodeBroadcast();
        return false;
    }

    return true;
}

// Popchain DevTeam
bool CPopnodeBroadcast::SimpleCheck(int& nDos)
{
    nDos = 0;

    // make sure addr is valid
    if(!IsValidNetAddr()) {
        LogPrintf("CPopnodeBroadcast::SimpleCheck -- Invalid addr, rejected: popnode=%s  addr=%s\n",
                    vin.prevout.ToStringShort(), addr.ToString());
        return false;
    }

    // make sure signature isn't in the future (past is OK)
    if (sigTime > GetAdjustedTime() + 60 * 60) {
        LogPrintf("CPopnodeBroadcast::SimpleCheck -- Signature rejected, too far into the future: popnode=%s\n", vin.prevout.ToStringShort());
        nDos = 1;
        return false;
    }

    // empty ping or incorrect sigTime/unknown blockhash
    if(lastPing == CPopnodePing() || !lastPing.SimpleCheck(nDos)) {
        // one of us is probably forked or smth, just mark it as expired and check the rest of the rules
        nActiveState = POPNODE_EXPIRED;
    }


    CScript pubkeyScript;
    pubkeyScript = GetScriptForDestination(pubKeyCollateralAddress.GetID());

    if(pubkeyScript.size() != 25) {
        LogPrintf("CPopnodeBroadcast::SimpleCheck -- pubKeyCollateralAddress has the wrong size\n");
        nDos = 100;
        return false;
    }

    CScript pubkeyScript2;
    pubkeyScript2 = GetScriptForDestination(pubKeyPopnode.GetID());

    if(pubkeyScript2.size() != 25) {
        LogPrintf("CPopnodeBroadcast::SimpleCheck -- pubKeyPopnode has the wrong size\n");
        nDos = 100;
        return false;
    }

    if(!vin.scriptSig.empty()) {
        LogPrintf("CPopnodeBroadcast::SimpleCheck -- Ignore Not Empty ScriptSig %s\n",vin.ToString());
        nDos = 100;
        return false;
    }

    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if(Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if(addr.GetPort() != mainnetDefaultPort) return false;
    } else if(addr.GetPort() == mainnetDefaultPort) return false;

    return true;
}

bool CPopnodeBroadcast::Update(CPopnode* pmn, int& nDos)
{
    nDos = 0;

    if(pmn->sigTime == sigTime && !fRecovery) {
        // mapSeenPopnodeBroadcast in CPopnodeMan::CheckMnbAndUpdatePopnodeList should filter legit duplicates
        // but this still can happen if we just started, which is ok, just do nothing here.
        return false;
    }

    // this broadcast is older than the one that we already have - it's bad and should never happen
    // unless someone is doing something fishy
    if(pmn->sigTime > sigTime) {
        LogPrintf("CPopnodeBroadcast::Update -- Bad sigTime %d (existing broadcast is at %d) for Popnode %s %s\n",
                      sigTime, pmn->sigTime, vin.prevout.ToStringShort(), addr.ToString());
        return false;
    }

    pmn->Check();

    // popnode is banned by PoSe
    if(pmn->IsPoSeBanned()) {
        LogPrintf("CPopnodeBroadcast::Update -- Banned by PoSe, popnode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    // IsVnAssociatedWithPubkey is validated once in CheckOutpoint, after that they just need to match
    if(pmn->pubKeyCollateralAddress != pubKeyCollateralAddress) {
        LogPrintf("CPopnodeBroadcast::Update -- Got mismatched pubKeyCollateralAddress and vin\n");
        nDos = 33;
        return false;
    }

    if (!CheckSignature(nDos)) {
        LogPrintf("CPopnodeBroadcast::Update -- CheckSignature() failed, popnode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    // if ther was no popnode broadcast recently or if it matches our Popnode privkey...
    if(!pmn->IsBroadcastedWithin(POPNODE_MIN_MNB_SECONDS) || (fPopNode && pubKeyPopnode == activePopnode.pubKeyPopnode)) {
        // take the newest entry
        LogPrintf("CPopnodeBroadcast::Update -- Got UPDATED Popnode entry: addr=%s\n", addr.ToString());
        if(pmn->UpdateFromNewBroadcast((*this))) {
            pmn->Check();
            Relay();
        }
        popnodeSync.AddedPopnodeList();
    }

    return true;
}

bool CPopnodeBroadcast::CheckOutpoint(int& nDos)
{
    // we are a popnode with the same vin (i.e. already activated) and this mnb is ours (matches our Popnode privkey)
    // so nothing to do here for us
    if(fPopNode && vin.prevout == activePopnode.vin.prevout && pubKeyPopnode == activePopnode.pubKeyPopnode) {
        return false;
    }

    if (!CheckSignature(nDos)) {
        LogPrintf("CPopnodeBroadcast::CheckOutpoint -- CheckSignature() failed, popnode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    {
	const CAmount ct = Params().GetConsensus().colleteral;		// colleteral
        TRY_LOCK(cs_main, lockMain);
        if(!lockMain) {
            // not mnb fault, let it to be checked again later
            LogPrint("popnode", "CPopnodeBroadcast::CheckOutpoint -- Failed to aquire lock, addr=%s", addr.ToString());
            mnodeman.mapSeenPopnodeBroadcast.erase(GetHash());
            return false;
        }

        CCoins coins;
        if(!pcoinsTip->GetCoins(vin.prevout.hash, coins) ||
           (unsigned int)vin.prevout.n>=coins.vout.size() ||
           coins.vout[vin.prevout.n].IsNull()) {
            LogPrint("popnode", "CPopnodeBroadcast::CheckOutpoint -- Failed to find Popnode UTXO, popnode=%s\n", vin.prevout.ToStringShort());
            return false;
        }
        if(coins.vout[vin.prevout.n].nValue != ct) {
            LogPrint("popnode", "CPopnodeBroadcast::CheckOutpoint -- Popnode UTXO should have %lld PCH, popnode=%s\n", ct, vin.prevout.ToStringShort());
            return false;
        }
        if(chainActive.Height() - coins.nHeight + 1 < Params().GetConsensus().nPopnodeMinimumConfirmations) {
            LogPrintf("CPopnodeBroadcast::CheckOutpoint -- Popnode UTXO must have at least %d confirmations, popnode=%s\n",
                    Params().GetConsensus().nPopnodeMinimumConfirmations, vin.prevout.ToStringShort());
            // maybe we miss few blocks, let this mnb to be checked again later
            mnodeman.mapSeenPopnodeBroadcast.erase(GetHash());
            return false;
        }
    }

    LogPrint("popnode", "CPopnodeBroadcast::CheckOutpoint -- Popnode UTXO verified\n");

    // make sure the vout that was signed is related to the transaction that spawned the Popnode
    //  - this is expensive, so it's only done once per Popnode
    if(!darkSendSigner.IsVinAssociatedWithPubkey(vin, pubKeyCollateralAddress)) {
        LogPrintf("CPopnodeMan::CheckOutpoint -- Got mismatched pubKeyCollateralAddress and vin\n");
        nDos = 33;
        return false;
    }

    // verify that sig time is legit in past
    // should be at least not earlier than block when 10000 PCH tx got nPopnodeMinimumConfirmations
    uint256 hashBlock = uint256();
    CTransaction tx2;
    GetTransaction(vin.prevout.hash, tx2, Params().GetConsensus(), hashBlock, true);
    {
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end() && (*mi).second) {
            CBlockIndex* pMNIndex = (*mi).second; // block for 10000 PCH tx -> 1 confirmation
            CBlockIndex* pConfIndex = chainActive[pMNIndex->nHeight + Params().GetConsensus().nPopnodeMinimumConfirmations - 1]; // block where tx got nPopnodeMinimumConfirmations
            if(pConfIndex->GetBlockTime() > sigTime) {
                LogPrintf("CPopnodeBroadcast::CheckOutpoint -- Bad sigTime %d (%d conf block is at %d) for Popnode %s %s\n",
                          sigTime, Params().GetConsensus().nPopnodeMinimumConfirmations, pConfIndex->GetBlockTime(), vin.prevout.ToStringShort(), addr.ToString());
                return false;
            }
        }
    }

	// check if it is registered on the Pop center server
	CPopnode mn(*this);
	if(!mnodeman.CheckActiveMaster(mn))
	{
		LogPrintf("CPopnodeBroadcast::CheckOutpoint -- Failed to find Popnode in the PopCenter's popnode list, popnode=%s\n", mn.vin.prevout.ToStringShort());
		return false;
	}

    return true;
}

bool CPopnodeBroadcast::Sign(CKey& keyCollateralAddress)
{
    std::string strError;
    std::string strMessage;

    sigTime = GetAdjustedTime();

    strMessage = addr.ToString(false) + boost::lexical_cast<std::string>(sigTime) +
                    pubKeyCollateralAddress.GetID().ToString() + pubKeyPopnode.GetID().ToString() +
                    boost::lexical_cast<std::string>(nProtocolVersion);

    if(!darkSendSigner.SignMessage(strMessage, vchSig, keyCollateralAddress)) {
        LogPrintf("CPopnodeBroadcast::Sign -- SignMessage() failed\n");
        return false;
    }

    if(!darkSendSigner.VerifyMessage(pubKeyCollateralAddress, vchSig, strMessage, strError)) {
        LogPrintf("CPopnodeBroadcast::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CPopnodeBroadcast::CheckSignature(int& nDos)
{
    std::string strMessage;
    std::string strError = "";
    nDos = 0;

    //
    // REMOVE AFTER MIGRATION TO 12.1
    //
    if(nProtocolVersion < 70201) {
        std::string vchPubkeyCollateralAddress(pubKeyCollateralAddress.begin(), pubKeyCollateralAddress.end());
        std::string vchPubkeyPopnode(pubKeyPopnode.begin(), pubKeyPopnode.end());
        strMessage = addr.ToString(false) + boost::lexical_cast<std::string>(sigTime) +
                        vchPubkeyCollateralAddress + vchPubkeyPopnode + boost::lexical_cast<std::string>(nProtocolVersion);

        LogPrint("popnode", "CPopnodeBroadcast::CheckSignature -- sanitized strMessage: %s  pubKeyCollateralAddress address: %s  sig: %s\n",
            SanitizeString(strMessage), CBitcoinAddress(pubKeyCollateralAddress.GetID()).ToString(),
            EncodeBase64(&vchSig[0], vchSig.size()));

        if(!darkSendSigner.VerifyMessage(pubKeyCollateralAddress, vchSig, strMessage, strError)) {
            if(addr.ToString() != addr.ToString(false)) {
                // maybe it's wrong format, try again with the old one
                strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) +
                                vchPubkeyCollateralAddress + vchPubkeyPopnode + boost::lexical_cast<std::string>(nProtocolVersion);

                LogPrint("popnode", "CPopnodeBroadcast::CheckSignature -- second try, sanitized strMessage: %s  pubKeyCollateralAddress address: %s  sig: %s\n",
                    SanitizeString(strMessage), CBitcoinAddress(pubKeyCollateralAddress.GetID()).ToString(),
                    EncodeBase64(&vchSig[0], vchSig.size()));

                if(!darkSendSigner.VerifyMessage(pubKeyCollateralAddress, vchSig, strMessage, strError)) {
                    // didn't work either
                    LogPrintf("CPopnodeBroadcast::CheckSignature -- Got bad Popnode announce signature, second try, sanitized error: %s\n",
                        SanitizeString(strError));
                    // don't ban for old popnodes, their sigs could be broken because of the bug
                    return false;
                }
            } else {
                // nope, sig is actually wrong
                LogPrintf("CPopnodeBroadcast::CheckSignature -- Got bad Popnode announce signature, sanitized error: %s\n",
                    SanitizeString(strError));
                // don't ban for old popnodes, their sigs could be broken because of the bug
                return false;
            }
        }
    } else {
    //
    // END REMOVE
    //
        strMessage = addr.ToString(false) + boost::lexical_cast<std::string>(sigTime) +
                        pubKeyCollateralAddress.GetID().ToString() + pubKeyPopnode.GetID().ToString() +
                        boost::lexical_cast<std::string>(nProtocolVersion);

        LogPrint("popnode", "CPopnodeBroadcast::CheckSignature -- strMessage: %s  pubKeyCollateralAddress address: %s  sig: %s\n", strMessage, CBitcoinAddress(pubKeyCollateralAddress.GetID()).ToString(), EncodeBase64(&vchSig[0], vchSig.size()));

        if(!darkSendSigner.VerifyMessage(pubKeyCollateralAddress, vchSig, strMessage, strError)){
            LogPrintf("CPopnodeBroadcast::CheckSignature -- Got bad Popnode announce signature, error: %s\n", strError);
            nDos = 100;
            return false;
        }
    }

    return true;
}

void CPopnodeBroadcast::Relay()
{
    CInv inv(MSG_POPNODE_ANNOUNCE, GetHash());
    RelayInv(inv);
}

CPopnodePing::CPopnodePing(CTxIn& vinNew)
{
    LOCK(cs_main);
    if (!chainActive.Tip() || chainActive.Height() < 12) return;

    vin = vinNew;
    blockHash = chainActive[chainActive.Height() - 12]->GetBlockHash();
    sigTime = GetAdjustedTime();
    vchSig = std::vector<unsigned char>();
}

bool CPopnodePing::Sign(CKey& keyPopnode, CPubKey& pubKeyPopnode)
{
    std::string strError;
    std::string strPopNodeSignMessage;

    sigTime = GetAdjustedTime();
    std::string strMessage = vin.ToString() + blockHash.ToString() + boost::lexical_cast<std::string>(sigTime);

    if(!darkSendSigner.SignMessage(strMessage, vchSig, keyPopnode)) {
        LogPrintf("CPopnodePing::Sign -- SignMessage() failed\n");
        return false;
    }

    if(!darkSendSigner.VerifyMessage(pubKeyPopnode, vchSig, strMessage, strError)) {
        LogPrintf("CPopnodePing::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CPopnodePing::CheckSignature(CPubKey& pubKeyPopnode, int &nDos)
{
    std::string strMessage = vin.ToString() + blockHash.ToString() + boost::lexical_cast<std::string>(sigTime);
    std::string strError = "";
    nDos = 0;

    if(!darkSendSigner.VerifyMessage(pubKeyPopnode, vchSig, strMessage, strError)) {
        LogPrintf("CPopnodePing::CheckSignature -- Got bad Popnode ping signature, popnode=%s, error: %s\n", vin.prevout.ToStringShort(), strError);
        nDos = 33;
        return false;
    }
    return true;
}

bool CPopnodePing::SimpleCheck(int& nDos)
{
    // don't ban by default
    nDos = 0;

    if (sigTime > GetAdjustedTime() + 60 * 60) {
        LogPrintf("CPopnodePing::SimpleCheck -- Signature rejected, too far into the future, popnode=%s\n", vin.prevout.ToStringShort());
        nDos = 1;
        return false;
    }

    {
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(blockHash);
        if (mi == mapBlockIndex.end()) {
            LogPrint("popnode", "CPopnodePing::SimpleCheck -- Popnode ping is invalid, unknown block hash: popnode=%s blockHash=%s\n", vin.prevout.ToStringShort(), blockHash.ToString());
            // maybe we stuck or forked so we shouldn't ban this node, just fail to accept this ping
            // TODO: or should we also request this block?
            return false;
        }
    }
    LogPrint("popnode", "CPopnodePing::SimpleCheck -- Popnode ping verified: popnode=%s  blockHash=%s  sigTime=%d\n", vin.prevout.ToStringShort(), blockHash.ToString(), sigTime);
    return true;
}

bool CPopnodePing::CheckAndUpdate(CPopnode* pmn, bool fFromNewBroadcast, int& nDos)
{
    // don't ban by default
    nDos = 0;

    if (!SimpleCheck(nDos)) {
        return false;
    }

    if (pmn == NULL) {
        LogPrint("popnode", "CPopnodePing::CheckAndUpdate -- Couldn't find Popnode entry, popnode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    if(!fFromNewBroadcast) {
        if (pmn->IsUpdateRequired()) {
            LogPrint("popnode", "CPopnodePing::CheckAndUpdate -- popnode protocol is outdated, popnode=%s\n", vin.prevout.ToStringShort());
            return false;
        }

        if (pmn->IsNewStartRequired()) {
            LogPrint("popnode", "CPopnodePing::CheckAndUpdate -- popnode is completely expired, new start is required, popnode=%s\n", vin.prevout.ToStringShort());
            return false;
        }
    }

    {
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(blockHash);
        if ((*mi).second && (*mi).second->nHeight < chainActive.Height() - 24) {
            LogPrintf("CPopnodePing::CheckAndUpdate -- Popnode ping is invalid, block hash is too old: popnode=%s  blockHash=%s\n", vin.prevout.ToStringShort(), blockHash.ToString());
            // nDos = 1;
            return false;
        }
    }

    LogPrint("popnode", "CPopnodePing::CheckAndUpdate -- New ping: popnode=%s  blockHash=%s  sigTime=%d\n", vin.prevout.ToStringShort(), blockHash.ToString(), sigTime);

    // LogPrintf("mnping - Found corresponding mn for vin: %s\n", vin.prevout.ToStringShort());
    // update only if there is no known ping for this popnode or
    // last ping was more then POPNODE_MIN_MNP_SECONDS-60 ago comparing to this one
    if (pmn->IsPingedWithin(POPNODE_MIN_MNP_SECONDS - 60, sigTime)) {
        LogPrint("popnode", "CPopnodePing::CheckAndUpdate -- Popnode ping arrived too early, popnode=%s\n", vin.prevout.ToStringShort());
        //nDos = 1; //disable, this is happening frequently and causing banned peers
        return false;
    }

    if (!CheckSignature(pmn->pubKeyPopnode, nDos)) return false;

    // so, ping seems to be ok, let's store it
    LogPrint("popnode", "CPopnodePing::CheckAndUpdate -- Popnode ping accepted, popnode=%s\n", vin.prevout.ToStringShort());
    pmn->lastPing = *this;

    // and update mnodeman.mapSeenPopnodeBroadcast.lastPing which is probably outdated
    CPopnodeBroadcast mnb(*pmn);
    uint256 hash = mnb.GetHash();
    if (mnodeman.mapSeenPopnodeBroadcast.count(hash)) {
        mnodeman.mapSeenPopnodeBroadcast[hash].second.lastPing = *this;
    }

    pmn->Check(true); // force update, ignoring cache
    if (!pmn->IsEnabled()) return false;

    LogPrint("popnode", "CPopnodePing::CheckAndUpdate -- Popnode ping acceepted and relayed, popnode=%s\n", vin.prevout.ToStringShort());
    Relay();

    return true;
}

void CPopnodePing::Relay()
{
    CInv inv(MSG_POPNODE_PING, GetHash());
    RelayInv(inv);
}


void CPopnode::UpdateWatchdogVoteTime()
{
    LOCK(cs);
    nTimeLastWatchdogVote = GetTime();
}
