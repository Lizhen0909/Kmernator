//
// Kmernator/apps/ContigExtender.cpp
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
#include "ReadSet.h"
#include "Options.h"
#include "Kmer.h"
#include "KmerSpectrum.h"
#include "DuplicateFragmentFilter.h"
#include "ContigExtender.h"
#include "Log.h"

using namespace std;
typedef TrackingDataMinimal4f DataType;
typedef KmerSpectrum<DataType, DataType> KS;


int main(int argc, char *argv[]) {
	// do not apply artifact filtering by default
	Options::getSkipArtifactFilter() = 1;
	// override the default output format!
	Options::getFormatOutput() = 3;

	if (!ContigExtenderOptions::parseOpts(argc, argv))
		throw std::invalid_argument("Please fix the command line arguments");

	Options::FileListType inputFiles = Options::getInputFiles();
	Options::FileListType contigFiles;
	contigFiles.push_back(ContigExtenderOptions::getContigFile());

	ReadSet reads;
	LOG_VERBOSE(1, "Reading Input Files" );
	reads.appendAllFiles(inputFiles);

	LOG_VERBOSE(1, "loaded " << reads.getSize() << " Reads, " << reads.getBaseCount() << " Bases ");

	ReadSet contigs;
	LOG_VERBOSE(1, "Reading Contig File" );
	contigs.appendAllFiles(contigFiles);
	LOG_VERBOSE(1, "loaded " << contigs.getSize() << " Reads, " << contigs.getBaseCount() << " Bases ");

	if (Options::getDeDupMode() > 0 && Options::getDeDupEditDistance() >= 0) {
	  LOG_VERBOSE(2, "Applying DuplicateFragmentPair Filter to Input Files");
	  unsigned long duplicateFragments = DuplicateFragmentFilter::filterDuplicateFragments(reads);
	  LOG_VERBOSE(1, "filter removed duplicate fragment pair reads: " << duplicateFragments);
	  LOG_DEBUG(1, MemoryUtils::getMemoryUsage());
	}

	ReadSet newContigs = ContigExtender<KS>::extendContigs(contigs, reads);

	string outputFilename = Options::getOutputFile();
	OfstreamMap ofmap(outputFilename,"");
	Options::getFormatOutput() = FormatOutput::FASTA_UNMASKED;
	if (!outputFilename.empty()) {
		for(unsigned long i = 0; i < newContigs.getSize(); i++)
			newContigs.getRead(i).write(ofmap.getOfstream(""));
	}

	LOG_VERBOSE(1, "Finished");
}
