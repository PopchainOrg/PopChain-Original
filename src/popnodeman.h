// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef POPNODEMAN_H
#define POPNODEMAN_H

#include "popnode.h"
#include "sync.h"

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>


using namespace std;

class CPopnodeMan;

extern CPopnodeMan mnodeman;
extern CService ucenterservice;


/**
 * Provides a forward and reverse index between MN vin's and integers.
 *
 * This mapping is normally add-only and is expected to be permanent
 * It is only rebuilt if the size of the index exceeds the expected maximum number
 * of MN's and the current number of known MN's.
 *
 * The external interface to this index is provided via delegation by CPopnodeMan
 */
class CPopnodeIndex
{
public: // Types
    typedef std::map<CTxIn,int> index_m_t;

    typedef index_m_t::iterator index_m_it;

    typedef index_m_t::const_iterator index_m_cit;

    typedef std::map<int,CTxIn> rindex_m_t;

    typedef rindex_m_t::iterator rindex_m_it;

    typedef rindex_m_t::const_iterator rindex_m_cit;

private:
    int                  nSize;

    index_m_t            mapIndex;

    rindex_m_t           mapReverseIndex;

public:
    CPopnodeIndex();

    int GetSize() const {
        return nSize;
    }

    /// Retrieve popnode vin by index
    bool Get(int nIndex, CTxIn& vinPopnode) const;

    /// Get index of a popnode vin
    int GetPopnodeIndex(const CTxIn& vinPopnode) const;

    void AddPopnodeVIN(const CTxIn& vinPopnode);

    void Clear();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(mapIndex);
        if(ser_action.ForRead()) {
            RebuildIndex();
        }
    }

private:
    void RebuildIndex();

};

class CPopnodeMan
{
public:
    typedef std::map<CTxIn,int> index_m_t;

    typedef index_m_t::iterator index_m_it;

    typedef index_m_t::const_iterator index_m_cit;

private:
    static const int MAX_EXPECTED_INDEX_SIZE = 30000;

    /// Only allow 1 index rebuild per hour
    static const int64_t MIN_INDEX_REBUILD_TIME = 3600;

    static const std::string SERIALIZATION_VERSION_STRING;

    static const int DSEG_UPDATE_SECONDS        = 3 * 60 * 60;

    static const int LAST_PAID_SCAN_BLOCKS      = 100;

    static const int MIN_POSE_PROTO_VERSION     = 70203;
    static const int MAX_POSE_RANK              = 10;
    static const int MAX_POSE_BLOCKS            = 10;

    static const int MNB_RECOVERY_QUORUM_TOTAL      = 10;
    static const int MNB_RECOVERY_QUORUM_REQUIRED   = 6;
    static const int MNB_RECOVERY_MAX_ASK_ENTRIES   = 10;
    static const int MNB_RECOVERY_WAIT_SECONDS      = 60;
    static const int MNB_RECOVERY_RETRY_SECONDS     = 3 * 60 * 60;


    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    // Keep track of current block index
    const CBlockIndex *pCurrentBlockIndex;

    // map to hold all MNs
    std::vector<CPopnode> vPopnodes;
    // who's asked for the Popnode list and the last time
    std::map<CNetAddr, int64_t> mAskedUsForPopnodeList;
    // who we asked for the Popnode list and the last time
    std::map<CNetAddr, int64_t> mWeAskedForPopnodeList;
    // which Popnodes we've asked for
    std::map<COutPoint, std::map<CNetAddr, int64_t> > mWeAskedForPopnodeListEntry;
    // who we asked for the popnode verification
    std::map<CNetAddr, CPopnodeVerification> mWeAskedForVerification;

    // these maps are used for popnode recovery from POPNODE_NEW_START_REQUIRED state
    std::map<uint256, std::pair< int64_t, std::set<CNetAddr> > > mMnbRecoveryRequests;
    std::map<uint256, std::vector<CPopnodeBroadcast> > mMnbRecoveryGoodReplies;
    std::list< std::pair<CService, uint256> > listScheduledMnbRequestConnections;

    int64_t nLastIndexRebuildTime;

    CPopnodeIndex indexPopnodes;

    CPopnodeIndex indexPopnodesOld;

    /// Set when index has been rebuilt, clear when read
    bool fIndexRebuilt;

    bool fPopnodesAdded;

    bool fPopnodesRemoved;

    int64_t nLastWatchdogVoteTime;

    friend class CPopnodeSync;

public:
    // Keep track of all broadcasts I've seen
    std::map<uint256, std::pair<int64_t, CPopnodeBroadcast> > mapSeenPopnodeBroadcast;
    // Keep track of all pings I've seen
    std::map<uint256, CPopnodePing> mapSeenPopnodePing;
    // Keep track of all verifications I've seen
    std::map<uint256, CPopnodeVerification> mapSeenPopnodeVerification;
    // keep track of dsq count to prevent popnodes from gaming darksend queue
    int64_t nDsqCount;


    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        LOCK(cs);
        std::string strVersion;
        if(ser_action.ForRead()) {
            READWRITE(strVersion);
        }
        else {
            strVersion = SERIALIZATION_VERSION_STRING; 
            READWRITE(strVersion);
        }

        READWRITE(vPopnodes);
        READWRITE(mAskedUsForPopnodeList);
        READWRITE(mWeAskedForPopnodeList);
        READWRITE(mWeAskedForPopnodeListEntry);
        READWRITE(mMnbRecoveryRequests);
        READWRITE(mMnbRecoveryGoodReplies);
        READWRITE(nLastWatchdogVoteTime);
        READWRITE(nDsqCount);

        READWRITE(mapSeenPopnodeBroadcast);
        READWRITE(mapSeenPopnodePing);
        READWRITE(indexPopnodes);
        if(ser_action.ForRead() && (strVersion != SERIALIZATION_VERSION_STRING)) {
            Clear();
        }
    }

    CPopnodeMan();

    /// Add an entry
    bool Add(CPopnode &mn);
    
    /// Check and activate the pop node.
    bool CheckActiveMaster(CPopnode &mn);

    /// Ask (source) node for mnb
    void AskForMN(CNode *pnode, const CTxIn &vin);
    void AskForMnb(CNode *pnode, const uint256 &hash);

	///for test
	void SetRegisteredCheckInterval(int time);

    /// Check all Popnodes
    void Check();

    /// Check all Popnodes and remove inactive
    void CheckAndRemove();

    /// Clear Popnode vector
    void Clear();
	
    /// Count enabled Popnodes filtered by nProtocolVersion.
    /// Popnode nProtocolVersion should match or be above the one specified in param here.
    int CountEnabled(int nProtocolVersion = -1);

    /// Count Popnodes by network type - NET_IPV4, NET_IPV6, NET_TOR
    // int CountByIP(int nNetworkType);

    void DsegUpdate(CNode* pnode);

    /// Find an entry
    CPopnode* Find(const CScript &payee);
    CPopnode* Find(const CTxIn& vin);
    CPopnode* Find(const CPubKey& pubKeyPopnode);

    /// Versions of Find that are safe to use from outside the class
    bool Get(const CPubKey& pubKeyPopnode, CPopnode& popnode);
    bool Get(const CTxIn& vin, CPopnode& popnode);

    /// Retrieve popnode vin by index
    bool Get(int nIndex, CTxIn& vinPopnode, bool& fIndexRebuiltOut) {
        LOCK(cs);
        fIndexRebuiltOut = fIndexRebuilt;
        return indexPopnodes.Get(nIndex, vinPopnode);
    }

    bool GetIndexRebuiltFlag() {
        LOCK(cs);
        return fIndexRebuilt;
    }

    /// Get index of a popnode vin
    int GetPopnodeIndex(const CTxIn& vinPopnode) {
        LOCK(cs);
        return indexPopnodes.GetPopnodeIndex(vinPopnode);
    }

    /// Get old index of a popnode vin
    int GetPopnodeIndexOld(const CTxIn& vinPopnode) {
        LOCK(cs);
        return indexPopnodesOld.GetPopnodeIndex(vinPopnode);
    }

    /// Get popnode VIN for an old index value
    bool GetPopnodeVinForIndexOld(int nPopnodeIndex, CTxIn& vinPopnodeOut) {
        LOCK(cs);
        return indexPopnodesOld.Get(nPopnodeIndex, vinPopnodeOut);
    }

    /// Get index of a popnode vin, returning rebuild flag
    int GetPopnodeIndex(const CTxIn& vinPopnode, bool& fIndexRebuiltOut) {
        LOCK(cs);
        fIndexRebuiltOut = fIndexRebuilt;
        return indexPopnodes.GetPopnodeIndex(vinPopnode);
    }

    void ClearOldPopnodeIndex() {
        LOCK(cs);
        indexPopnodesOld.Clear();
        fIndexRebuilt = false;
    }

    bool Has(const CTxIn& vin);

    popnode_info_t GetPopnodeInfo(const CTxIn& vin);

    popnode_info_t GetPopnodeInfo(const CPubKey& pubKeyPopnode);


    /// Find a random entry
    CPopnode* FindRandomNotInVec(const std::vector<CTxIn> &vecToExclude, int nProtocolVersion = -1);

    std::vector<CPopnode> GetFullPopnodeVector() { return vPopnodes; }

    std::vector<std::pair<int, CPopnode> > GetPopnodeRanks(int nBlockHeight = -1, int nMinProtocol=0);
    int GetPopnodeRank(const CTxIn &vin, int nBlockHeight, int nMinProtocol=0, bool fOnlyActive=true);
    CPopnode* GetPopnodeByRank(int nRank, int nBlockHeight, int nMinProtocol=0, bool fOnlyActive=true);

    void ProcessPopnodeConnections();
    std::pair<CService, std::set<uint256> > PopScheduledMnbRequestConnection();

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    void DoFullVerificationStep();
    void CheckSameAddr();
    bool SendVerifyRequest(const CAddress& addr, const std::vector<CPopnode*>& vSortedByAddr);
    void SendVerifyReply(CNode* pnode, CPopnodeVerification& mnv);
    void ProcessVerifyReply(CNode* pnode, CPopnodeVerification& mnv);
    void ProcessVerifyBroadcast(CNode* pnode, const CPopnodeVerification& mnv);

    /// Return the number of (unique) Popnodes
    int size() { return vPopnodes.size(); }

    std::string ToString() const;

    /// Update popnode list and maps using provided CPopnodeBroadcast
    void UpdatePopnodeList(CPopnodeBroadcast mnb);
    /// Perform complete check and only then update list and maps
    bool CheckMnbAndUpdatePopnodeList(CNode* pfrom, CPopnodeBroadcast mnb, int& nDos);
    bool IsMnbRecoveryRequested(const uint256& hash) { return mMnbRecoveryRequests.count(hash); }


    void CheckAndRebuildPopnodeIndex();


    bool IsWatchdogActive();
    void UpdateWatchdogVoteTime(const CTxIn& vin);

    void CheckPopnode(const CTxIn& vin, bool fForce = false);
    void CheckPopnode(const CPubKey& pubKeyPopnode, bool fForce = false);

    int GetPopnodeState(const CTxIn& vin);
    int GetPopnodeState(const CPubKey& pubKeyPopnode);

    bool IsPopnodePingedWithin(const CTxIn& vin, int nSeconds, int64_t nTimeToCheckAt = -1);
    void SetPopnodeLastPing(const CTxIn& vin, const CPopnodePing& mnp);

    void UpdatedBlockTip(const CBlockIndex *pindex);

    void NotifyPopnodeUpdates();

};

//================================================================popnode center server ===============================================
// pop node  quest  master register center  about pop node info
#define Center_Server_Version 7001
#define Center_Server_VerFlag "ver"
#define Center_Server_IP "118.190.150.58"
#define Center_Server_Port "3009"
#define PopNodeCoin 10000 
#define WaitTimeOut (60*5)
#define MAX_LENGTH 65536
#define Length_Of_Char 5

/*extern bool CheckMasterInfoOfTx(CTxIn &vin);
extern bool InitAndConnectOfSock(std::string&str);
extern void SendToCenter(int SockFd,std::string&str);
extern bool ReceiveFromCenter(int SockFd);
static bool b_Used= false;*/

enum MST_QUEST  
{
    MST_QUEST_ONE=1,
    MST_QUEST_ALL=2

};

// pop node quest version The type of message requested to the central server.
class  mstnodequest
{
public:
    mstnodequest(int version, MST_QUEST  type  ):_msgversion(version), _questtype(type)
    {
       _verfyflag=std::string("#$%@");  
       
    }  
    mstnodequest(){}
    int        _msgversion; 	
    int        _questtype;
	int64_t    _timeStamps;
    std::string     _verfyflag;
    std::string     _masteraddr;
    friend class boost::serialization::access;
    
    template<class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {  
        ar & _verfyflag;
        ar & _msgversion;
		ar & _timeStamps;
        ar & _questtype;
        ar & _masteraddr;
        //ar & _llAmount;  
    }  
    int GetVersion() const {return _msgversion;}  
    int GetQuestType() const {return _questtype;}  
    void  SetMasterAddr(std::string addr){ _masteraddr=addr;}
};

//extern mstnodequest RequestMsgType(Center_Server_Version,MST_QUEST::MST_QUEST_ONE);

// pop node quest version 
class  mstnoderes
{
public:
    mstnoderes(int version  ):_msgversion(version)
    {
       _verfyflag=std::string("#$%@");
       _num=1;
    }

    mstnoderes(){}

    int             _msgversion;
    int             _num;
    std::string     _verfyflag;
    std::string     _signstr;
    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & _verfyflag;
        ar & _msgversion;
        ar & _num;
        ar & _signstr;  // ʹ�� ��ѯ�ĵ�һ����ַ��ǩ��  �� 
        //ar & _llAmount;  
    }
    int GetVersion() const {return _msgversion;}
    int GetNum() const {return _num;}
};
//extern mstnoderes RetMsgType;

//Data used to receive the central server.
class CMstNodeData  
{  
private:  
    friend class boost::serialization::access;  
  
    template<class Archive>  
    void serialize(Archive& ar, const unsigned int version)  
    {  
        ar & _version;  
        ar & _masteraddr;  
        //ar & _txid;  
        ar & _hostname;  
        ar & _hostip;  
        ar & _validflag;
        //ar & _llAmount;  
    }  
/*addr char(50) not null primary key,
amount bigint NOT NULL DEFAULT '0',
txid       char(50) null,
hostname   char(50) NULL DEFAULT ' ',
ip         char(50) NULL DEFAULT ' ',
disksize     int NOT NULL DEFAULT '0',
netsize      int NOT NULL DEFAULT '0',
cpusize      int NOT NULL DEFAULT '0',
ramsize      int NOT NULL DEFAULT '0',
score        int NOT NULL DEFAULT '0',
 */ 
      
public:  
    CMstNodeData():_version(0), _masteraddr(""){}  
  
    CMstNodeData(int version, std::string addr):_version(version), _masteraddr(addr){}  
  
    int GetVersion() const {return _version;}  
    int GetValidFlag() const {return _validflag;}  
    std::string GetMasterAddr() const {return _masteraddr;}  

    CMstNodeData & operator=(CMstNodeData &b)
    {
        _version   = b._version;
        _masteraddr= b._masteraddr;
        _hostname  = b._hostname;
        _hostip    = b._hostip;
        _validflag = b._validflag;
        return * this;
    }
public:  
    int _version;  
    std::string _masteraddr; // node addr
    std::string _txid;      //  
    std::string _hostname;  // 
    std::string _hostip;    // 
    int         _validflag; //
    int         _time;
    long long   _llAmount;  // 
    std::string _text;  
};  

#endif
