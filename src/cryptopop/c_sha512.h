// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef C_SHA512_H
#define C_SHA512_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

	/*
     * FUNCTION：one-way function SHA512
     *      1. input : message
     *		2. output：return
	*/
	extern void crypto_sha512(uint8_t *input, uint32_t inputLen, uint8_t *output);

#ifdef __cplusplus
}
#endif


#endif
