// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef BITCOIN_DSANNOUNCEMENTINTERFACE_H
#define BITCOIN_DSANNOUNCEMENTINTERFACE_H

#include "validationinterface.h"

class CDSAnnouncementInterface : public CValidationInterface
{
public:
    // virtual CDSAnnouncementInterface();
    CDSAnnouncementInterface();
    virtual ~CDSAnnouncementInterface();

protected:
    // CValidationInterface
    void SyncTransaction(const CTransaction &tx, const CBlock *pblock);
	void UpdatedBlockTip(const CBlockIndex *pindex);

private:
};

#endif // BITCOIN_DSANNOUNCEMENTINTERFACE_H
