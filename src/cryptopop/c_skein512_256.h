// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef C_SKEIN512_256_H
#define C_SKEIN512_256_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

	/*
     * FUNCTION：one-way function Skein-512(256bits)
     *      1. input : message
     *		2. output：return
	*/
	void crypto_skein512_256(uint8_t *input, uint32_t inputLen, uint8_t *output);

#ifdef __cplusplus
}
#endif

#endif
