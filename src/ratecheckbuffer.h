// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef RATECHECKBUFFER_H
#define RATECHECKBUFFER_H

//#define ENABLE_UC_DEBUG

#include "bloom.h"
#include "cachemap.h"
#include "cachemultimap.h"
#include "chain.h"
#include "net.h"
#include "sync.h"
#include "timedata.h"
#include "util.h"

using namespace std;

static const int RATE_BUFFER_SIZE = 5;

class CRateCheckBuffer {
private:
    std::vector<int64_t> vecTimestamps;

    int nDataStart;

    int nDataEnd;

    bool fBufferEmpty;

public:
    CRateCheckBuffer()
        : vecTimestamps(RATE_BUFFER_SIZE),
          nDataStart(0),
          nDataEnd(0),
          fBufferEmpty(true)
        {}

    void AddTimestamp(int64_t nTimestamp)
    {
        if((nDataEnd == nDataStart) && !fBufferEmpty) {
            // Buffer full, discard 1st element
            nDataStart = (nDataStart + 1) % RATE_BUFFER_SIZE;
        }
        vecTimestamps[nDataEnd] = nTimestamp;
        nDataEnd = (nDataEnd + 1) % RATE_BUFFER_SIZE;
        fBufferEmpty = false;
    }

    int64_t GetMinTimestamp()
    {
        int nIndex = nDataStart;
        int64_t nMin = numeric_limits<int64_t>::max();
        if(fBufferEmpty) {
            return nMin;
        }
        do {
            if(vecTimestamps[nIndex] < nMin) {
                nMin = vecTimestamps[nIndex];
            }
            nIndex = (nIndex + 1) % RATE_BUFFER_SIZE;
        } while(nIndex != nDataEnd);
        return nMin;
    }

    int64_t GetMaxTimestamp()
    {
        int nIndex = nDataStart;
        int64_t nMax = 0;
        if(fBufferEmpty) {
            return nMax;
        }
        do {
            if(vecTimestamps[nIndex] > nMax) {
                nMax = vecTimestamps[nIndex];
            }
            nIndex = (nIndex + 1) % RATE_BUFFER_SIZE;
        } while(nIndex != nDataEnd);
        return nMax;
    }

    int GetCount()
    {
        int nCount = 0;
        if(fBufferEmpty) {
            return 0;
        }
        if(nDataEnd > nDataStart) {
            nCount = nDataEnd - nDataStart;
        }
        else {
            nCount = RATE_BUFFER_SIZE - nDataStart + nDataEnd;
        }

        return nCount;
    }

    double GetRate()
    {
        int nCount = GetCount();
        if(nCount < 2) {
            return 0.0;
        }
        int64_t nMin = GetMinTimestamp();
        int64_t nMax = GetMaxTimestamp();
        if(nMin == nMax) {
            // multiple objects with the same timestamp => infinite rate
            return 1.0e10;
        }
        return double(nCount) / double(nMax - nMin);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(vecTimestamps);
        READWRITE(nDataStart);
        READWRITE(nDataEnd);
        READWRITE(fBufferEmpty);
    }
};


#endif
