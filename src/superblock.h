// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

//#define ENABLE_UC_DEBUG

#include "base58.h"
#include "key.h"
#include "script/standard.h"
#include "util.h"

#include <boost/shared_ptr.hpp>

class CSuperblock;
class CSuperblockManager;

static const int TRIGGER_UNKNOWN            = -1;
static const int TRIGGER_SUPERBLOCK         = 1000;

typedef boost::shared_ptr<CSuperblock> CSuperblock_sptr;

/**
*   Superblock Manager
*
*   Class for querying superblock information
*/

class CSuperblockManager
{
public:
    static bool IsSuperblockTriggered(int nBlockHeight);
    static void AppendFoundersReward(CMutableTransaction &txNewRet, int nBlockHeight,CTxOut&  txoutFound);
    static void CreateSuperblock(CMutableTransaction& txNewRet, int nBlockHeight, CTxOut&  txoutFound);
};


// Popchain DevTeam
class CSuperblock
{
private:

public:

    CSuperblock();
    CSuperblock(uint256& nHash);

    static bool IsValidBlockHeight(int nBlockHeight);
    static CAmount GetPaymentsLimit(int nBlockHeight); 
	static bool IsFounderValid(const CTransaction& txNew, int nBlockHeight, CAmount blockReward);

    // may use in the future
    bool IsValid(const CTransaction& txNew, int nBlockHeight, CAmount blockReward);
};

#endif
