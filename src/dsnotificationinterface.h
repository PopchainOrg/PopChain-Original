// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef BITCOIN_DSNOTIFICATIONINTERFACE_H
#define BITCOIN_DSNOTIFICATIONINTERFACE_H

#include "validationinterface.h"

class CDSNotificationInterface : public CValidationInterface
{
public:
    // virtual CDSNotificationInterface();
    CDSNotificationInterface();
    virtual ~CDSNotificationInterface();

protected:
    // CValidationInterface
    void UpdatedBlockTip(const CBlockIndex *pindex);
    void SyncTransaction(const CTransaction &tx, const CBlock *pblock);

private:
};

#endif // BITCOIN_DSNOTIFICATIONINTERFACE_H
