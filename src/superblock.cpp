// Copyright (c) 2017-2018 The Popchain Core Developers

//#define ENABLE_UC_DEBUG

#include "core_io.h"
#include "superblock.h"
#include "init.h"
#include "main.h"
#include "chainparams.h"
#include "utilstrencodings.h"
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

#include <univalue.h>

/**
*   Is Superblock Triggered
*
*   - Does this block have a non-executed and actived trigger?
*/
// Popchain DevTeam
bool CSuperblockManager::IsSuperblockTriggered(int nBlockHeight)
{
    LogPrint("gobject", "CSuperblockManager::IsSuperblockTriggered -- Start nBlockHeight = %d\n", nBlockHeight);
    if (!CSuperblock::IsValidBlockHeight(nBlockHeight)) {
        return false;
    }
    else LogPrintf("SuperBlockHeight check right %d\n",nBlockHeight);
    return true;
}


/**
*    Append Founders reward
*    Popchain DevTeam
*/

void CSuperblockManager::AppendFoundersReward(CMutableTransaction& txNewRet, int nBlockHeight,CTxOut&  txoutFound)
{
    // add founders reward
    const Consensus::Params& cp = Params().GetConsensus();
    const CAmount foundersReward = cp.foundersReward;
    const CScript foundersScript = Params().GetFoundersRewardScriptAtHeight(nBlockHeight);
    
    CTxOut txout = CTxOut(foundersReward, foundersScript);
    txNewRet.vout.push_back(txout);
    //voutSuperblockRet.push_back(txout);

    // PRINT NICE LOG OUTPUT FOR SUPERBLOCK PAYMENT
    txoutFound=txout;
    CTxDestination address1;
    ExtractDestination(foundersScript, address1);
    CBitcoinAddress address2(address1);

    // TODO: PRINT NICE N.N PCH OUTPUT

    DBG( cout << "CSuperblockManager::AppendFoundersReward Before LogPrintf call, nAmount = " << foundersReward << endl; );
    LogPrintf("NEW Superblock : output to founders (addr %s, amount %d)\n", address2.ToString(), foundersReward);
    DBG( cout << "CSuperblockManager::AppendFoundersReward After LogPrintf call " << endl; );
}

/**
*   Create Superblock Payments
*
*   - Create the correct payment structure for a given superblock
*/
//popchain
void CSuperblockManager::CreateSuperblock(CMutableTransaction& txNewRet, int nBlockHeight, CTxOut&  txoutFound)
{
    DBG( cout << "CSuperblockManager::CreateSuperblock Start" << endl; );

    LogPrintf("CSuperblockManager::CreateSuperblock -- start superblock.\n");

    // add founders reward
    if (nBlockHeight > 1 && nBlockHeight < Params().GetConsensus().endOfFoundersReward())
    {
        AppendFoundersReward(txNewRet, nBlockHeight,txoutFound);
    }
}



CSuperblock::
CSuperblock()
{}

CSuperblock::
CSuperblock(uint256& nHash)
{}

/**
 *   Is Valid Superblock Height
 *		
 *   - See if a block at this height can be a superblock
 *   Popchain DevTeam
 */

bool CSuperblock::IsValidBlockHeight(int nBlockHeight)
{
    // SUPERBLOCKS CAN HAPPEN ONLY after hardfork and only ONCE PER CYCLE
    return nBlockHeight >= Params().GetConsensus().nSuperblockStartBlock &&
            ((nBlockHeight % Params().GetConsensus().nSuperblockCycle) == 0);
}


// Popchain DevTeam
CAmount CSuperblock::GetPaymentsLimit(int nBlockHeight)
{
    const Consensus::Params& consensusParams = Params().GetConsensus();

    if(!IsValidBlockHeight(nBlockHeight)) {
        return 0;
    }
    CAmount nPaymentsLimit = GetBlockSubsidy(nBlockHeight, consensusParams);
    LogPrint("gobject", "CSuperblock::GetPaymentsLimit -- Valid superblock height %d, payments max %lld\n", nBlockHeight, nPaymentsLimit);
    LogPrintf("popchain CSuperblock::GetPaymentsLimit -- Valid superblock height %d, payments max %lld\n", nBlockHeight, nPaymentsLimit);
    return nPaymentsLimit;
}


/**
*   Is Transaction Valid
*   Popchain DevTeam
*/
bool CSuperblock::IsFounderValid(const CTransaction& txNew, int nBlockHeight, CAmount blockReward)
{
    // founder reward check
    // it's ok to use founders address as budget address ?
     LogPrint("gobject", "IsFounderValid nBlockHeight = %d \n",
             nBlockHeight);
    const Consensus::Params& cp = Params().GetConsensus();
    CAmount foundersActual(0), foundersExpected(GetFoundersReward(nBlockHeight, cp));
    CAmount nBlockValue = txNew.GetValueOut();
    if (nBlockHeight > 1 && nBlockHeight < cp.endOfFoundersReward())
    {
	CBitcoinAddress address(Params().GetFoundersRewardAddressAtHeight(nBlockHeight));    
	   
        for (const CTxOut &out: txNew.vout)
        {
            CTxDestination address1;
            ExtractDestination(out.scriptPubKey, address1);
            CBitcoinAddress address2(address1);
            //CBitcoinAddress address2(CScriptID(out.scriptPubKey));                                                                                                                                              
            LogPrintf(" out.scriptPubKey [%s]  [%s] \n",address2.ToString(), address.ToString());
            //if (out.scriptPubKey == Params().GetFoundersRewardScriptAtHeight(nBlockHeight))
            if (address2 == address)
            {
                foundersActual += out.nValue;
            }
        }
        if (foundersActual != foundersExpected)
        {
    	    if (foundersActual == 0)
	        {
    	        LogPrintf("CSuperblock::IsValid -- ERROR: Block invalid, founders reward missing: block %lld, expected value  %lld\n", nBlockValue, foundersExpected);
	            return false;
	        }
            LogPrintf("CSuperblock::IsValid -- ERROR: Block invalid, wrong founders reward: block %lld, actual value %lld, expected value  %lld\n", nBlockValue, foundersActual, foundersExpected);
            return false;
        }
    }

    
    if(nBlockValue > blockReward + foundersExpected)
    {
        LogPrintf("CSuperblock::IsValid -- ERROR: Block invalid, block value limit exceeded: block %lld, limit %lld\n", nBlockValue, blockReward + foundersExpected);
        return false;
    }
    return true;
}
