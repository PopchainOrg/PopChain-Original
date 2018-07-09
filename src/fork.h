// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef FORK_H
#define FORK_H

#include "hash.h"
#include "net.h"
#include "utilstrencodings.h"

class CForkMessage;
class CForkManager;

/*
    Don't ever reuse these IDs for other forks
    - This would result in old clients getting confused about which fork is for what
*/
static const int FORK_START                                            = 10001;
static const int FORK_END                                              = 10018;

static const int FORK_2_INSTANTSEND_ENABLED                            = 10001;
static const int FORK_3_INSTANTSEND_BLOCK_FILTERING                    = 10002;
static const int FORK_5_INSTANTSEND_MAX_VALUE                          = 10004;
static const int FORK_9_SUPERBLOCKS_ENABLED                            = 10008;
static const int FORK_12_RECONSIDER_BLOCKS                             = 10011;
static const int FORK_13_OLD_SUPERBLOCK_FLAG                           = 10012;
static const int FORK_14_REQUIRE_SENTINEL_FLAG                         = 10013;
static const int FORK_18_REQUIRE_MASTER_VERIFY_FLAG                    = 10018;

static const int64_t FORK_2_INSTANTSEND_ENABLED_DEFAULT                = 0;            // ON
static const int64_t FORK_3_INSTANTSEND_BLOCK_FILTERING_DEFAULT        = 0;            // ON
static const int64_t FORK_5_INSTANTSEND_MAX_VALUE_DEFAULT              = 1000;         // 1000 PCH
static const int64_t FORK_9_SUPERBLOCKS_ENABLED_DEFAULT                = 0;		// ON
static const int64_t FORK_12_RECONSIDER_BLOCKS_DEFAULT                 = 0;            // 0 BLOCKS
static const int64_t FORK_13_OLD_SUPERBLOCK_FLAG_DEFAULT               = 4070908800ULL;// OFF
static const int64_t FORK_14_REQUIRE_SENTINEL_FLAG_DEFAULT             = 4070908800ULL;// OFF
static const int64_t FORK_18_REQUIRE_MASTER_VERIFY_FLAG_DEFAULT        = 1519894519;// ON

extern std::map<uint256, CForkMessage> mapForks;
extern CForkManager forkManager;

//
// Fork classes
// Keep track of all of the network fork settings
//

class CForkMessage
{
private:
    std::vector<unsigned char> vchSig;

public:
    int nForkID;
    int64_t nValue;
    int64_t nTimeSigned;

    CForkMessage(int nForkID, int64_t nValue, int64_t nTimeSigned) :
        nForkID(nForkID),
        nValue(nValue),
        nTimeSigned(nTimeSigned)
        {}

    CForkMessage() :
        nForkID(0),
        nValue(0),
        nTimeSigned(0)
        {}


    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(nForkID);
        READWRITE(nValue);
        READWRITE(nTimeSigned);
        READWRITE(vchSig);
    }

    uint256 GetHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << nForkID;
        ss << nValue;
        ss << nTimeSigned;
        return ss.GetHash();
    }

    bool Sign(std::string strSignKey);
    bool CheckSignature();
    void Relay();
};


class CForkManager
{
private:
    std::vector<unsigned char> vchSig;
    std::string strMasterPrivKey;
    std::map<int, CForkMessage> mapForksActive;

public:

    CForkManager() {}

    void ProcessFork(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
    void ExecuteFork(int nForkID, int nValue);
    bool UpdateFork(int nForkID, int64_t nValue);

    bool IsForkActive(int nForkID);
    int64_t GetForkValue(int nForkID);
    int GetForkIDByName(std::string strName);
    std::string GetForkNameByID(int nForkID);

    bool SetPrivKey(std::string strPrivKey);
};

#endif
