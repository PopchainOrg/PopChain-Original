// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef C_RIPEMD160_H
#define C_RIPEMD160_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

	/*
	 * FUNCTION：one-way function RipeMD160
	 *      1. input : message
	 *		2. output：return
	*/
	void crypto_ripemd160(uint8_t *input, uint32_t inputLen, uint8_t *output);

#ifdef __cplusplus
}
#endif	

#endif
