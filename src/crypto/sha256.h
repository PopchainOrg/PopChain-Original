// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef BITCOIN_CRYPTO_SHA256_H
#define BITCOIN_CRYPTO_SHA256_H

#include <stdint.h>
#include <stdlib.h>

/** A hasher class for SHA-256. */
class CSHA256
{
private:
    uint32_t s[8];
    unsigned char buf[64];
    size_t bytes;

public:
    static const size_t OUTPUT_SIZE = 32;

    CSHA256();
    CSHA256& Write(const unsigned char* data, size_t len);
    void Finalize(unsigned char hash[OUTPUT_SIZE]);
    CSHA256& Reset();
};

#endif // BITCOIN_CRYPTO_SHA256_H
