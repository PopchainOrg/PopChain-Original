﻿// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef C_SHA3_256_H
#define C_SHA3_256_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

	/*
	 * FUNCTION：one-way function SHA3-256
	 *      1. input : message
	 *		2. output：return
	*/
	void crypto_sha3_256(uint8_t *input, uint32_t inputLen, uint8_t *output);

#ifdef __cplusplus
}
#endif


#endif