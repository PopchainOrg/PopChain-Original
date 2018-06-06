// Copyright (c) 2017-2018 The Popchain Core Developers

//
// Unit tests for block-chain checkpoints
//

#include "checkpoints.h"

#include "uint256.h"
#include "test/test_pop.h"
#include "chainparams.h"

#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_FIXTURE_TEST_SUITE(Checkpoints_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(sanity)
{
    const CCheckpointData& checkpoints = Params(CBaseChainParams::MAIN).Checkpoints();
    BOOST_CHECK(Checkpoints::GetTotalBlocksEstimate(checkpoints) >= 0);
}

BOOST_AUTO_TEST_SUITE_END()
