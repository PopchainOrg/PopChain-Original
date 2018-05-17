// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef C_AES128_H
#define C_AES128_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

	/*
     * FUNCTION：one-way function AES128
     *      1. input : message
     *		2. output：return
	*/
	void crypto_aes128(uint8_t *input, uint32_t inputLen, uint8_t *output);

#ifdef __cplusplus
}
#endif

#endif
