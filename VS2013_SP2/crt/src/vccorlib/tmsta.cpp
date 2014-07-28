//
// Copyright (C) Microsoft Corporation
// All rights reserved.
//

//Define STA thread
int __abi___threading_model = 1;

// Compiler is pulling in the following symbol:
// INCLUDE:___refSTAThread (decorated x86) to enable STA
extern "C" {
	int __refSTAThread = 0;
}