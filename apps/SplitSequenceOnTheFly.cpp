//
// Kmernator/apps/FixPair.cpp
//
// Author: Rob Egan
//
// Copyright 2010 The Regents of the University of California.
// All rights reserved.
//
// The United States Government has rights in this work pursuant
// to contracts DE-AC03-76SF00098, W-7405-ENG-36 and/or
// W-7405-ENG-48 between the United States Department of Energy
// and the University of California.
//
// Redistribution and use in source and binary forms are permitted
// provided that: (1) source distributions retain this entire
// copyright notice and comment, and (2) distributions including
// binaries display the following acknowledgement:  "This product
// includes software developed by the University of California,
// JGI-PSF and its contributors" in the documentation or other
// materials provided with the distribution and in all advertising
// materials mentioning features or use of this software.  Neither the
// name of the University nor the names of its contributors may be
// used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE.
//

#include <iostream>
#include <cstdlib>
#include <cstring>

#include "config.h"
#include "Sequence.h"
#include "ReadSet.h"
#include "Options.h"
#include "Log.h"

using namespace std;

class SSOptions : public Options {
public:
	static int getNumFiles() {
		return getVarMap()["num-files"].as<int> ();
	}
	static int getFileNum() {
		return getVarMap()["file-num"].as<int> ();
	}
	static bool parseOpts(int argc, char *argv[]) {
		// set options specific to this program
		getPosDesc().add("input-file", -1);
		getDesc().add_options()("help", "produce help message")
				("num-files", po::value<int>(), "The number of files to split into N")
				("file-num",  po::value<int>(), "The number of the file to ouput (0-(N-1))");


		bool ret = Options::parseOpts(argc, argv);
		po::variables_map & vm = getVarMap();
		if (getInputFiles().empty() || vm.count("num-files") == 0 || vm.count("file-num") == 0) {
			ret = false;
			LOG_ERROR(1, "Please specify num-files, file-num and at least one input file");
		}
		return ret;
	}
};


int main(int argc, char *argv[]) {
	Options::getVerbosity() = 0;
	if (!SSOptions::parseOpts(argc, argv))
		throw std::invalid_argument("Please fix the command line arguments");

	Options::FileListType inputs = Options::getInputFiles();

	ReadSet reads;
	LOG_VERBOSE(1, "Reading Input Files");
	reads.appendAllFiles(inputs, SSOptions::getFileNum(), SSOptions::getNumFiles());
	LOG_VERBOSE(1,"loaded " << reads.getSize() << " Reads, " << reads.getBaseCount() << " Bases ");

	for(ReadSet::ReadSetSizeType readIdx = 0 ; readIdx < reads.getSize(); readIdx++) {
		const Read &read = reads.getRead(readIdx);
		read.write(std::cout);
	}

}

// $Log: FixPair.cpp,v $
// Revision 1.4  2010-05-18 20:50:18  regan
// merged changes from PerformanceTuning-20100506
//
// Revision 1.3.2.1  2010-05-07 22:59:29  regan
// refactored base type declarations
//
// Revision 1.3  2010-05-06 21:46:57  regan
// merged changes from PerformanceTuning-20100501
//
//
