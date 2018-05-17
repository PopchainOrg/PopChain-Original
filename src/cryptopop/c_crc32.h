// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef C_CRC32_H
#define C_CRC32_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

	/*
	 * FUNCTION：one-way function CRC32
	 *      1. input : message
	 *		2. output：return
	*/
	void CRC32_Table_Init();
	void crypto_crc32(uint8_t *input, uint32_t inputLen, uint8_t *output);

#ifdef __cplusplus
}
#endif

#endif
