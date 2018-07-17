// Copyright (c) 2017-2018 The Popchain Core Developers

#include "popsend.h"
#include "main.h"
#include "fork.h"

#include <boost/lexical_cast.hpp>

class CForkMessage;
class CForkManager;

CForkManager forkManager;

std::map<uint256, CForkMessage> mapForks;

void CForkManager::ProcessFork(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if(fLiteMode) return; // disable all Pop specific functionality

    if (strCommand == NetMsgType::FORK) {

        CDataStream vMsg(vRecv);
        CForkMessage fork;
        vRecv >> fork;

        uint256 hash = fork.GetHash();

        std::string strLogMsg;
        {
            LOCK(cs_main);
            pfrom->setAskFor.erase(hash);
            if(!chainActive.Tip()) return;
            strLogMsg = strprintf("FORK -- hash: %s id: %d value: %10d bestHeight: %d peer=%d", hash.ToString(), fork.nForkID, fork.nValue, chainActive.Height(), pfrom->id);
        }

        if(mapForksActive.count(fork.nForkID)) {
            if (mapForksActive[fork.nForkID].nTimeSigned >= fork.nTimeSigned) {
                LogPrint("fork", "%s seen\n", strLogMsg);
                return;
            } else {
                LogPrintf("%s updated\n", strLogMsg);
            }
        } else {
            LogPrintf("%s new\n", strLogMsg);
        }

        if(!fork.CheckSignature()) {
            LogPrintf("CForkManager::ProcessFork -- invalid signature\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        mapForks[hash] = fork;
        mapForksActive[fork.nForkID] = fork;
        fork.Relay();

        //does a task if needed
        ExecuteFork(fork.nForkID, fork.nValue);

    } else if (strCommand == NetMsgType::GETFORKS) {

        std::map<int, CForkMessage>::iterator it = mapForksActive.begin();

        while(it != mapForksActive.end()) {
            pfrom->PushMessage(NetMsgType::FORK, it->second);
            it++;
        }
    }

}

void CForkManager::ExecuteFork(int nForkID, int nValue)
{
    //correct fork via fork technology
    if(nForkID == FORK_12_RECONSIDER_BLOCKS && nValue > 0) {
        // allow to reprocess 24h of blocks max, which should be enough to resolve any issues
        int64_t nMaxBlocks = 576;
        // this potentially can be a heavy operation, so only allow this to be executed once per 10 minutes
        int64_t nTimeout = 10 * 60;

        static int64_t nTimeExecuted = 0; // i.e. it was never executed before

        if(GetTime() - nTimeExecuted < nTimeout) {
            LogPrint("fork", "CForkManager::ExecuteFork -- ERROR: Trying to reconsider blocks, too soon - %d/%d\n", GetTime() - nTimeExecuted, nTimeout);
            return;
        }

        if(nValue > nMaxBlocks) {
            LogPrintf("CForkManager::ExecuteFork -- ERROR: Trying to reconsider too many blocks %d/%d\n", nValue, nMaxBlocks);
            return;
        }


        LogPrintf("CForkManager::ExecuteFork -- Reconsider Last %d Blocks\n", nValue);

        ReprocessBlocks(nValue);
        nTimeExecuted = GetTime();
    }
}

bool CForkManager::UpdateFork(int nForkID, int64_t nValue)
{

    CForkMessage fork = CForkMessage(nForkID, nValue, GetTime());

    if(fork.Sign(strMasterPrivKey)) {
        fork.Relay();
        mapForks[fork.GetHash()] = fork;
        mapForksActive[nForkID] = fork;
        return true;
    }

    return false;
}

// grab the fork, otherwise say it's off
// Popchain DevTeam
bool CForkManager::IsForkActive(int nForkID)
{
    int64_t r = -1;

    if(mapForksActive.count(nForkID)){
        r = mapForksActive[nForkID].nValue;
    } else {
        switch (nForkID) {
            case FORK_2_INSTANTSEND_ENABLED:               r = FORK_2_INSTANTSEND_ENABLED_DEFAULT; break;
            case FORK_3_INSTANTSEND_BLOCK_FILTERING:       r = FORK_3_INSTANTSEND_BLOCK_FILTERING_DEFAULT; break;
            case FORK_5_INSTANTSEND_MAX_VALUE:             r = FORK_5_INSTANTSEND_MAX_VALUE_DEFAULT; break;
            case FORK_9_SUPERBLOCKS_ENABLED:               r = FORK_9_SUPERBLOCKS_ENABLED_DEFAULT; break;
            case FORK_12_RECONSIDER_BLOCKS:                r = FORK_12_RECONSIDER_BLOCKS_DEFAULT; break;
            case FORK_13_OLD_SUPERBLOCK_FLAG:              r = FORK_13_OLD_SUPERBLOCK_FLAG_DEFAULT; break;
            case FORK_14_REQUIRE_SENTINEL_FLAG:            r = FORK_14_REQUIRE_SENTINEL_FLAG_DEFAULT; break;
	    case FORK_18_REQUIRE_MASTER_VERIFY_FLAG:       r = FORK_18_REQUIRE_MASTER_VERIFY_FLAG_DEFAULT; break;
            default:
                LogPrint("fork", "CForkManager::IsForkActive -- Unknown Fork ID %d\n", nForkID);
                r = 4070908800ULL; // 2099-1-1 i.e. off by default
                break;
        }
    }

    return r < GetTime();
}

// grab the value of the fork on the network, or the default
// Popchain DevTeam
int64_t CForkManager::GetForkValue(int nForkID)
{
    if (mapForksActive.count(nForkID))
        return mapForksActive[nForkID].nValue;

    switch (nForkID) {
        case FORK_2_INSTANTSEND_ENABLED:               return FORK_2_INSTANTSEND_ENABLED_DEFAULT;
        case FORK_3_INSTANTSEND_BLOCK_FILTERING:       return FORK_3_INSTANTSEND_BLOCK_FILTERING_DEFAULT;
        case FORK_5_INSTANTSEND_MAX_VALUE:             return FORK_5_INSTANTSEND_MAX_VALUE_DEFAULT;
        case FORK_9_SUPERBLOCKS_ENABLED:               return FORK_9_SUPERBLOCKS_ENABLED_DEFAULT;
        case FORK_12_RECONSIDER_BLOCKS:                return FORK_12_RECONSIDER_BLOCKS_DEFAULT;
        case FORK_13_OLD_SUPERBLOCK_FLAG:              return FORK_13_OLD_SUPERBLOCK_FLAG_DEFAULT;
        case FORK_14_REQUIRE_SENTINEL_FLAG:            return FORK_14_REQUIRE_SENTINEL_FLAG_DEFAULT;
        case FORK_18_REQUIRE_MASTER_VERIFY_FLAG:       return FORK_18_REQUIRE_MASTER_VERIFY_FLAG_DEFAULT;
        default:
            LogPrint("fork", "CForkManager::GetForkValue -- Unknown Fork ID %d\n", nForkID);
            return -1;
    }

}

// Popchain DevTeam
int CForkManager::GetForkIDByName(std::string strName)
{
    if (strName == "FORK_2_INSTANTSEND_ENABLED")               return FORK_2_INSTANTSEND_ENABLED;
    if (strName == "FORK_3_INSTANTSEND_BLOCK_FILTERING")       return FORK_3_INSTANTSEND_BLOCK_FILTERING;
    if (strName == "FORK_5_INSTANTSEND_MAX_VALUE")             return FORK_5_INSTANTSEND_MAX_VALUE;
    if (strName == "FORK_9_SUPERBLOCKS_ENABLED")               return FORK_9_SUPERBLOCKS_ENABLED;
    if (strName == "FORK_12_RECONSIDER_BLOCKS")                return FORK_12_RECONSIDER_BLOCKS;
    if (strName == "FORK_13_OLD_SUPERBLOCK_FLAG")              return FORK_13_OLD_SUPERBLOCK_FLAG;
    if (strName == "FORK_14_REQUIRE_SENTINEL_FLAG")            return FORK_14_REQUIRE_SENTINEL_FLAG;
    if (strName == "FORK_18_REQUIRE_MASTER_VERIFY_FLAG")       return FORK_18_REQUIRE_MASTER_VERIFY_FLAG;
    LogPrint("fork", "CForkManager::GetForkIDByName -- Unknown Fork name '%s'\n", strName);
    return -1;
}

// Popchain DevTeam
std::string CForkManager::GetForkNameByID(int nForkID)
{
    switch (nForkID) {
        case FORK_2_INSTANTSEND_ENABLED:               return "FORK_2_INSTANTSEND_ENABLED";
        case FORK_3_INSTANTSEND_BLOCK_FILTERING:       return "FORK_3_INSTANTSEND_BLOCK_FILTERING";
        case FORK_5_INSTANTSEND_MAX_VALUE:             return "FORK_5_INSTANTSEND_MAX_VALUE";
        case FORK_9_SUPERBLOCKS_ENABLED:               return "FORK_9_SUPERBLOCKS_ENABLED";
        case FORK_12_RECONSIDER_BLOCKS:                return "FORK_12_RECONSIDER_BLOCKS";
        case FORK_13_OLD_SUPERBLOCK_FLAG:              return "FORK_13_OLD_SUPERBLOCK_FLAG";
        case FORK_14_REQUIRE_SENTINEL_FLAG:            return "FORK_14_REQUIRE_SENTINEL_FLAG";
        case FORK_18_REQUIRE_MASTER_VERIFY_FLAG:       return "FORK_18_REQUIRE_MASTER_VERIFY_FLAG";
        default:
            LogPrint("fork", "CForkManager::GetForkNameByID -- Unknown Fork ID %d\n", nForkID);
            return "Unknown";
    }
}

bool CForkManager::SetPrivKey(std::string strPrivKey)
{
    CForkMessage fork;

    fork.Sign(strPrivKey);

    if(fork.CheckSignature()){
        // Test signing successful, proceed
        LogPrintf("CForkManager::SetPrivKey -- Successfully initialized as fork signer\n");
        strMasterPrivKey = strPrivKey;
        return true;
    } else {
        return false;
    }
}

bool CForkMessage::Sign(std::string strSignKey)
{
    CKey key;
    CPubKey pubkey;
    std::string strError = "";
    std::string strMessage = boost::lexical_cast<std::string>(nForkID) + boost::lexical_cast<std::string>(nValue) + boost::lexical_cast<std::string>(nTimeSigned);

    if(!darkSendSigner.GetKeysFromSecret(strSignKey, key, pubkey)) {
        LogPrintf("CForkMessage::Sign -- GetKeysFromSecret() failed, invalid fork key %s\n", strSignKey);
        return false;
    }

    if(!darkSendSigner.SignMessage(strMessage, vchSig, key)) {
        LogPrintf("CForkMessage::Sign -- SignMessage() failed\n");
        return false;
    }

    if(!darkSendSigner.VerifyMessage(pubkey, vchSig, strMessage, strError)) {
        LogPrintf("CForkMessage::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

void CForkMessage::Relay()
{
    CInv inv(MSG_FORK, GetHash());
    RelayInv(inv);
}

bool CForkMessage::CheckSignature()
{
    //note: need to investigate why this is failing
    std::string strError = "";
    std::string strMessage = boost::lexical_cast<std::string>(nForkID) + boost::lexical_cast<std::string>(nValue) + boost::lexical_cast<std::string>(nTimeSigned);
    CPubKey pubkey(ParseHex(Params().ForkPubKey()));

    if(!darkSendSigner.VerifyMessage(pubkey, vchSig, strMessage, strError)) {
        LogPrintf("CForkMessage::CheckSignature -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

