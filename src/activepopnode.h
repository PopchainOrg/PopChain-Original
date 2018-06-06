// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef ACTIVEPOPNODE_H
#define ACTIVEPOPNODE_H

#include "net.h"
#include "key.h"
#include "wallet/wallet.h"

class CActivePopnode;

static const int ACTIVE_POPNODE_INITIAL          = 0; // initial state
static const int ACTIVE_POPNODE_SYNC_IN_PROCESS  = 1;
static const int ACTIVE_POPNODE_INPUT_TOO_NEW    = 2;
static const int ACTIVE_POPNODE_NOT_CAPABLE      = 3;
static const int ACTIVE_POPNODE_STARTED          = 4;

extern CActivePopnode activePopnode;

// Responsible for activating the Popnode and pinging the network
class CActivePopnode
{
public:
    enum popnode_type_enum_t {
        POPNODE_UNKNOWN = 0,
        POPNODE_REMOTE  = 1,
        POPNODE_LOCAL   = 2
    };

private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    popnode_type_enum_t eType;

    bool fPingerEnabled;

    /// Ping Popnode
    bool SendPopnodePing();

public:
    // Keys for the active Popnode
    CPubKey pubKeyPopnode;
    CKey keyPopnode;

    // Initialized while registering Popnode
    CTxIn vin;
    CService service;

    int nState; // should be one of ACTIVE_POPNODE_XXXX
    std::string strNotCapableReason;

    CActivePopnode()
        : eType(POPNODE_UNKNOWN),
          fPingerEnabled(false),
          pubKeyPopnode(),
          keyPopnode(),
          vin(),
          service(),
          nState(ACTIVE_POPNODE_INITIAL)
    {}

    /// Manage state of active Popnode
    void ManageState();

    std::string GetStateString() const;
    std::string GetStatus() const;
    std::string GetTypeString() const;

private:
    void ManageStateInitial();
    void ManageStateRemote();
    void ManageStateLocal();
};

#endif
