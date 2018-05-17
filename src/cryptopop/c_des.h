// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef C_DES_H
#define C_DES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

	/*
	 * FUNCTION：one-way function DES
	 *      1. input : message
	 *		2. output：return
	*/
	void crypto_des(uint8_t *input, uint32_t inputLen, uint8_t *output);

#ifdef __cplusplus
}
#endif

#endif
