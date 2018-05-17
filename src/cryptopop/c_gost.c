// Copyright (c) 2017-2018 The Popchain Core Developers

#include "c_gost.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "common.h"
#include "jtr_gost.h"

#define GOST_BINARY_SIZE	32

/*
 * FUNCTION：one-way function GOST R 34.11-94
 *      1. input : message
 *		2. output：return
*/
void crypto_gost(uint8_t *input, uint32_t inputLen, uint8_t *output) {
	uint8_t result[GOST_BINARY_SIZE];

	gost_ctx ctx;
	john_gost_init(&ctx);
	john_gost_update(&ctx, input, inputLen);
	john_gost_final(&ctx, result);
	
	memcpy(output, result, OUTPUT_LEN*sizeof(uint8_t));
}
