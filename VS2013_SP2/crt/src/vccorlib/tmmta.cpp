//
// Copyright (C) Microsoft Corporation
// All rights reserved.
//

//Define MTA thread
int __abi___threading_model = 2;

// Compiler is pulling in the following symbol:
// INCLUDE:___refMTAThread (decorated x86) to enable MTA
extern "C" {
	int __refMTAThread = 0;
}