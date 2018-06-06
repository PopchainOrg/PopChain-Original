// Copyright (c) 2017-2018 The Popchain Core Developers

#include "activepopnode.h"
#include "addrman.h"
#include "darksend.h"
#include "popnode-payments.h"
#include "popnode-sync.h"
#include "popnodeman.h"
#include "netfulfilledman.h"
#include "util.h"

#include <iostream>
#include <sstream>
/** Popnode manager */
CPopnodeMan mnodeman;
CService ucenterservice;

const std::string CPopnodeMan::SERIALIZATION_VERSION_STRING = "CPopnodeMan-Version-4";
const int mstnd_iReqBufLen = 500;
const int mstnd_iReqMsgHeadLen = 4;
const int mstnd_iReqMsgTimeout = 10;
const std::string mstnd_SigPubkey = "03e867486ebaeeadda25f1e47612cdaad3384af49fa1242c5821b424937f8ec1f5";


struct CompareLastPaidBlock
{
    bool operator()(const std::pair<int, CPopnode*>& t1,
                    const std::pair<int, CPopnode*>& t2) const
    {
        return (t1.first != t2.first) ? (t1.first < t2.first) : (t1.second->vin < t2.second->vin);
    }
};

struct CompareScoreMN
{
    bool operator()(const std::pair<int64_t, CPopnode*>& t1,
                    const std::pair<int64_t, CPopnode*>& t2) const
    {
        return (t1.first != t2.first) ? (t1.first < t2.first) : (t1.second->vin < t2.second->vin);
    }
};

CPopnodeIndex::CPopnodeIndex()
    : nSize(0),
      mapIndex(),
      mapReverseIndex()
{}

bool CPopnodeIndex::Get(int nIndex, CTxIn& vinPopnode) const
{
    rindex_m_cit it = mapReverseIndex.find(nIndex);
    if(it == mapReverseIndex.end()) {
        return false;
    }
    vinPopnode = it->second;
    return true;
}

int CPopnodeIndex::GetPopnodeIndex(const CTxIn& vinPopnode) const
{
    index_m_cit it = mapIndex.find(vinPopnode);
    if(it == mapIndex.end()) {
        return -1;
    }
    return it->second;
}

void CPopnodeIndex::AddPopnodeVIN(const CTxIn& vinPopnode)
{
    index_m_it it = mapIndex.find(vinPopnode);
    if(it != mapIndex.end()) {
        return;
    }
    int nNextIndex = nSize;
    mapIndex[vinPopnode] = nNextIndex;
    mapReverseIndex[nNextIndex] = vinPopnode;
    ++nSize;
}

void CPopnodeIndex::Clear()
{
    mapIndex.clear();
    mapReverseIndex.clear();
    nSize = 0;
}
struct CompareByAddr

{
    bool operator()(const CPopnode* t1,
                    const CPopnode* t2) const
    {
        return t1->addr < t2->addr;
    }
};

void CPopnodeIndex::RebuildIndex()
{
    nSize = mapIndex.size();
    for(index_m_it it = mapIndex.begin(); it != mapIndex.end(); ++it) {
        mapReverseIndex[it->second] = it->first;
    }
}

void showbuf(const char * buf, int len)
{
	int i = 0, count = 0;
	
	for (i = 0; i < len; ++i)
	{
		printf("%02x ", (uint8_t)buf[i]);
		count++;
		if(count % 8 == 0)
			printf("    ");
		if(count % 16 == 0)
			printf("\n");
	}
	printf("\n");
}

/*void GetRequestMsg(std::string & str)
{
	mstnodequest   mstquest(111,MST_QUEST_ONE);
    mstquest.SetMasterAddr(std::string("NdsRM9waShDUT3TqhgdsGCzqH33Wwb8zDB") );
    std::ostringstream os;
    boost::archive::binary_oarchive oa(os);
    oa<<mstquest;
	str = os.str();
}*/

bool SendRequestNsg(SOCKET sock, CPopnode &mn, mstnodequest &mstquest)
{
	std::string strReq;
	char cbuf[mstnd_iReqBufLen];
	memset(cbuf,0,sizeof(cbuf));
	int buflength = 0;
	
	CBitcoinAddress address(mn.pubKeyCollateralAddress.GetID());
	
	mstquest.SetMasterAddr(address.ToString()/*std::string("uRr71rfTD1nvpmxaSxou5ATvqGriXCysrL")*/);
	mstquest._timeStamps = GetTime();
	
	//std::cout << "check popnode addr " << mstquest._masteraddr << std::endl;
	LogPrintf("CheckActiveMaster: start check popnode %s\n", mstquest._masteraddr);
	
    std::ostringstream os;
    boost::archive::binary_oarchive oa(os);
    oa<<mstquest;
	strReq = os.str();
	
	buflength = strReq.length();
	if(buflength + mstnd_iReqMsgHeadLen > mstnd_iReqBufLen)
		return error("SendRequestNsg : buff size error, string length is %d, need to increase buff size", buflength + mstnd_iReqMsgHeadLen);
	unsigned int n = HNSwapl(buflength);
	memcpy(cbuf, &n, mstnd_iReqMsgHeadLen);
	memcpy(cbuf + mstnd_iReqMsgHeadLen, strReq.c_str(), buflength);
	buflength += mstnd_iReqMsgHeadLen;

	//showbuf(cbuf, buflength);
		
	int nBytes = send(sock, cbuf, buflength, 0);
	if(nBytes != buflength)
		return false;
	return true;
}

extern const std::string strMessageMagic;
bool VerifymsnRes(const mstnoderes & res, const mstnodequest & qst)
{
	CPubKey pubkeyFromSig;
	std::vector<unsigned char> vchSigRcv;
	vchSigRcv = ParseHex(res._signstr);
		
	CPubKey pubkeyLocal(ParseHex(mstnd_SigPubkey));
		
	CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << qst._masteraddr;
	ss << qst._timeStamps;
	uint256 reqhash = ss.GetHash();
		
	if(!pubkeyFromSig.RecoverCompact(reqhash, vchSigRcv)) {
		LogPrintf("VerifymsnRes:Error recovering public key.");
		return false;
	}
	
	if(pubkeyFromSig.GetID() != pubkeyLocal.GetID()) {
        LogPrintf("Keys don't match: pubkey=%s, pubkeyFromSig=%s, hash=%s, vchSig=%s",
                    pubkeyLocal.GetID().ToString().c_str(), pubkeyFromSig.GetID().ToString().c_str(), ss.GetHash().ToString().c_str(),
                    EncodeBase64(&vchSigRcv[0], vchSigRcv.size()));
		/*std::cout << "Keys don't match: pubkey = " << pubkeyLocal.GetID().ToString() << " ,pubkeyFromSig = " << pubkeyFromSig.GetID().ToString()
			<< std::endl << "wordHash = " << reqhash.ToString()
			<< std::endl << "vchSig = " << EncodeBase64(&vchSigRcv[0], vchSigRcv.size()) << std::endl;*/
        return false;
    }
	return true;
}

CPopnodeMan::CPopnodeMan()
: cs(),
  vPopnodes(),
  mAskedUsForPopnodeList(),
  mWeAskedForPopnodeList(),
  mWeAskedForPopnodeListEntry(),
  listScheduledMnbRequestConnections(),
  nLastIndexRebuildTime(0),
  indexPopnodes(),
  indexPopnodesOld(),
  fIndexRebuilt(false),
  fPopnodesAdded(false),
  fPopnodesRemoved(false),
  nLastWatchdogVoteTime(0),
  mapSeenPopnodeBroadcast(),
  mapSeenPopnodePing(),
  nDsqCount(0)
{}

bool CPopnodeMan::CheckActiveMaster(CPopnode &mn)
{
	//return false;
    // Activation validation of the primary node.
    // It is still in the testing phase, and the code will be developed after the test.

	if (!sporkManager.IsSporkActive(SPORK_18_REQUIRE_MASTER_VERIFY_FLAG))
	{
		return true;
	}

    bool proxyConnectionFailed = false;
    SOCKET hSocket;
    if(ConnectSocket(ucenterservice, hSocket, DEFAULT_CONNECT_TIMEOUT, &proxyConnectionFailed))
    {
        if (!IsSelectableSocket(hSocket)) {
            LogPrintf("CPopnodeMan::CheckActiveMaster: Cannot create connection: non-selectable socket created (fd >= FD_SETSIZE ?)\n");
            CloseSocket(hSocket);
            return /*false*/true;
        }

		mstnodequest mstquest(111,MST_QUEST_ONE);		
		if(!SendRequestNsg(hSocket, mn, mstquest))
		{
			CloseSocket(hSocket);
			return error("CPopnodeMan::CheckActiveMaster: send RequestMsgType error");
		}

		char cbuf[mstnd_iReqBufLen];
		memset(cbuf,0,sizeof(cbuf));
		int nBytes = 0;

		int64_t nTimeLast = GetTime();
		while(nBytes <= 0)
		{
			nBytes = recv(hSocket, cbuf, sizeof(cbuf), 0);
			if((GetTime() - nTimeLast) >= mstnd_iReqMsgTimeout)
			{
				CloseSocket(hSocket);
				LogPrintf("CPopnodeMan::CheckActiveMaster: Passed because wait for ack message timeout\n");
				return /*error("CPopnodeMan::CheckActiveMaster: recv CMstNodeData timeout")*/true;
			}
		}
		if(nBytes > mstnd_iReqBufLen)
		{
			CloseSocket(hSocket);
			return error("CPopnodeMan::CheckActiveMaster: msg have too much bytes %d, need increase rcv buf size", nBytes);
		}
		
		int msglen = 0;
		memcpy(&msglen, cbuf, mstnd_iReqMsgHeadLen);
		msglen = HNSwapl(msglen);

		if(msglen != nBytes - mstnd_iReqMsgHeadLen)
		{
			CloseSocket(hSocket);
			return error("CPopnodeMan::CheckActiveMaster: receive a error msg length is %d, recv bytes is %d", msglen, nBytes);
		}
		
		std::string str(cbuf + mstnd_iReqMsgHeadLen, msglen);

		mstnoderes  mstres;
		std::istringstream strstream(str);
		boost::archive::binary_iarchive ia(strstream);
		ia >> mstres;

		if(mstres._num > 0)
		{
			if(!VerifymsnRes(mstres, mstquest))
			{
				CloseSocket(hSocket);
				return error("CPopnodeMan::CheckActiveMaster: receive a error msg can't verify");;
			}
			std::vector<CMstNodeData> vecnode;
		    CMstNodeData  mstnode;
			for (int i = 0; i < mstres._num; ++i)
			{
				ia >> mstnode;
				//std::cout << "mstnode "<<mstnode._masteraddr<< " validflag " << mstnode._validflag << " hostname  "<<mstnode._hostname << "  "<< mstnode._hostip << std::endl;
				if(mstnode._validflag <= 0)
				{
					CloseSocket(hSocket);
					return error("receive a invalid validflag by mstnode %s, validflag %d", mstnode._masteraddr.c_str(), mstnode._validflag);
				}
				vecnode.push_back(mstnode);
			}
			//std::cout << "PopNode check success *********************" << std::endl;
			LogPrintf("CPopnodeMan::CheckActiveMaster: PopNode %s check success\n", mstquest._masteraddr);
			CloseSocket(hSocket);
			return true;
		}
        else 
        {
            return false;
        }    
    }
	CloseSocket(hSocket);
	LogPrintf("CPopnodeMan::CheckActiveMaster: Passed because could't connect to center server\n");
	return /*false*/true;
}

bool CPopnodeMan::Add(CPopnode &mn)
{
    LOCK(cs);
    CPopnode *pmn = Find(mn.vin);
    //bool bActive = true;
    if (pmn == NULL) {
        LogPrint("popnode", "CPopnodeMan::Add -- Adding new Popnode: addr=%s, %i now\n", mn.addr.ToString(), size() + 1);
        /*if (sporkManager.IsSporkActive(SPORK_18_REQUIRE_MASTER_VERIFY_FLAG))
        { 
             bActive = CheckActiveMaster(mn);
        } 
        if ( bActive )
        {
            vPopnodes.push_back(mn);
            indexPopnodes.AddPopnodeVIN(mn.vin);
            fPopnodesAdded = true;
            return true;
        }*/
        vPopnodes.push_back(mn);
        indexPopnodes.AddPopnodeVIN(mn.vin);
        fPopnodesAdded = true;
        return true;
    }
    return false;
}

void CPopnodeMan::AskForMN(CNode* pnode, const CTxIn &vin)
{
    if(!pnode) return;

    LOCK(cs);

    std::map<COutPoint, std::map<CNetAddr, int64_t> >::iterator it1 = mWeAskedForPopnodeListEntry.find(vin.prevout);
    if (it1 != mWeAskedForPopnodeListEntry.end()) {
        std::map<CNetAddr, int64_t>::iterator it2 = it1->second.find(pnode->addr);
        if (it2 != it1->second.end()) {
            if (GetTime() < it2->second) {
                // we've asked recently, should not repeat too often or we could get banned
                return;
            }
            // we asked this node for this outpoint but it's ok to ask again already
            LogPrintf("CPopnodeMan::AskForMN -- Asking same peer %s for missing popnode entry again: %s\n", pnode->addr.ToString(), vin.prevout.ToStringShort());
        } else {
            // we already asked for this outpoint but not this node
            LogPrintf("CPopnodeMan::AskForMN -- Asking new peer %s for missing popnode entry: %s\n", pnode->addr.ToString(), vin.prevout.ToStringShort());
        }
    } else {
        // we never asked any node for this outpoint
        LogPrintf("CPopnodeMan::AskForMN -- Asking peer %s for missing popnode entry for the first time: %s\n", pnode->addr.ToString(), vin.prevout.ToStringShort());
    }
    mWeAskedForPopnodeListEntry[vin.prevout][pnode->addr] = GetTime() + DSEG_UPDATE_SECONDS;

    pnode->PushMessage(NetMsgType::DSEG, vin);
}

void CPopnodeMan::SetRegisteredCheckInterval(int time)
{
	BOOST_FOREACH(CPopnode& mn, vPopnodes) {
        mn.SetRegisteredCheckInterval(time);
    }
}


void CPopnodeMan::Check()
{
    LOCK(cs);

    LogPrint("popnode", "CPopnodeMan::Check -- nLastWatchdogVoteTime=%d, IsWatchdogActive()=%d\n", nLastWatchdogVoteTime, IsWatchdogActive());

    BOOST_FOREACH(CPopnode& mn, vPopnodes) {
        mn.Check();
    }
}

// Popchain DevTeam
void CPopnodeMan::CheckAndRemove()
{
    if(!popnodeSync.IsPopnodeListSynced()) return;

    LogPrintf("CPopnodeMan::CheckAndRemove\n");

    {
        // Need LOCK2 here to ensure consistent locking order because code below locks cs_main
        // in CheckMnbAndUpdatePopnodeList()
        LOCK2(cs_main, cs);

        Check();

        // Remove spent popnodes, prepare structures and make requests to reasure the state of inactive ones
        std::vector<CPopnode>::iterator it = vPopnodes.begin();
        std::vector<std::pair<int, CPopnode> > vecPopnodeRanks;
        // ask for up to MNB_RECOVERY_MAX_ASK_ENTRIES popnode entries at a time
        int nAskForMnbRecovery = MNB_RECOVERY_MAX_ASK_ENTRIES;
        while(it != vPopnodes.end()) {
            CPopnodeBroadcast mnb = CPopnodeBroadcast(*it);
            uint256 hash = mnb.GetHash();
            // If collateral was spent ...
            if ((*it).IsOutpointSpent() || (*it).IsRegistered()) {
                LogPrint("popnode", "CPopnodeMan::CheckAndRemove -- Removing Popnode: %s  addr=%s  %i now\n", (*it).GetStateString(), (*it).addr.ToString(), size() - 1);

                // erase all of the broadcasts we've seen from this txin, ...
                mapSeenPopnodeBroadcast.erase(hash);
                mWeAskedForPopnodeListEntry.erase((*it).vin.prevout);

                // and finally remove it from the list
                it = vPopnodes.erase(it);
                fPopnodesRemoved = true;
            } else {
                bool fAsk = pCurrentBlockIndex &&
                            (nAskForMnbRecovery > 0) &&
                            popnodeSync.IsSynced() &&
                            it->IsNewStartRequired() &&
                            !IsMnbRecoveryRequested(hash);
                if(fAsk) {
                    // this mn is in a non-recoverable state and we haven't asked other nodes yet
                    std::set<CNetAddr> setRequested;
                    // calulate only once and only when it's needed
                    if(vecPopnodeRanks.empty()) {
                        int nRandomBlockHeight = GetRandInt(pCurrentBlockIndex->nHeight);
                        vecPopnodeRanks = GetPopnodeRanks(nRandomBlockHeight);
                    }
                    bool fAskedForMnbRecovery = false;
                    // ask first MNB_RECOVERY_QUORUM_TOTAL popnodes we can connect to and we haven't asked recently
                    for(int i = 0; setRequested.size() < MNB_RECOVERY_QUORUM_TOTAL && i < (int)vecPopnodeRanks.size(); i++) {
                        // avoid banning
                        if(mWeAskedForPopnodeListEntry.count(it->vin.prevout) && mWeAskedForPopnodeListEntry[it->vin.prevout].count(vecPopnodeRanks[i].second.addr)) continue;
                        // didn't ask recently, ok to ask now
                        CService addr = vecPopnodeRanks[i].second.addr;
                        setRequested.insert(addr);
                        listScheduledMnbRequestConnections.push_back(std::make_pair(addr, hash));
                        fAskedForMnbRecovery = true;
                    }
                    if(fAskedForMnbRecovery) {
                        LogPrint("popnode", "CPopnodeMan::CheckAndRemove -- Recovery initiated, popnode=%s\n", it->vin.prevout.ToStringShort());
                        nAskForMnbRecovery--;
                    }
                    // wait for mnb recovery replies for MNB_RECOVERY_WAIT_SECONDS seconds
                    mMnbRecoveryRequests[hash] = std::make_pair(GetTime() + MNB_RECOVERY_WAIT_SECONDS, setRequested);
                }
                ++it;
            }
        }

        // proces replies for POPNODE_NEW_START_REQUIRED popnodes
        LogPrint("popnode", "CPopnodeMan::CheckAndRemove -- mMnbRecoveryGoodReplies size=%d\n", (int)mMnbRecoveryGoodReplies.size());
        std::map<uint256, std::vector<CPopnodeBroadcast> >::iterator itMnbReplies = mMnbRecoveryGoodReplies.begin();
        while(itMnbReplies != mMnbRecoveryGoodReplies.end()){
            if(mMnbRecoveryRequests[itMnbReplies->first].first < GetTime()) {
                // all nodes we asked should have replied now
                if(itMnbReplies->second.size() >= MNB_RECOVERY_QUORUM_REQUIRED) {
                    // majority of nodes we asked agrees that this mn doesn't require new mnb, reprocess one of new mnbs
                    LogPrint("popnode", "CPopnodeMan::CheckAndRemove -- reprocessing mnb, popnode=%s\n", itMnbReplies->second[0].vin.prevout.ToStringShort());
                    // mapSeenPopnodeBroadcast.erase(itMnbReplies->first);
                    int nDos;
                    itMnbReplies->second[0].fRecovery = true;
                    CheckMnbAndUpdatePopnodeList(NULL, itMnbReplies->second[0], nDos);
                }
                LogPrint("popnode", "CPopnodeMan::CheckAndRemove -- removing mnb recovery reply, popnode=%s, size=%d\n", itMnbReplies->second[0].vin.prevout.ToStringShort(), (int)itMnbReplies->second.size());
                mMnbRecoveryGoodReplies.erase(itMnbReplies++);
            } else {
                ++itMnbReplies;
            }
        }
    }
    {
        // no need for cm_main below
        LOCK(cs);

        std::map<uint256, std::pair< int64_t, std::set<CNetAddr> > >::iterator itMnbRequest = mMnbRecoveryRequests.begin();
        while(itMnbRequest != mMnbRecoveryRequests.end()){
            // Allow this mnb to be re-verified again after MNB_RECOVERY_RETRY_SECONDS seconds
            // if mn is still in POPNODE_NEW_START_REQUIRED state.
            if(GetTime() - itMnbRequest->second.first > MNB_RECOVERY_RETRY_SECONDS) {
                mMnbRecoveryRequests.erase(itMnbRequest++);
            } else {
                ++itMnbRequest;
            }
        }

        // check who's asked for the Popnode list
        std::map<CNetAddr, int64_t>::iterator it1 = mAskedUsForPopnodeList.begin();
        while(it1 != mAskedUsForPopnodeList.end()){
            if((*it1).second < GetTime()) {
                mAskedUsForPopnodeList.erase(it1++);
            } else {
                ++it1;
            }
        }

        // check who we asked for the Popnode list
        it1 = mWeAskedForPopnodeList.begin();
        while(it1 != mWeAskedForPopnodeList.end()){
            if((*it1).second < GetTime()){
                mWeAskedForPopnodeList.erase(it1++);
            } else {
                ++it1;
            }
        }

        // check which Popnodes we've asked for
        std::map<COutPoint, std::map<CNetAddr, int64_t> >::iterator it2 = mWeAskedForPopnodeListEntry.begin();
        while(it2 != mWeAskedForPopnodeListEntry.end()){
            std::map<CNetAddr, int64_t>::iterator it3 = it2->second.begin();
            while(it3 != it2->second.end()){
                if(it3->second < GetTime()){
                    it2->second.erase(it3++);
                } else {
                    ++it3;
                }
            }
            if(it2->second.empty()) {
                mWeAskedForPopnodeListEntry.erase(it2++);
            } else {
                ++it2;
            }
        }

        std::map<CNetAddr, CPopnodeVerification>::iterator it3 = mWeAskedForVerification.begin();
        while(it3 != mWeAskedForVerification.end()){
            if(it3->second.nBlockHeight < pCurrentBlockIndex->nHeight - MAX_POSE_BLOCKS) {
                mWeAskedForVerification.erase(it3++);
            } else {
                ++it3;
            }
        }

        // NOTE: do not expire mapSeenPopnodeBroadcast entries here, clean them on mnb updates!

        // remove expired mapSeenPopnodePing
        std::map<uint256, CPopnodePing>::iterator it4 = mapSeenPopnodePing.begin();
        while(it4 != mapSeenPopnodePing.end()){
            if((*it4).second.IsExpired()) {
                LogPrint("popnode", "CPopnodeMan::CheckAndRemove -- Removing expired Popnode ping: hash=%s\n", (*it4).second.GetHash().ToString());
                mapSeenPopnodePing.erase(it4++);
            } else {
                ++it4;
            }
        }

        // remove expired mapSeenPopnodeVerification
        std::map<uint256, CPopnodeVerification>::iterator itv2 = mapSeenPopnodeVerification.begin();
        while(itv2 != mapSeenPopnodeVerification.end()){
            if((*itv2).second.nBlockHeight < pCurrentBlockIndex->nHeight - MAX_POSE_BLOCKS){
                LogPrint("popnode", "CPopnodeMan::CheckAndRemove -- Removing expired Popnode verification: hash=%s\n", (*itv2).first.ToString());
                mapSeenPopnodeVerification.erase(itv2++);
            } else {
                ++itv2;
            }
        }

        LogPrintf("CPopnodeMan::CheckAndRemove -- %s\n", ToString());

        if(fPopnodesRemoved) {
            CheckAndRebuildPopnodeIndex();
        }
    }

    if(fPopnodesRemoved) {
        NotifyPopnodeUpdates();
    }
}

void CPopnodeMan::Clear()
{
    LOCK(cs);
    vPopnodes.clear();
    mAskedUsForPopnodeList.clear();
    mWeAskedForPopnodeList.clear();
    mWeAskedForPopnodeListEntry.clear();
    mapSeenPopnodeBroadcast.clear();
    mapSeenPopnodePing.clear();
    nDsqCount = 0;
    nLastWatchdogVoteTime = 0;
    indexPopnodes.Clear();
    indexPopnodesOld.Clear();
}


// Popchain DevTeam
int CPopnodeMan::CountEnabled(int nProtocolVersion)
{
    LOCK(cs);
    int nCount = 0;

    BOOST_FOREACH(CPopnode& mn, vPopnodes) {
        if(!mn.IsEnabled()) continue;
        nCount++;
    }

    return nCount;
}

/* Only IPv4 popnodes are allowed in 12.1, saving this for later
int CPopnodeMan::CountByIP(int nNetworkType)
{
    LOCK(cs);
    int nNodeCount = 0;

    BOOST_FOREACH(CPopnode& mn, vPopnodes)
        if ((nNetworkType == NET_IPV4 && mn.addr.IsIPv4()) ||
            (nNetworkType == NET_TOR  && mn.addr.IsTor())  ||
            (nNetworkType == NET_IPV6 && mn.addr.IsIPv6())) {
                nNodeCount++;
        }

    return nNodeCount;
}
*/

void CPopnodeMan::DsegUpdate(CNode* pnode)
{
    LOCK(cs);

    if(Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if(!(pnode->addr.IsRFC1918() || pnode->addr.IsLocal())) {
            std::map<CNetAddr, int64_t>::iterator it = mWeAskedForPopnodeList.find(pnode->addr);
            if(it != mWeAskedForPopnodeList.end() && GetTime() < (*it).second) {
                LogPrintf("CPopnodeMan::DsegUpdate -- we already asked %s for the list; skipping...\n", pnode->addr.ToString());
                return;
            }
        }
    }
    
    pnode->PushMessage(NetMsgType::DSEG, CTxIn());
    int64_t askAgain = GetTime() + DSEG_UPDATE_SECONDS;
    mWeAskedForPopnodeList[pnode->addr] = askAgain;

    LogPrint("popnode", "CPopnodeMan::DsegUpdate -- asked %s for the list\n", pnode->addr.ToString());
}

CPopnode* CPopnodeMan::Find(const CScript &payee)
{
    LOCK(cs);

    BOOST_FOREACH(CPopnode& mn, vPopnodes)
    {
        if(GetScriptForDestination(mn.pubKeyCollateralAddress.GetID()) == payee)
            return &mn;
    }
    return NULL;
}

CPopnode* CPopnodeMan::Find(const CTxIn &vin)
{
    LOCK(cs);

    BOOST_FOREACH(CPopnode& mn, vPopnodes)
    {
        if(mn.vin.prevout == vin.prevout)
            return &mn;
    }
    return NULL;
}

CPopnode* CPopnodeMan::Find(const CPubKey &pubKeyPopnode)
{
    LOCK(cs);

    BOOST_FOREACH(CPopnode& mn, vPopnodes)
    {
        if(mn.pubKeyPopnode == pubKeyPopnode)
            return &mn;
    }
    return NULL;
}

bool CPopnodeMan::Get(const CPubKey& pubKeyPopnode, CPopnode& popnode)
{
    // Theses mutexes are recursive so double locking by the same thread is safe.
    LOCK(cs);
    CPopnode* pMN = Find(pubKeyPopnode);
    if(!pMN)  {
        return false;
    }
    popnode = *pMN;
    return true;
}

bool CPopnodeMan::Get(const CTxIn& vin, CPopnode& popnode)
{
    // Theses mutexes are recursive so double locking by the same thread is safe.
    LOCK(cs);
    CPopnode* pMN = Find(vin);
    if(!pMN)  {
        return false;
    }
    popnode = *pMN;
    return true;
}

popnode_info_t CPopnodeMan::GetPopnodeInfo(const CTxIn& vin)
{
    popnode_info_t info;
    LOCK(cs);
    CPopnode* pMN = Find(vin);
    if(!pMN)  {
        return info;
    }
    info = pMN->GetInfo();
    return info;
}

popnode_info_t CPopnodeMan::GetPopnodeInfo(const CPubKey& pubKeyPopnode)
{
    popnode_info_t info;
    LOCK(cs);
    CPopnode* pMN = Find(pubKeyPopnode);
    if(!pMN)  {
        return info;
    }
    info = pMN->GetInfo();
    return info;
}

bool CPopnodeMan::Has(const CTxIn& vin)
{
    LOCK(cs);
    CPopnode* pMN = Find(vin);
    return (pMN != NULL);
}


// Popchain DevTeam
CPopnode* CPopnodeMan::FindRandomNotInVec(const std::vector<CTxIn> &vecToExclude, int nProtocolVersion)
{
    LOCK(cs);

    int nCountEnabled = CountEnabled(nProtocolVersion);
    int nCountNotExcluded = nCountEnabled - vecToExclude.size();

    LogPrintf("CPopnodeMan::FindRandomNotInVec -- %d enabled popnodes, %d popnodes to choose from\n", nCountEnabled, nCountNotExcluded);
    if(nCountNotExcluded < 1) return NULL;

    // fill a vector of pointers
    std::vector<CPopnode*> vpPopnodesShuffled;
    BOOST_FOREACH(CPopnode &mn, vPopnodes) {
        vpPopnodesShuffled.push_back(&mn);
    }

    InsecureRand insecureRand;
    // shuffle pointers
    std::random_shuffle(vpPopnodesShuffled.begin(), vpPopnodesShuffled.end(), insecureRand);
    bool fExclude;

    // loop through
    BOOST_FOREACH(CPopnode* pmn, vpPopnodesShuffled) {
        if(pmn->nProtocolVersion < nProtocolVersion || !pmn->IsEnabled()) continue;
        fExclude = false;
        BOOST_FOREACH(const CTxIn &txinToExclude, vecToExclude) {
            if(pmn->vin.prevout == txinToExclude.prevout) {
                fExclude = true;
                break;
            }
        }
        if(fExclude) continue;
        // found the one not in vecToExclude
        LogPrint("popnode", "CPopnodeMan::FindRandomNotInVec -- found, popnode=%s\n", pmn->vin.prevout.ToStringShort());
        return pmn;
    }

    LogPrint("popnode", "CPopnodeMan::FindRandomNotInVec -- failed\n");
    return NULL;
}

int CPopnodeMan::GetPopnodeRank(const CTxIn& vin, int nBlockHeight, int nMinProtocol, bool fOnlyActive)
{
    std::vector<std::pair<int64_t, CPopnode*> > vecPopnodeScores;

    //make sure we know about this block
    uint256 blockHash = uint256();
    if(!GetBlockHash(blockHash, nBlockHeight)) return -1;

    LOCK(cs);

    // scan for winner
    BOOST_FOREACH(CPopnode& mn, vPopnodes) {
        if(mn.nProtocolVersion < nMinProtocol) continue;
        if(fOnlyActive) {
            if(!mn.IsEnabled()) continue;
        }
        else {
            if(!mn.IsValidForPayment()) continue;
        }
        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecPopnodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecPopnodeScores.rbegin(), vecPopnodeScores.rend(), CompareScoreMN());

    int nRank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CPopnode*)& scorePair, vecPopnodeScores) {
        nRank++;
        if(scorePair.second->vin.prevout == vin.prevout) return nRank;
    }

    return -1;
}

std::vector<std::pair<int, CPopnode> > CPopnodeMan::GetPopnodeRanks(int nBlockHeight, int nMinProtocol)
{
    std::vector<std::pair<int64_t, CPopnode*> > vecPopnodeScores;
    std::vector<std::pair<int, CPopnode> > vecPopnodeRanks;

    //make sure we know about this block
    uint256 blockHash = uint256();
    if(!GetBlockHash(blockHash, nBlockHeight)) return vecPopnodeRanks;

    LOCK(cs);

    // scan for winner
    BOOST_FOREACH(CPopnode& mn, vPopnodes) {

        if(mn.nProtocolVersion < nMinProtocol || !mn.IsEnabled()) continue;

        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecPopnodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecPopnodeScores.rbegin(), vecPopnodeScores.rend(), CompareScoreMN());

    int nRank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CPopnode*)& s, vecPopnodeScores) {
        nRank++;
        vecPopnodeRanks.push_back(std::make_pair(nRank, *s.second));
    }

    return vecPopnodeRanks;
}

CPopnode* CPopnodeMan::GetPopnodeByRank(int nRank, int nBlockHeight, int nMinProtocol, bool fOnlyActive)
{
    std::vector<std::pair<int64_t, CPopnode*> > vecPopnodeScores;

    LOCK(cs);

    uint256 blockHash;
    if(!GetBlockHash(blockHash, nBlockHeight)) {
        LogPrintf("CPopnode::GetPopnodeByRank -- ERROR: GetBlockHash() failed at nBlockHeight %d\n", nBlockHeight);
        return NULL;
    }

    // Fill scores
    BOOST_FOREACH(CPopnode& mn, vPopnodes) {

        if(mn.nProtocolVersion < nMinProtocol) continue;
        if(fOnlyActive && !mn.IsEnabled()) continue;

        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecPopnodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecPopnodeScores.rbegin(), vecPopnodeScores.rend(), CompareScoreMN());

    int rank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CPopnode*)& s, vecPopnodeScores){
        rank++;
        if(rank == nRank) {
            return s.second;
        }
    }

    return NULL;
}

void CPopnodeMan::ProcessPopnodeConnections()
{
    //we don't care about this for regtest
    if(Params().NetworkIDString() == CBaseChainParams::REGTEST) return;

    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes) {
        if(pnode->fPopnode) {
            if(darkSendPool.pSubmittedToPopnode != NULL && pnode->addr == darkSendPool.pSubmittedToPopnode->addr) continue;
            LogPrintf("Closing Popnode connection: peer=%d, addr=%s\n", pnode->id, pnode->addr.ToString());
            pnode->fDisconnect = true;
        }
    }
}

std::pair<CService, std::set<uint256> > CPopnodeMan::PopScheduledMnbRequestConnection()
{
    LOCK(cs);
    if(listScheduledMnbRequestConnections.empty()) {
        return std::make_pair(CService(), std::set<uint256>());
    }

    std::set<uint256> setResult;

    listScheduledMnbRequestConnections.sort();
    std::pair<CService, uint256> pairFront = listScheduledMnbRequestConnections.front();

    // squash hashes from requests with the same CService as the first one into setResult
    std::list< std::pair<CService, uint256> >::iterator it = listScheduledMnbRequestConnections.begin();
    while(it != listScheduledMnbRequestConnections.end()) {
        if(pairFront.first == it->first) {
            setResult.insert(it->second);
            it = listScheduledMnbRequestConnections.erase(it);
        } else {
            // since list is sorted now, we can be sure that there is no more hashes left
            // to ask for from this addr
            break;
        }
    }
    return std::make_pair(pairFront.first, setResult);
}


void CPopnodeMan::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if(fLiteMode) return; // disable all Pop specific functionality
    if(!popnodeSync.IsBlockchainSynced()) return;

    if (strCommand == NetMsgType::MNANNOUNCE) { //Popnode Broadcast

        CPopnodeBroadcast mnb;
        vRecv >> mnb;

        pfrom->setAskFor.erase(mnb.GetHash());

        LogPrint("popnode", "MNANNOUNCE -- Popnode announce, popnode=%s\n", mnb.vin.prevout.ToStringShort());

        // backward compatibility patch
        if(pfrom->nVersion < 70204) {
            int64_t nLastDsqDummy;
            vRecv >> nLastDsqDummy;
        }

        int nDos = 0;

        if (CheckMnbAndUpdatePopnodeList(pfrom, mnb, nDos)) {
            // use announced Popnode as a peer
            addrman.Add(CAddress(mnb.addr), pfrom->addr, 2*60*60);
        } else if(nDos > 0) {
            Misbehaving(pfrom->GetId(), nDos);
        }

        if(fPopnodesAdded) {
            NotifyPopnodeUpdates();
        }
    } else if (strCommand == NetMsgType::MNPING) { //Popnode Ping

        CPopnodePing mnp;
        vRecv >> mnp;

        uint256 nHash = mnp.GetHash();

        pfrom->setAskFor.erase(nHash);

        LogPrint("popnode", "MNPING -- Popnode ping, popnode=%s\n", mnp.vin.prevout.ToStringShort());

        // Need LOCK2 here to ensure consistent locking order because the CheckAndUpdate call below locks cs_main
        LOCK2(cs_main, cs);

        if(mapSeenPopnodePing.count(nHash)) return; //seen
        mapSeenPopnodePing.insert(std::make_pair(nHash, mnp));

        LogPrint("popnode", "MNPING -- Popnode ping, popnode=%s new\n", mnp.vin.prevout.ToStringShort());

        // see if we have this Popnode
        CPopnode* pmn = mnodeman.Find(mnp.vin);

        // too late, new MNANNOUNCE is required
        if(pmn && pmn->IsNewStartRequired()) return;

        int nDos = 0;
        if(mnp.CheckAndUpdate(pmn, false, nDos)) return;

        if(nDos > 0) {
            // if anything significant failed, mark that node
            Misbehaving(pfrom->GetId(), nDos);
        } else if(pmn != NULL) {
            // nothing significant failed, mn is a known one too
            return;
        }

        // something significant is broken or mn is unknown,
        // we might have to ask for a popnode entry once
        AskForMN(pfrom, mnp.vin);

    } else if (strCommand == NetMsgType::DSEG) { //Get Popnode list or specific entry
        // Ignore such requests until we are fully synced.
        // We could start processing this after popnode list is synced
        // but this is a heavy one so it's better to finish sync first.
        if (!popnodeSync.IsSynced()) return;

        CTxIn vin;
        vRecv >> vin;

        LogPrint("popnode", "DSEG -- Popnode list, popnode=%s\n", vin.prevout.ToStringShort());

        LOCK(cs);

        if(vin == CTxIn()) { //only should ask for this once
            //local network
            bool isLocal = (pfrom->addr.IsRFC1918() || pfrom->addr.IsLocal());

            if(!isLocal && Params().NetworkIDString() == CBaseChainParams::MAIN) {
                std::map<CNetAddr, int64_t>::iterator i = mAskedUsForPopnodeList.find(pfrom->addr);
                if (i != mAskedUsForPopnodeList.end()){
                    int64_t t = (*i).second;
                    if (GetTime() < t) {
                        Misbehaving(pfrom->GetId(), 34);
                        LogPrintf("DSEG -- peer already asked me for the list, peer=%d\n", pfrom->id);
                        return;
                    }
                }
                int64_t askAgain = GetTime() + DSEG_UPDATE_SECONDS;
                mAskedUsForPopnodeList[pfrom->addr] = askAgain;
            }
        } //else, asking for a specific node which is ok

        int nInvCount = 0;

        BOOST_FOREACH(CPopnode& mn, vPopnodes) {
            if (vin != CTxIn() && vin != mn.vin) continue; // asked for specific vin but we are not there yet
            if (mn.addr.IsRFC1918() || mn.addr.IsLocal()) continue; // do not send local network popnode

            LogPrint("popnode", "DSEG -- Sending Popnode entry: popnode=%s  addr=%s\n", mn.vin.prevout.ToStringShort(), mn.addr.ToString());
            CPopnodeBroadcast mnb = CPopnodeBroadcast(mn);
            uint256 hash = mnb.GetHash();
            pfrom->PushInventory(CInv(MSG_POPNODE_ANNOUNCE, hash));
            pfrom->PushInventory(CInv(MSG_POPNODE_PING, mn.lastPing.GetHash()));
            nInvCount++;

            if (!mapSeenPopnodeBroadcast.count(hash)) {
                mapSeenPopnodeBroadcast.insert(std::make_pair(hash, std::make_pair(GetTime(), mnb)));
            }

            if (vin == mn.vin) {
                LogPrintf("DSEG -- Sent 1 Popnode inv to peer %d\n", pfrom->id);
                return;
            }
        }

        if(vin == CTxIn()) {
            pfrom->PushMessage(NetMsgType::SYNCSTATUSCOUNT, POPNODE_SYNC_LIST, nInvCount);
            LogPrintf("DSEG -- Sent %d Popnode invs to peer %d\n", nInvCount, pfrom->id);
            return;
        }
        // smth weird happen - someone asked us for vin we have no idea about?
        LogPrint("popnode", "DSEG -- No invs sent to peer %d\n", pfrom->id);

    } else if (strCommand == NetMsgType::MNVERIFY) { // Popnode Verify

        // Need LOCK2 here to ensure consistent locking order because the all functions below call GetBlockHash which locks cs_main
        LOCK2(cs_main, cs);

        CPopnodeVerification mnv;
        vRecv >> mnv;

        if(mnv.vchSig1.empty()) {
            // CASE 1: someone asked me to verify myself /IP we are using/
            SendVerifyReply(pfrom, mnv);
        } else if (mnv.vchSig2.empty()) {
            // CASE 2: we _probably_ got verification we requested from some popnode
            ProcessVerifyReply(pfrom, mnv);
        } else {
            // CASE 3: we _probably_ got verification broadcast signed by some popnode which verified another one
            ProcessVerifyBroadcast(pfrom, mnv);
        }
    }
}

// Verification of popnodes via unique direct requests.

void CPopnodeMan::DoFullVerificationStep()
{
    if(activePopnode.vin == CTxIn()) return;
    if(!popnodeSync.IsSynced()) return;

    std::vector<std::pair<int, CPopnode> > vecPopnodeRanks = GetPopnodeRanks(pCurrentBlockIndex->nHeight - 1, MIN_POSE_PROTO_VERSION);

    // Need LOCK2 here to ensure consistent locking order because the SendVerifyRequest call below locks cs_main
    // through GetHeight() signal in ConnectNode
    LOCK2(cs_main, cs);

    int nCount = 0;
    int nCountMax = std::max(10, (int)vPopnodes.size() / 100); // verify at least 10 popnode at once but at most 1% of all known popnodes

    int nMyRank = -1;
    int nRanksTotal = (int)vecPopnodeRanks.size();

    // send verify requests only if we are in top MAX_POSE_RANK
    std::vector<std::pair<int, CPopnode> >::iterator it = vecPopnodeRanks.begin();
    while(it != vecPopnodeRanks.end()) {
        if(it->first > MAX_POSE_RANK) {
            LogPrint("popnode", "CPopnodeMan::DoFullVerificationStep -- Must be in top %d to send verify request\n",
                        (int)MAX_POSE_RANK);
            return;
        }
        if(it->second.vin == activePopnode.vin) {
            nMyRank = it->first;
            LogPrint("popnode", "CPopnodeMan::DoFullVerificationStep -- Found self at rank %d/%d, verifying up to %d popnodes\n",
                        nMyRank, nRanksTotal, nCountMax);
            break;
        }
        ++it;
    }

    // edge case: list is too short and this popnode is not enabled
    if(nMyRank == -1) return;

    // send verify requests to up to nCountMax popnodes starting from
    // (MAX_POSE_RANK + nCountMax * (nMyRank - 1) + 1)
    int nOffset = MAX_POSE_RANK + nCountMax * (nMyRank - 1);
    if(nOffset >= (int)vecPopnodeRanks.size()) return;

    std::vector<CPopnode*> vSortedByAddr;
    BOOST_FOREACH(CPopnode& mn, vPopnodes) {
        vSortedByAddr.push_back(&mn);
    }

    sort(vSortedByAddr.begin(), vSortedByAddr.end(), CompareByAddr());

    it = vecPopnodeRanks.begin() + nOffset;
    while(it != vecPopnodeRanks.end()) {
        if(it->second.IsPoSeVerified() || it->second.IsPoSeBanned()) {
            LogPrint("popnode", "CPopnodeMan::DoFullVerificationStep -- Already %s%s%s popnode %s address %s, skipping...\n",
                        it->second.IsPoSeVerified() ? "verified" : "",
                        it->second.IsPoSeVerified() && it->second.IsPoSeBanned() ? " and " : "",
                        it->second.IsPoSeBanned() ? "banned" : "",
                        it->second.vin.prevout.ToStringShort(), it->second.addr.ToString());
            ++it;
            continue;
        }
        LogPrint("popnode", "CPopnodeMan::DoFullVerificationStep -- Verifying popnode %s rank %d/%d address %s\n",
                    it->second.vin.prevout.ToStringShort(), it->first, nRanksTotal, it->second.addr.ToString());
        if(SendVerifyRequest((CAddress)it->second.addr, vSortedByAddr)) {
            nCount++;
            if(nCount >= nCountMax) break;
        }
        ++it;
    }

    LogPrint("popnode", "CPopnodeMan::DoFullVerificationStep -- Sent verification requests to %d popnodes\n", nCount);
}

// This function tries to find popnodes with the same addr,
// find a verified one and ban all the other. If there are many nodes
// with the same addr but none of them is verified yet, then none of them are banned.
// It could take many times to run this before most of the duplicate nodes are banned.

void CPopnodeMan::CheckSameAddr()
{
    if(!popnodeSync.IsSynced() || vPopnodes.empty()) return;

    std::vector<CPopnode*> vBan;
    std::vector<CPopnode*> vSortedByAddr;

    {
        LOCK(cs);

        CPopnode* pprevPopnode = NULL;
        CPopnode* pverifiedPopnode = NULL;

        BOOST_FOREACH(CPopnode& mn, vPopnodes) {
            vSortedByAddr.push_back(&mn);
        }

        sort(vSortedByAddr.begin(), vSortedByAddr.end(), CompareByAddr());

        BOOST_FOREACH(CPopnode* pmn, vSortedByAddr) {
            // check only (pre)enabled popnodes
            if(!pmn->IsEnabled() && !pmn->IsPreEnabled()) continue;
            // initial step
            if(!pprevPopnode) {
                pprevPopnode = pmn;
                pverifiedPopnode = pmn->IsPoSeVerified() ? pmn : NULL;
                continue;
            }
            // second+ step
            if(pmn->addr == pprevPopnode->addr) {
                if(pverifiedPopnode) {
                    // another popnode with the same ip is verified, ban this one
                    vBan.push_back(pmn);
                } else if(pmn->IsPoSeVerified()) {
                    // this popnode with the same ip is verified, ban previous one
                    vBan.push_back(pprevPopnode);
                    // and keep a reference to be able to ban following popnodes with the same ip
                    pverifiedPopnode = pmn;
                }
            } else {
                pverifiedPopnode = pmn->IsPoSeVerified() ? pmn : NULL;
            }
            pprevPopnode = pmn;
        }
    }

    // ban duplicates
    BOOST_FOREACH(CPopnode* pmn, vBan) {
        LogPrintf("CPopnodeMan::CheckSameAddr -- increasing PoSe ban score for popnode %s\n", pmn->vin.prevout.ToStringShort());
        pmn->IncreasePoSeBanScore();
    }
}

bool CPopnodeMan::SendVerifyRequest(const CAddress& addr, const std::vector<CPopnode*>& vSortedByAddr)
{
    if(netfulfilledman.HasFulfilledRequest(addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request")) {
        // we already asked for verification, not a good idea to do this too often, skip it
        LogPrint("popnode", "CPopnodeMan::SendVerifyRequest -- too many requests, skipping... addr=%s\n", addr.ToString());
        return false;
    }

    CNode* pnode = ConnectNode(addr, NULL, true);
    if(pnode == NULL) {
        LogPrintf("CPopnodeMan::SendVerifyRequest -- can't connect to node to verify it, addr=%s\n", addr.ToString());
        return false;
    }

    netfulfilledman.AddFulfilledRequest(addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request");
    // use random nonce, store it and require node to reply with correct one later
    CPopnodeVerification mnv(addr, GetRandInt(999999), pCurrentBlockIndex->nHeight - 1);
    mWeAskedForVerification[addr] = mnv;
    LogPrintf("CPopnodeMan::SendVerifyRequest -- verifying node using nonce %d addr=%s\n", mnv.nonce, addr.ToString());
    pnode->PushMessage(NetMsgType::MNVERIFY, mnv);

    return true;
}

void CPopnodeMan::SendVerifyReply(CNode* pnode, CPopnodeVerification& mnv)
{
    // only popnodes can sign this, why would someone ask regular node?
    if(!fPopNode) {
        // do not ban, malicious node might be using my IP
        // and trying to confuse the node which tries to verify it
        return;
    }

    if(netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-reply")) {
        // peer should not ask us that often
        LogPrintf("PopnodeMan::SendVerifyReply -- ERROR: peer already asked me recently, peer=%d\n", pnode->id);
        Misbehaving(pnode->id, 20);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight)) {
        LogPrintf("PopnodeMan::SendVerifyReply -- can't get block hash for unknown block height %d, peer=%d\n", mnv.nBlockHeight, pnode->id);
        return;
    }

    std::string strMessage = strprintf("%s%d%s", activePopnode.service.ToString(false), mnv.nonce, blockHash.ToString());

    if(!darkSendSigner.SignMessage(strMessage, mnv.vchSig1, activePopnode.keyPopnode)) {
        LogPrintf("PopnodeMan::SendVerifyReply -- SignMessage() failed\n");
        return;
    }

    std::string strError;

    if(!darkSendSigner.VerifyMessage(activePopnode.pubKeyPopnode, mnv.vchSig1, strMessage, strError)) {
        LogPrintf("PopnodeMan::SendVerifyReply -- VerifyMessage() failed, error: %s\n", strError);
        return;
    }

    pnode->PushMessage(NetMsgType::MNVERIFY, mnv);
    netfulfilledman.AddFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-reply");
}

void CPopnodeMan::ProcessVerifyReply(CNode* pnode, CPopnodeVerification& mnv)
{
    std::string strError;

    // did we even ask for it? if that's the case we should have matching fulfilled request
    if(!netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request")) {
        LogPrintf("CPopnodeMan::ProcessVerifyReply -- ERROR: we didn't ask for verification of %s, peer=%d\n", pnode->addr.ToString(), pnode->id);
        Misbehaving(pnode->id, 20);
        return;
    }

    // Received nonce for a known address must match the one we sent
    if(mWeAskedForVerification[pnode->addr].nonce != mnv.nonce) {
        LogPrintf("CPopnodeMan::ProcessVerifyReply -- ERROR: wrong nounce: requested=%d, received=%d, peer=%d\n",
                    mWeAskedForVerification[pnode->addr].nonce, mnv.nonce, pnode->id);
        Misbehaving(pnode->id, 20);
        return;
    }

    // Received nBlockHeight for a known address must match the one we sent
    if(mWeAskedForVerification[pnode->addr].nBlockHeight != mnv.nBlockHeight) {
        LogPrintf("CPopnodeMan::ProcessVerifyReply -- ERROR: wrong nBlockHeight: requested=%d, received=%d, peer=%d\n",
                    mWeAskedForVerification[pnode->addr].nBlockHeight, mnv.nBlockHeight, pnode->id);
        Misbehaving(pnode->id, 20);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight)) {
        // this shouldn't happen...
        LogPrintf("PopnodeMan::ProcessVerifyReply -- can't get block hash for unknown block height %d, peer=%d\n", mnv.nBlockHeight, pnode->id);
        return;
    }

    // we already verified this address, why node is spamming?
    if(netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-done")) {
        LogPrintf("CPopnodeMan::ProcessVerifyReply -- ERROR: already verified %s recently\n", pnode->addr.ToString());
        Misbehaving(pnode->id, 20);
        return;
    }

    {
        LOCK(cs);

        CPopnode* prealPopnode = NULL;
        std::vector<CPopnode*> vpPopnodesToBan;
        std::vector<CPopnode>::iterator it = vPopnodes.begin();
        std::string strMessage1 = strprintf("%s%d%s", pnode->addr.ToString(false), mnv.nonce, blockHash.ToString());
        while(it != vPopnodes.end()) {
            if((CAddress)it->addr == pnode->addr) {
                if(darkSendSigner.VerifyMessage(it->pubKeyPopnode, mnv.vchSig1, strMessage1, strError)) {
                    // found it!
                    prealPopnode = &(*it);
                    if(!it->IsPoSeVerified()) {
                        it->DecreasePoSeBanScore();
                    }
                    netfulfilledman.AddFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-done");

                    // we can only broadcast it if we are an activated popnode
                    if(activePopnode.vin == CTxIn()) continue;
                    // update ...
                    mnv.addr = it->addr;
                    mnv.vin1 = it->vin;
                    mnv.vin2 = activePopnode.vin;
                    std::string strMessage2 = strprintf("%s%d%s%s%s", mnv.addr.ToString(false), mnv.nonce, blockHash.ToString(),
                                            mnv.vin1.prevout.ToStringShort(), mnv.vin2.prevout.ToStringShort());
                    // ... and sign it
                    if(!darkSendSigner.SignMessage(strMessage2, mnv.vchSig2, activePopnode.keyPopnode)) {
                        LogPrintf("PopnodeMan::ProcessVerifyReply -- SignMessage() failed\n");
                        return;
                    }

                    std::string strError;

                    if(!darkSendSigner.VerifyMessage(activePopnode.pubKeyPopnode, mnv.vchSig2, strMessage2, strError)) {
                        LogPrintf("PopnodeMan::ProcessVerifyReply -- VerifyMessage() failed, error: %s\n", strError);
                        return;
                    }

                    mWeAskedForVerification[pnode->addr] = mnv;
                    mnv.Relay();

                } else {
                    vpPopnodesToBan.push_back(&(*it));
                }
            }
            ++it;
        }
        // no real popnode found?...
        if(!prealPopnode) {
            // this should never be the case normally,
            // only if someone is trying to game the system in some way or smth like that
            LogPrintf("CPopnodeMan::ProcessVerifyReply -- ERROR: no real popnode found for addr %s\n", pnode->addr.ToString());
            Misbehaving(pnode->id, 20);
            return;
        }
        LogPrintf("CPopnodeMan::ProcessVerifyReply -- verified real popnode %s for addr %s\n",
                    prealPopnode->vin.prevout.ToStringShort(), pnode->addr.ToString());
        // increase ban score for everyone else
        BOOST_FOREACH(CPopnode* pmn, vpPopnodesToBan) {
            pmn->IncreasePoSeBanScore();
            LogPrint("popnode", "CPopnodeMan::ProcessVerifyBroadcast -- increased PoSe ban score for %s addr %s, new score %d\n",
                        prealPopnode->vin.prevout.ToStringShort(), pnode->addr.ToString(), pmn->nPoSeBanScore);
        }
        LogPrintf("CPopnodeMan::ProcessVerifyBroadcast -- PoSe score increased for %d fake popnodes, addr %s\n",
                    (int)vpPopnodesToBan.size(), pnode->addr.ToString());
    }
}

void CPopnodeMan::ProcessVerifyBroadcast(CNode* pnode, const CPopnodeVerification& mnv)
{
    std::string strError;

    if(mapSeenPopnodeVerification.find(mnv.GetHash()) != mapSeenPopnodeVerification.end()) {
        // we already have one
        return;
    }
    mapSeenPopnodeVerification[mnv.GetHash()] = mnv;

    // we don't care about history
    if(mnv.nBlockHeight < pCurrentBlockIndex->nHeight - MAX_POSE_BLOCKS) {
        LogPrint("popnode", "PopnodeMan::ProcessVerifyBroadcast -- Outdated: current block %d, verification block %d, peer=%d\n",
                    pCurrentBlockIndex->nHeight, mnv.nBlockHeight, pnode->id);
        return;
    }

    if(mnv.vin1.prevout == mnv.vin2.prevout) {
        LogPrint("popnode", "PopnodeMan::ProcessVerifyBroadcast -- ERROR: same vins %s, peer=%d\n",
                    mnv.vin1.prevout.ToStringShort(), pnode->id);
        // that was NOT a good idea to cheat and verify itself,
        // ban the node we received such message from
        Misbehaving(pnode->id, 100);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight)) {
        // this shouldn't happen...
        LogPrintf("PopnodeMan::ProcessVerifyBroadcast -- Can't get block hash for unknown block height %d, peer=%d\n", mnv.nBlockHeight, pnode->id);
        return;
    }

    int nRank = GetPopnodeRank(mnv.vin2, mnv.nBlockHeight, MIN_POSE_PROTO_VERSION);
    if(nRank < MAX_POSE_RANK) {
        LogPrint("popnode", "PopnodeMan::ProcessVerifyBroadcast -- Mastrernode is not in top %d, current rank %d, peer=%d\n",
                    (int)MAX_POSE_RANK, nRank, pnode->id);
        return;
    }

    {
        LOCK(cs);

        std::string strMessage1 = strprintf("%s%d%s", mnv.addr.ToString(false), mnv.nonce, blockHash.ToString());
        std::string strMessage2 = strprintf("%s%d%s%s%s", mnv.addr.ToString(false), mnv.nonce, blockHash.ToString(),
                                mnv.vin1.prevout.ToStringShort(), mnv.vin2.prevout.ToStringShort());

        CPopnode* pmn1 = Find(mnv.vin1);
        if(!pmn1) {
            LogPrintf("CPopnodeMan::ProcessVerifyBroadcast -- can't find popnode1 %s\n", mnv.vin1.prevout.ToStringShort());
            return;
        }

        CPopnode* pmn2 = Find(mnv.vin2);
        if(!pmn2) {
            LogPrintf("CPopnodeMan::ProcessVerifyBroadcast -- can't find popnode2 %s\n", mnv.vin2.prevout.ToStringShort());
            return;
        }

        if(pmn1->addr != mnv.addr) {
            LogPrintf("CPopnodeMan::ProcessVerifyBroadcast -- addr %s do not match %s\n", mnv.addr.ToString(), pnode->addr.ToString());
            return;
        }

        if(darkSendSigner.VerifyMessage(pmn1->pubKeyPopnode, mnv.vchSig1, strMessage1, strError)) {
            LogPrintf("PopnodeMan::ProcessVerifyBroadcast -- VerifyMessage() for popnode1 failed, error: %s\n", strError);
            return;
        }

        if(darkSendSigner.VerifyMessage(pmn2->pubKeyPopnode, mnv.vchSig2, strMessage2, strError)) {
            LogPrintf("PopnodeMan::ProcessVerifyBroadcast -- VerifyMessage() for popnode2 failed, error: %s\n", strError);
            return;
        }

        if(!pmn1->IsPoSeVerified()) {
            pmn1->DecreasePoSeBanScore();
        }
        mnv.Relay();

        LogPrintf("CPopnodeMan::ProcessVerifyBroadcast -- verified popnode %s for addr %s\n",
                    pmn1->vin.prevout.ToStringShort(), pnode->addr.ToString());

        // increase ban score for everyone else with the same addr
        int nCount = 0;
        BOOST_FOREACH(CPopnode& mn, vPopnodes) {
            if(mn.addr != mnv.addr || mn.vin.prevout == mnv.vin1.prevout) continue;
            mn.IncreasePoSeBanScore();
            nCount++;
            LogPrint("popnode", "CPopnodeMan::ProcessVerifyBroadcast -- increased PoSe ban score for %s addr %s, new score %d\n",
                        mn.vin.prevout.ToStringShort(), mn.addr.ToString(), mn.nPoSeBanScore);
        }
        LogPrintf("CPopnodeMan::ProcessVerifyBroadcast -- PoSe score incresed for %d fake popnodes, addr %s\n",
                    nCount, pnode->addr.ToString());
    }
}

std::string CPopnodeMan::ToString() const
{
    std::ostringstream info;

    info << "Popnodes: " << (int)vPopnodes.size() <<
            ", peers who asked us for Popnode list: " << (int)mAskedUsForPopnodeList.size() <<
            ", peers we asked for Popnode list: " << (int)mWeAskedForPopnodeList.size() <<
            ", entries in Popnode list we asked for: " << (int)mWeAskedForPopnodeListEntry.size() <<
            ", popnode index size: " << indexPopnodes.GetSize() <<
            ", nDsqCount: " << (int)nDsqCount;

    return info.str();
}

void CPopnodeMan::UpdatePopnodeList(CPopnodeBroadcast mnb)
{
    LOCK(cs);
    mapSeenPopnodePing.insert(std::make_pair(mnb.lastPing.GetHash(), mnb.lastPing));
    mapSeenPopnodeBroadcast.insert(std::make_pair(mnb.GetHash(), std::make_pair(GetTime(), mnb)));

    LogPrintf("CPopnodeMan::UpdatePopnodeList -- popnode=%s  addr=%s\n", mnb.vin.prevout.ToStringShort(), mnb.addr.ToString());

    CPopnode* pmn = Find(mnb.vin);
    if(pmn == NULL) {
        CPopnode mn(mnb);
        if(Add(mn)) {
            popnodeSync.AddedPopnodeList();
        }
    } else {
        CPopnodeBroadcast mnbOld = mapSeenPopnodeBroadcast[CPopnodeBroadcast(*pmn).GetHash()].second;
        if(pmn->UpdateFromNewBroadcast(mnb)) {
            popnodeSync.AddedPopnodeList();
            mapSeenPopnodeBroadcast.erase(mnbOld.GetHash());
        }
    }
}

bool CPopnodeMan::CheckMnbAndUpdatePopnodeList(CNode* pfrom, CPopnodeBroadcast mnb, int& nDos)
{
    // Need LOCK2 here to ensure consistent locking order because the SimpleCheck call below locks cs_main
    LOCK2(cs_main, cs);

    nDos = 0;
    LogPrint("popnode", "CPopnodeMan::CheckMnbAndUpdatePopnodeList -- popnode=%s\n", mnb.vin.prevout.ToStringShort());

    uint256 hash = mnb.GetHash();
    if(mapSeenPopnodeBroadcast.count(hash) && !mnb.fRecovery) { //seen
        LogPrint("popnode", "CPopnodeMan::CheckMnbAndUpdatePopnodeList -- popnode=%s seen\n", mnb.vin.prevout.ToStringShort());
        // less then 2 pings left before this MN goes into non-recoverable state, bump sync timeout
        if(GetTime() - mapSeenPopnodeBroadcast[hash].first > POPNODE_NEW_START_REQUIRED_SECONDS - POPNODE_MIN_MNP_SECONDS * 2) {
            LogPrint("popnode", "CPopnodeMan::CheckMnbAndUpdatePopnodeList -- popnode=%s seen update\n", mnb.vin.prevout.ToStringShort());
            mapSeenPopnodeBroadcast[hash].first = GetTime();
            popnodeSync.AddedPopnodeList();
        }
        // did we ask this node for it?
        if(pfrom && IsMnbRecoveryRequested(hash) && GetTime() < mMnbRecoveryRequests[hash].first) {
            LogPrint("popnode", "CPopnodeMan::CheckMnbAndUpdatePopnodeList -- mnb=%s seen request\n", hash.ToString());
            if(mMnbRecoveryRequests[hash].second.count(pfrom->addr)) {
                LogPrint("popnode", "CPopnodeMan::CheckMnbAndUpdatePopnodeList -- mnb=%s seen request, addr=%s\n", hash.ToString(), pfrom->addr.ToString());
                // do not allow node to send same mnb multiple times in recovery mode
                mMnbRecoveryRequests[hash].second.erase(pfrom->addr);
                // does it have newer lastPing?
                if(mnb.lastPing.sigTime > mapSeenPopnodeBroadcast[hash].second.lastPing.sigTime) {
                    // simulate Check
                    CPopnode mnTemp = CPopnode(mnb);
                    mnTemp.Check();
                    LogPrint("popnode", "CPopnodeMan::CheckMnbAndUpdatePopnodeList -- mnb=%s seen request, addr=%s, better lastPing: %d min ago, projected mn state: %s\n", hash.ToString(), pfrom->addr.ToString(), (GetTime() - mnb.lastPing.sigTime)/60, mnTemp.GetStateString());
                    if(mnTemp.IsValidStateForAutoStart(mnTemp.nActiveState)) {
                        // this node thinks it's a good one
                        LogPrint("popnode", "CPopnodeMan::CheckMnbAndUpdatePopnodeList -- popnode=%s seen good\n", mnb.vin.prevout.ToStringShort());
                        mMnbRecoveryGoodReplies[hash].push_back(mnb);
                    }
                }
            }
        }
        return true;
    }
    mapSeenPopnodeBroadcast.insert(std::make_pair(hash, std::make_pair(GetTime(), mnb)));

    LogPrint("popnode", "CPopnodeMan::CheckMnbAndUpdatePopnodeList -- popnode=%s new\n", mnb.vin.prevout.ToStringShort());

    if(!mnb.SimpleCheck(nDos)) {
        LogPrint("popnode", "CPopnodeMan::CheckMnbAndUpdatePopnodeList -- SimpleCheck() failed, popnode=%s\n", mnb.vin.prevout.ToStringShort());
        return false;
    }

    // search Popnode list
    CPopnode* pmn = Find(mnb.vin);
    if(pmn) {
        CPopnodeBroadcast mnbOld = mapSeenPopnodeBroadcast[CPopnodeBroadcast(*pmn).GetHash()].second;
        if(!mnb.Update(pmn, nDos)) {
            LogPrint("popnode", "CPopnodeMan::CheckMnbAndUpdatePopnodeList -- Update() failed, popnode=%s\n", mnb.vin.prevout.ToStringShort());
            return false;
        }
        if(hash != mnbOld.GetHash()) {
            mapSeenPopnodeBroadcast.erase(mnbOld.GetHash());
        }
    } else {
        if(mnb.CheckOutpoint(nDos)) {
            Add(mnb);
            popnodeSync.AddedPopnodeList();
            // if it matches our Popnode privkey...
            if(fPopNode && mnb.pubKeyPopnode == activePopnode.pubKeyPopnode) {
                mnb.nPoSeBanScore = -POPNODE_POSE_BAN_MAX_SCORE;
                if(mnb.nProtocolVersion == PROTOCOL_VERSION) {
                    // ... and PROTOCOL_VERSION, then we've been remotely activated ...
                    LogPrintf("CPopnodeMan::CheckMnbAndUpdatePopnodeList -- Got NEW Popnode entry: popnode=%s  sigTime=%lld  addr=%s\n",
                                mnb.vin.prevout.ToStringShort(), mnb.sigTime, mnb.addr.ToString());
                    activePopnode.ManageState();
                } else {
                    // ... otherwise we need to reactivate our node, do not add it to the list and do not relay
                    // but also do not ban the node we get this message from
                    LogPrintf("CPopnodeMan::CheckMnbAndUpdatePopnodeList -- wrong PROTOCOL_VERSION, re-activate your MN: message nProtocolVersion=%d  PROTOCOL_VERSION=%d\n", mnb.nProtocolVersion, PROTOCOL_VERSION);
                    return false;
                }
            }
            mnb.Relay();
        } else {
            LogPrintf("CPopnodeMan::CheckMnbAndUpdatePopnodeList -- Rejected Popnode entry: %s  addr=%s\n", mnb.vin.prevout.ToStringShort(), mnb.addr.ToString());
            return false;
        }
    }

    return true;
}

void CPopnodeMan::CheckAndRebuildPopnodeIndex()
{
    LOCK(cs);

    if(GetTime() - nLastIndexRebuildTime < MIN_INDEX_REBUILD_TIME) {
        return;
    }

    if(indexPopnodes.GetSize() <= MAX_EXPECTED_INDEX_SIZE) {
        return;
    }

    if(indexPopnodes.GetSize() <= int(vPopnodes.size())) {
        return;
    }

    indexPopnodesOld = indexPopnodes;
    indexPopnodes.Clear();
    for(size_t i = 0; i < vPopnodes.size(); ++i) {
        indexPopnodes.AddPopnodeVIN(vPopnodes[i].vin);
    }

    fIndexRebuilt = true;
    nLastIndexRebuildTime = GetTime();
}

void CPopnodeMan::UpdateWatchdogVoteTime(const CTxIn& vin)
{
    LOCK(cs);
    CPopnode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->UpdateWatchdogVoteTime();
    nLastWatchdogVoteTime = GetTime();
}

bool CPopnodeMan::IsWatchdogActive()
{
    LOCK(cs);
    // Check if any popnodes have voted recently, otherwise return false
    return (GetTime() - nLastWatchdogVoteTime) <= POPNODE_WATCHDOG_MAX_SECONDS;
}

void CPopnodeMan::CheckPopnode(const CTxIn& vin, bool fForce)
{
    LOCK(cs);
    CPopnode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->Check(fForce);
}

void CPopnodeMan::CheckPopnode(const CPubKey& pubKeyPopnode, bool fForce)
{
    LOCK(cs);
    CPopnode* pMN = Find(pubKeyPopnode);
    if(!pMN)  {
        return;
    }
    pMN->Check(fForce);
}

int CPopnodeMan::GetPopnodeState(const CTxIn& vin)
{
    LOCK(cs);
    CPopnode* pMN = Find(vin);
    if(!pMN)  {
        return CPopnode::POPNODE_NEW_START_REQUIRED;
    }
    return pMN->nActiveState;
}

int CPopnodeMan::GetPopnodeState(const CPubKey& pubKeyPopnode)
{
    LOCK(cs);
    CPopnode* pMN = Find(pubKeyPopnode);
    if(!pMN)  {
        return CPopnode::POPNODE_NEW_START_REQUIRED;
    }
    return pMN->nActiveState;
}

bool CPopnodeMan::IsPopnodePingedWithin(const CTxIn& vin, int nSeconds, int64_t nTimeToCheckAt)
{
    LOCK(cs);
    CPopnode* pMN = Find(vin);
    if(!pMN) {
        return false;
    }
    return pMN->IsPingedWithin(nSeconds, nTimeToCheckAt);
}

void CPopnodeMan::SetPopnodeLastPing(const CTxIn& vin, const CPopnodePing& mnp)
{
    LOCK(cs);
    CPopnode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->lastPing = mnp;
    mapSeenPopnodePing.insert(std::make_pair(mnp.GetHash(), mnp));

    CPopnodeBroadcast mnb(*pMN);
    uint256 hash = mnb.GetHash();
    if(mapSeenPopnodeBroadcast.count(hash)) {
        mapSeenPopnodeBroadcast[hash].second.lastPing = mnp;
    }
}

// Popchain DevTeam
void CPopnodeMan::UpdatedBlockTip(const CBlockIndex *pindex)
{
    pCurrentBlockIndex = pindex;
    LogPrint("popnode", "CPopnodeMan::UpdatedBlockTip -- pCurrentBlockIndex->nHeight=%d\n", pCurrentBlockIndex->nHeight);

    CheckSameAddr();

    if(fPopNode) {
        DoFullVerificationStep();
    }
}

// Popchain DevTeam
void CPopnodeMan::NotifyPopnodeUpdates()
{
    LOCK(cs);
    fPopnodesAdded = false;
    fPopnodesRemoved = false;
}
