// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef POPNODE_PAYMENTS_H
#define POPNODE_PAYMENTS_H

#include "util.h"
#include "core_io.h"
#include "key.h"
#include "main.h"
#include "popnode.h"
#include "utilstrencodings.h"

bool IsBlockValueValid(const CBlock& block, int nBlockHeight, CAmount blockReward, std::string &strErrorRet);
bool IsBlockPayeeValid(const CTransaction& txNew, int nBlockHeight, CAmount blockReward);

void FillBlockPayments(CMutableTransaction& txNew, int nBlockHeight, CAmount blockReward, CTxOut&  txoutFound);

#endif
