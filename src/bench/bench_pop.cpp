// Copyright (c) 2017-2018 The Popchain Core Developers

#include "bench.h"

#include "key.h"
#include "main.h"
#include "util.h"

int
main(int argc, char** argv)
{
    ECC_Start();
    SetupEnvironment();
    fPrintToDebugLog = false; // don't want to write to debug.log file

    benchmark::BenchRunner::RunAll();

    ECC_Stop();
}
