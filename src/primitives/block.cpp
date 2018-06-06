// Copyright (c) 2017-2018 The Popchain Core Developers

#include "primitives/block.h"

#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"
//#include "crypto/common.h"

uint256 CBlockHeader::GetHash() const
{
//	return SerializeHash(*this);
	uint256 hash;

	CryptoPop(this, (unsigned char *)&hash);
//	view_data_u8("PoW 3", (unsigned char *)&hash, OUTPUT_LEN); 
//	std::cout<<"gethex() ="<<hash.GetHex()<<std::endl;
//	std::cout<<"tostring ="<<hash.ToString()<<std::endl; 
	return hash;	
}

std::string CBlockHeader::ToString() const                                                                                                                                                                                                                                   
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=%d, hashPrevBlock=%s, hashMerkleRoot=%s, hashClaimTrie=%s, nTime=%u, nBits=%08x, nNonce=%s)\n",
		GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        hashClaimTrie.ToString(),
        nTime, nBits, nNonce.ToString());

    return s.str();
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=%d, hashPrevBlock=%s, hashMerkleRoot=%s, hashClaimTrie=%s, nTime=%u, nBits=%08x, nNonce=%s, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        hashClaimTrie.ToString(),
        nTime, nBits, nNonce.ToString(),
        vtx.size());
    for (unsigned int i = 0; i < vtx.size(); i++)
    {
        s << "  " << vtx[i].ToString() << "\n";
    }
    return s.str();
}
