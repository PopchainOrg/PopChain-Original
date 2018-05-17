// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef C_SHA256_H
#define C_SHA256_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

	/*
	 * FUNCTION：one-way function SHA256
	 *      1. input : message
	 *		2. output：return
	*/
	void crypto_sha256(uint8_t *input, uint32_t inputLen, uint8_t *output);

#ifdef __cplusplus
}
#endif


#endif
