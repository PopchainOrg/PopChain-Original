// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef C_GOST_H
#define C_GOST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

	/*
	 * FUNCTION：one-way function GOST R 34.11-94
	 *      1. input : message
	 *		2. output：return
	*/
	void gost_init_table(void);
	void crypto_gost(uint8_t *input, uint32_t inputLen, uint8_t *output);

#ifdef __cplusplus
}
#endif

#endif
