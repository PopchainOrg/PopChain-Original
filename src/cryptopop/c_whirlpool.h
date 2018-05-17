// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef C_WHIRLPOOL_H
#define C_WHIRLPOOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

	/*
     * FUNCTION：one-way function Whirlpool
     *      1. input : message
     *		2. output：return
	*/
	void crypto_whirlpool(uint8_t *input, uint32_t inputLen, uint8_t *output);

#ifdef __cplusplus
}
#endif
	
#endif
