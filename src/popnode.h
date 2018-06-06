// Copyright (c) 2017-2018 The Popchain Core Developers


#ifndef POPNODE_H
#define POPNODE_H

#include "key.h"
#include "main.h"
#include "net.h"
#include "spork.h"
#include "timedata.h"

class CPopnode;
class CPopnodeBroadcast;
class CPopnodePing;

static const int POPNODE_CHECK_SECONDS               =   5;
static const int POPNODE_MIN_MNB_SECONDS             =   5 * 60;
static const int POPNODE_MIN_MNP_SECONDS             =  10 * 60;
static const int POPNODE_EXPIRATION_SECONDS          =  65 * 60;
static const int POPNODE_WATCHDOG_MAX_SECONDS        = 120 * 60;
static const int POPNODE_NEW_START_REQUIRED_SECONDS  = 180 * 60;

static const int POPNODE_POSE_BAN_MAX_SCORE          = 5;
//
// The Popnode Ping Class : Contains a different serialize method for sending pings from popnodes throughout the network
//

class CPopnodePing
{
public:
    CTxIn vin;
    uint256 blockHash;
    int64_t sigTime; //mnb message times
    std::vector<unsigned char> vchSig;
    //removed stop

    CPopnodePing() :
        vin(),
        blockHash(),
        sigTime(0),
        vchSig()
        {}

    CPopnodePing(CTxIn& vinNew);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(vin);
        READWRITE(blockHash);
        READWRITE(sigTime);
        READWRITE(vchSig);
    }

    void swap(CPopnodePing& first, CPopnodePing& second) // nothrow
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.vin, second.vin);
        swap(first.blockHash, second.blockHash);
        swap(first.sigTime, second.sigTime);
        swap(first.vchSig, second.vchSig);
    }

    uint256 GetHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << vin;
        ss << sigTime;
        return ss.GetHash();
    }

    bool IsExpired() { return GetTime() - sigTime > POPNODE_NEW_START_REQUIRED_SECONDS; }

    bool Sign(CKey& keyPopnode, CPubKey& pubKeyPopnode);
    bool CheckSignature(CPubKey& pubKeyPopnode, int &nDos);
    bool SimpleCheck(int& nDos);
    bool CheckAndUpdate(CPopnode* pmn, bool fFromNewBroadcast, int& nDos);
    void Relay();

    CPopnodePing& operator=(CPopnodePing from)
    {
        swap(*this, from);
        return *this;
    }
    friend bool operator==(const CPopnodePing& a, const CPopnodePing& b)
    {
        return a.vin == b.vin && a.blockHash == b.blockHash;
    }
    friend bool operator!=(const CPopnodePing& a, const CPopnodePing& b)
    {
        return !(a == b);
    }

};

struct popnode_info_t
{
    popnode_info_t()
        : vin(),
          addr(),
          pubKeyCollateralAddress(),
          pubKeyPopnode(),
          sigTime(0),
          nLastDsq(0),
          nTimeLastChecked(0),
          nTimeLastPaid(0),
          nTimeLastWatchdogVote(0),
          nActiveState(0),
          nProtocolVersion(0),
          fInfoValid(false)
        {}

    CTxIn vin;
    CService addr;
    CPubKey pubKeyCollateralAddress;
    CPubKey pubKeyPopnode;
    int64_t sigTime; //mnb message time
    int64_t nLastDsq; //the dsq count from the last dsq broadcast of this node
    int64_t nTimeLastChecked;
    int64_t nTimeLastPaid;
    int64_t nTimeLastWatchdogVote;
    int nActiveState;
    int nProtocolVersion;
    bool fInfoValid;
};

//
// The Popnode Class. For managing the Darksend process. It contains the input of the 10000 PCH, signature to prove
// it's the one who own that ip address and code for calculating the payment election.
//
class CPopnode
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

	int MNM_REGISTERED_CHECK_SECONDS   = 60 * 60;
	
public:
    enum state {
        POPNODE_PRE_ENABLED,
        POPNODE_ENABLED,
        POPNODE_EXPIRED,
        POPNODE_OUTPOINT_SPENT,
        POPNODE_UPDATE_REQUIRED,
        POPNODE_WATCHDOG_EXPIRED,
        POPNODE_NEW_START_REQUIRED,
        POPNODE_POSE_BAN,
        POPNODE_NO_REGISTERED
    };

    CTxIn vin;
    CService addr;
    CPubKey pubKeyCollateralAddress;
    CPubKey pubKeyPopnode;
    CPopnodePing lastPing;
    std::vector<unsigned char> vchSig;
    int64_t sigTime; //mnb message time
    int64_t nLastDsq; //the dsq count from the last dsq broadcast of this node
    int64_t nTimeLastChecked;
	int64_t nTimeLastCheckedRegistered;
    int64_t nTimeLastPaid;
    int64_t nTimeLastWatchdogVote;
    int nActiveState;
    int nCacheCollateralBlock;
    int nBlockLastPaid;
    int nProtocolVersion;
    int nPoSeBanScore;
    int nPoSeBanHeight;
    bool fAllowMixingTx;
    bool fUnitTest;

    CPopnode();
    CPopnode(const CPopnode& other);
    CPopnode(const CPopnodeBroadcast& mnb);
    CPopnode(CService addrNew, CTxIn vinNew, CPubKey pubKeyCollateralAddressNew, CPubKey pubKeyPopnodeNew, int nProtocolVersionIn);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        LOCK(cs);
        READWRITE(vin);
        READWRITE(addr);
        READWRITE(pubKeyCollateralAddress);
        READWRITE(pubKeyPopnode);
        READWRITE(lastPing);
        READWRITE(vchSig);
        READWRITE(sigTime);
        READWRITE(nLastDsq);
        READWRITE(nTimeLastChecked);
		READWRITE(nTimeLastCheckedRegistered);
        READWRITE(nTimeLastPaid);
        READWRITE(nTimeLastWatchdogVote);
        READWRITE(nActiveState);
        READWRITE(nCacheCollateralBlock);
        READWRITE(nBlockLastPaid);
        READWRITE(nProtocolVersion);
        READWRITE(nPoSeBanScore);
        READWRITE(nPoSeBanHeight);
        READWRITE(fAllowMixingTx);
        READWRITE(fUnitTest);
    }

    void swap(CPopnode& first, CPopnode& second) // nothrow
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.vin, second.vin);
        swap(first.addr, second.addr);
        swap(first.pubKeyCollateralAddress, second.pubKeyCollateralAddress);
        swap(first.pubKeyPopnode, second.pubKeyPopnode);
        swap(first.lastPing, second.lastPing);
        swap(first.vchSig, second.vchSig);
        swap(first.sigTime, second.sigTime);
        swap(first.nLastDsq, second.nLastDsq);
        swap(first.nTimeLastChecked, second.nTimeLastChecked);
		swap(first.nTimeLastCheckedRegistered, second.nTimeLastCheckedRegistered);
        swap(first.nTimeLastPaid, second.nTimeLastPaid);
        swap(first.nTimeLastWatchdogVote, second.nTimeLastWatchdogVote);
        swap(first.nActiveState, second.nActiveState);
        swap(first.nCacheCollateralBlock, second.nCacheCollateralBlock);
        swap(first.nBlockLastPaid, second.nBlockLastPaid);
        swap(first.nProtocolVersion, second.nProtocolVersion);
        swap(first.nPoSeBanScore, second.nPoSeBanScore);
        swap(first.nPoSeBanHeight, second.nPoSeBanHeight);
        swap(first.fAllowMixingTx, second.fAllowMixingTx);
        swap(first.fUnitTest, second.fUnitTest);
    }

    // CALCULATE A RANK AGAINST OF GIVEN BLOCK
    arith_uint256 CalculateScore(const uint256& blockHash);

    bool UpdateFromNewBroadcast(CPopnodeBroadcast& mnb);

    void Check(bool fForce = false);

    bool IsBroadcastedWithin(int nSeconds) { return GetAdjustedTime() - sigTime < nSeconds; }

    bool IsPingedWithin(int nSeconds, int64_t nTimeToCheckAt = -1)
    {
        if(lastPing == CPopnodePing()) return false;

        if(nTimeToCheckAt == -1) {
            nTimeToCheckAt = GetAdjustedTime();
        }
        return nTimeToCheckAt - lastPing.sigTime < nSeconds;
    }

    bool IsEnabled() { return nActiveState == POPNODE_ENABLED; }
    bool IsPreEnabled() { return nActiveState == POPNODE_PRE_ENABLED; }
    bool IsPoSeBanned() { return nActiveState == POPNODE_POSE_BAN; }
    // NOTE: this one relies on nPoSeBanScore, not on nActiveState as everything else here
    bool IsPoSeVerified() { return nPoSeBanScore <= -POPNODE_POSE_BAN_MAX_SCORE; }
    bool IsExpired() { return nActiveState == POPNODE_EXPIRED; }
    bool IsOutpointSpent() { return nActiveState == POPNODE_OUTPOINT_SPENT; }
    bool IsUpdateRequired() { return nActiveState == POPNODE_UPDATE_REQUIRED; }
    bool IsWatchdogExpired() { return nActiveState == POPNODE_WATCHDOG_EXPIRED; }
    bool IsNewStartRequired() { return nActiveState == POPNODE_NEW_START_REQUIRED; }
	bool IsRegistered() { return nActiveState == POPNODE_NO_REGISTERED; }

	void SetRegisteredCheckInterval(int time) { MNM_REGISTERED_CHECK_SECONDS = time * 60; }

    static bool IsValidStateForAutoStart(int nActiveStateIn)
    {
        return  nActiveStateIn == POPNODE_ENABLED ||
                nActiveStateIn == POPNODE_PRE_ENABLED ||
                nActiveStateIn == POPNODE_EXPIRED ||
                nActiveStateIn == POPNODE_WATCHDOG_EXPIRED;
    }

    bool IsValidForPayment()
    {
        if(nActiveState == POPNODE_ENABLED) {
            return true;
        }
        if(!sporkManager.IsSporkActive(SPORK_14_REQUIRE_SENTINEL_FLAG) &&
           (nActiveState == POPNODE_WATCHDOG_EXPIRED)) {
            return true;
        }

        return false;
    }

    bool IsValidNetAddr();
    static bool IsValidNetAddr(CService addrIn);

    void IncreasePoSeBanScore() { if(nPoSeBanScore < POPNODE_POSE_BAN_MAX_SCORE) nPoSeBanScore++; }
    void DecreasePoSeBanScore() { if(nPoSeBanScore > -POPNODE_POSE_BAN_MAX_SCORE) nPoSeBanScore--; }

    popnode_info_t GetInfo();

    static std::string StateToString(int nStateIn);
    std::string GetStateString() const;
    std::string GetStatus() const;

    int GetCollateralAge();

    int GetLastPaidTime() { return nTimeLastPaid; }
    int GetLastPaidBlock() { return nBlockLastPaid; }

    void UpdateWatchdogVoteTime();

    CPopnode& operator=(CPopnode from)
    {
        swap(*this, from);
        return *this;
    }
    friend bool operator==(const CPopnode& a, const CPopnode& b)
    {
        return a.vin == b.vin;
    }
    friend bool operator!=(const CPopnode& a, const CPopnode& b)
    {
        return !(a.vin == b.vin);
    }

};


//
// The Popnode Broadcast Class : Contains a different serialize method for sending popnodes through the network
//

class CPopnodeBroadcast : public CPopnode
{
public:

    bool fRecovery;

    CPopnodeBroadcast() : CPopnode(), fRecovery(false) {}
    CPopnodeBroadcast(const CPopnode& mn) : CPopnode(mn), fRecovery(false) {}
    CPopnodeBroadcast(CService addrNew, CTxIn vinNew, CPubKey pubKeyCollateralAddressNew, CPubKey pubKeyPopnodeNew, int nProtocolVersionIn) :
        CPopnode(addrNew, vinNew, pubKeyCollateralAddressNew, pubKeyPopnodeNew, nProtocolVersionIn), fRecovery(false) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(vin);
        READWRITE(addr);
        READWRITE(pubKeyCollateralAddress);
        READWRITE(pubKeyPopnode);
        READWRITE(vchSig);
        READWRITE(sigTime);
        READWRITE(nProtocolVersion);
        READWRITE(lastPing);
    }

    uint256 GetHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        //
        // REMOVE AFTER MIGRATION TO 12.1
        //
        if(nProtocolVersion < 70201) {
            ss << sigTime;
            ss << pubKeyCollateralAddress;
        } else {
        //
        // END REMOVE
        //
            ss << vin;
            ss << pubKeyCollateralAddress;
            ss << sigTime;
        }
        return ss.GetHash();
    }

    /// Create Popnode broadcast, needs to be relayed manually after that
    static bool Create(CTxIn vin, CService service, CKey keyCollateralAddressNew, CPubKey pubKeyCollateralAddressNew, CKey keyPopnodeNew, CPubKey pubKeyPopnodeNew, std::string &strErrorRet, CPopnodeBroadcast &mnbRet);
    static bool Create(std::string strService, std::string strKey, std::string strTxHash, std::string strOutputIndex, std::string& strErrorRet, CPopnodeBroadcast &mnbRet, bool fOffline = false);

    bool SimpleCheck(int& nDos);
    bool Update(CPopnode* pmn, int& nDos);
    bool CheckOutpoint(int& nDos);

    bool Sign(CKey& keyCollateralAddress);
    bool CheckSignature(int& nDos);
    void Relay();
};

class CPopnodeVerification
{
public:
    CTxIn vin1;
    CTxIn vin2;
    CService addr;
    int nonce;
    int nBlockHeight;
    std::vector<unsigned char> vchSig1;
    std::vector<unsigned char> vchSig2;

    CPopnodeVerification() :
        vin1(),
        vin2(),
        addr(),
        nonce(0),
        nBlockHeight(0),
        vchSig1(),
        vchSig2()
        {}

    CPopnodeVerification(CService addr, int nonce, int nBlockHeight) :
        vin1(),
        vin2(),
        addr(addr),
        nonce(nonce),
        nBlockHeight(nBlockHeight),
        vchSig1(),
        vchSig2()
        {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(vin1);
        READWRITE(vin2);
        READWRITE(addr);
        READWRITE(nonce);
        READWRITE(nBlockHeight);
        READWRITE(vchSig1);
        READWRITE(vchSig2);
    }

    uint256 GetHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << vin1;
        ss << vin2;
        ss << addr;
        ss << nonce;
        ss << nBlockHeight;
        return ss.GetHash();
    }

    void Relay() const
    {
        CInv inv(MSG_POPNODE_VERIFY, GetHash());
        RelayInv(inv);
    }
};

#endif
