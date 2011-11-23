//
// DistributedNucleatingAssembler.cpp
// Author: Rob Egan
//
// Copyright 2011 The Regents of the University of California.
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
// binaries display the following acknowledgment:  "This product
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

#include "config.h"
#include "ReadSet.h"
#include "Options.h"
#include "Kmer.h"
#include "KmerSpectrum.h"
#include "DuplicateFragmentFilter.h"
#include "ContigExtender.h"
#include "DistributedFunctions.h"
#include "MemoryUtils.h"
#include "Utils.h"
#include "Log.h"
#include "Vmatch.h"
#include "Cap3.h"
#include "KmerMatch.h"
#include "MatcherInterface.h"

using namespace std;
typedef TrackingDataMinimal4f DataType;
typedef KmerSpectrum<DataType, DataType> KS;

typedef KmerSpectrum< TrackingDataWithAllReads, TrackingDataWithAllReads > KS2;


class _DistributedNucleatingAssemblerOptions: public _ContigExtenderBaseOptions, public _Cap3Options, public _VmatchOptions, public _KmerMatchOptions {
public:
	static int getMaxIterations() {
		return getVarMap()["max-iterations"].as<int> ();
	}
	static int getMaxContigLength() {
		return getVarMap()["max-contig-length"].as<int>();
	}
	void _resetDefaults() {
		_Cap3Options::_resetDefaults();
		_ContigExtenderBaseOptions::_resetDefaults();
		_VmatchOptions::_resetDefaults();
		_KmerMatchOptions::_resetDefaults();

		GeneralOptions::_resetDefaults();
		GeneralOptions::getOptions().getSkipArtifactFilter() = 1;
		// override the default output format!
		GeneralOptions::getOptions().getFormatOutput() = 3;
		GeneralOptions::getOptions().getMmapInput() = 0;
		GeneralOptions::getOptions().getVerbose() = 1;
		GeneralOptions::getOptions().getMaxThreads() = 1;
	}
	void _setOptions(po::options_description &desc,
			po::positional_options_description &p) {

		po::options_description opts("Distributed Nucleating Assembly Options");

		p.add("input-file", -1);

		opts.add_options()

		("max-iterations", po::value<int>()->default_value(1000),
				"the maximum number of rounds to extend the set of contigs")
		("max-contig-length", po::value<int>()->default_value(3000),
				"the maximum size of a contig to continue extending")
		;
		desc.add(opts);

		_KmerMatchOptions::_setOptions(desc,p);
		_VmatchOptions::_setOptions(desc,p);
		_ContigExtenderBaseOptions::_setOptions(desc,p);
		_Cap3Options::_setOptions(desc,p);
		GeneralOptions::_setOptions(desc,p);

	};
	bool _parseOptions(po::variables_map &vm) {

		bool ret = true;
		ret &= GeneralOptions::_parseOptions(vm);
		ret &= _KmerMatchOptions::_parseOptions(vm);
		ret &= _VmatchOptions::_parseOptions(vm);
		ret &= _ContigExtenderBaseOptions::_parseOptions(vm);
		ret &= _Cap3Options::_parseOptions(vm);

		if (Options::getOptions().getOutputFile().empty()) {
			LOG_ERROR(1, "You must specify an --output");
			ret = false;
		}

		return ret;
	}
};
typedef OptionsBaseTemplate<_DistributedNucleatingAssemblerOptions>
		DistributedNucleatingAssemblerOptions;


std::string extendContigsWithCap3(ReadSet & contigs,
		ReadSet::ReadSetVector &contigReadSet, ReadSet & changedContigs,
		ReadSet & finalContigs, ReadSet::ReadSetSizeType minimumCoverage) {
	std::stringstream extendLog;
	std::string cap3Path = DistributedNucleatingAssemblerOptions::getOptions().getCap3Path();
	int maxReads = DistributedNucleatingAssemblerOptions::getOptions().getCap3MaxReads();

	#pragma omp parallel for
	for (ReadSet::ReadSetSizeType i = 0; i < contigs.getSize(); i++) {
		const Read &oldRead = contigs.getRead(i);
		Read newRead = oldRead;
		SequenceLengthType oldLen = oldRead.getLength(), newLen = 0;

		ReadSet::ReadSetSizeType poolSize = contigReadSet[i].getSize();

		if (poolSize > minimumCoverage) {
			LOG_VERBOSE_OPTIONAL(2, true, "Extending " << oldRead.getName() << " with " << poolSize << " pool of reads");

			ReadSet sampledSet = contigReadSet[i].randomlySample(maxReads);
			newRead = Cap3::extendContig(oldRead, sampledSet);
			newLen = newRead.getLength();
		}
		long deltaLen = (long)newLen - (long)oldLen;
		if (deltaLen > 0) {
			extendLog << std::endl << "Cap3 Extended " << oldRead.getName() << " "
					<< deltaLen << " bases to " << newRead.getLength() << ": "
					<< newRead.getName() << " with " << poolSize
					<< " reads in the pool";
			//#pragma omp critical
			changedContigs.append(newRead);
		} else {
			extendLog << std::endl << "Did not extend " << oldRead.getName()
					<< " with " << poolSize << " reads in the pool";
			//#pragma omp critical
			finalContigs.append(oldRead);
		}
	}


	return extendLog.str();
}
std::string extendContigsWithContigExtender(ReadSet & contigs,
		ReadSet::ReadSetVector &contigReadSet, ReadSet & changedContigs,
		ReadSet & finalContigs, SequenceLengthType minKmerSize,
		double minimumCoverage, SequenceLengthType maxKmerSize,
		SequenceLengthType maxExtend, SequenceLengthType kmerStep) {

	std::stringstream extendLog;
	//#pragma omp parallel for
	for (ReadSet::ReadSetSizeType i = 0; i < contigs.getSize(); i++) {
		const Read &oldRead = contigs.getRead(i);
		Read newRead;
		SequenceLengthType oldLen = oldRead.getLength(), newLen = 0;
		ReadSet::ReadSetSizeType poolSize = contigReadSet[i].getSize();
		SequenceLengthType myKmerSize = minKmerSize;
		if (poolSize > minimumCoverage) {
			LOG_VERBOSE_OPTIONAL(2, true, "kmer-Extending " << oldRead.getName() << " with " << poolSize << " pool of reads");
			ReadSet myContig;
			myContig.append(oldRead);
			ReadSet newContig;

			while (newLen <= oldLen && myKmerSize <= maxKmerSize) {
				newContig = ContigExtender<KS>::extendContigs(myContig,
						contigReadSet[i], maxExtend, myKmerSize, myKmerSize);
				newLen = newContig.getRead(0).getLength();
				myKmerSize += kmerStep;
			}
			newRead = newContig.getRead(0);
		} else {
			newRead = oldRead;
		}
		long deltaLen = (long) newLen - (long) oldLen;
		if (deltaLen > 0) {
			extendLog << std::endl << "Kmer Extended " << oldRead.getName() << " "
					<< deltaLen << " bases to " << newRead.getLength() << ": "
					<< newRead.getName() << " with " << poolSize
					<< " reads in the pool K " << (myKmerSize - kmerStep);
			//#pragma omp critical
			changedContigs.append(newRead);
		} else {
			extendLog << std::endl << "Did not extend " << oldRead.getName()
					<< " with " << poolSize << " reads in the pool";
			//#pragma omp critical
			finalContigs.append(oldRead);
		}
	}
	return extendLog.str();
}

void finishLongContigs(long maxContigLength, ReadSet &changedContigs, ReadSet &finalContigs) {
	ReadSet keepContigs;
	for(long i = 0; i < (long) changedContigs.getSize(); i++) {
		const Read &read = changedContigs.getRead(i);
		if ((long) read.getLength() >= maxContigLength)
			finalContigs.append(read);
		else
			keepContigs.append(read);
	}
	changedContigs.swap(keepContigs);
}

int main(int argc, char *argv[]) {
	// do not apply artifact filtering by default
	double timing1, timing2;

	mpi::communicator world = initializeWorldAndOptions< DistributedNucleatingAssemblerOptions >(argc, argv);

	timing1 = MPI_Wtime();

	OptionsBaseInterface::FileListType inputFiles =
			Options::getOptions().getInputFiles();
	std::string contigFile =
			DistributedNucleatingAssemblerOptions::getOptions().getContigFile();
	std::string finalContigFile;
	double minimumCoverage =
			DistributedNucleatingAssemblerOptions::getOptions().getMinimumCoverage();
	long maxIterations =
					DistributedNucleatingAssemblerOptions::getOptions().getMaxIterations();

	ReadSet reads;
	LOG_VERBOSE_OPTIONAL(1, world.rank() == 0, "Reading Input Files" );
	reads.appendAllFiles(inputFiles, world.rank(), world.size());

	setGlobalReadSetOffsets(world, reads);
	timing2 = MPI_Wtime();

	LOG_VERBOSE_OPTIONAL(1, world.rank() == 1, "loaded " << reads.getGlobalSize() << " Reads in " << (timing2-timing1) << " seconds" );

	boost::shared_ptr< MatcherInterface > matcher;
	if (KmerBaseOptions::getOptions().getKmerSize() == 0) {
		matcher.reset( new Vmatch(world, UniqueName::generateHashName(inputFiles), reads) );
	} else {
		matcher.reset( new KmerMatch(world, reads) );
	}

	SequenceLengthType minKmerSize, maxKmerSize, kmerStep, maxExtend;
	ContigExtender<KS>::getMinMaxKmerSize(reads, minKmerSize, maxKmerSize,
			kmerStep);
	maxKmerSize = boost::mpi::all_reduce(world, maxKmerSize, mpi::minimum<
			SequenceLengthType>());
	LOG_VERBOSE(1, "Kmer size ranges: " << minKmerSize << "\t" << maxKmerSize << "\t" << kmerStep);
	maxExtend = maxKmerSize;

	timing1 = timing2;
	timing2 = MPI_Wtime();
	LOG_VERBOSE_OPTIONAL(1, world.rank() == 0, "Prepared vmatch indexes in " << (timing2-timing1) << " seconds");

	ReadSet finalContigs;
	ReadSet contigs;
	contigs.appendFastaFile(contigFile, world.rank(), world.size());

	short iteration = 0;
	while (++iteration <= maxIterations) {

		matcher->resetTimes(MPI_Wtime());

		setGlobalReadSetOffsets(world, contigs);

		LOG_VERBOSE_OPTIONAL(1, world.rank() == 0, "Iteration: " << iteration << ". Contig File: " << contigFile << ". contains " << contigs.getGlobalSize() << " Reads");
		if (contigs.getGlobalSize() == 0) {
			LOG_VERBOSE_OPTIONAL(1, true, "There are no contigs to extend in " << contigFile);
			break;
		}

		MatcherInterface::MatchReadResults contigReadSet = matcher->match(contigs, contigFile);

		ReadSet changedContigs;
		std::string extendLog;

		if (!DistributedNucleatingAssemblerOptions::getOptions().getCap3Path().empty()) {
			extendLog = extendContigsWithCap3(contigs, contigReadSet, changedContigs, finalContigs, minimumCoverage);
		} else {
			extendLog = extendContigsWithContigExtender(contigs, contigReadSet,
				changedContigs, finalContigs,
				minKmerSize, minimumCoverage, maxKmerSize, maxExtend, kmerStep);
		}

		matcher->recordTime("extendContigs", MPI_Wtime());
		LOG_VERBOSE(1, (extendLog));

		finishLongContigs(DistributedNucleatingAssemblerOptions::getOptions().getMaxContigLength(), changedContigs, finalContigs);

		LOG_DEBUG(1, "Changed contigs: " << changedContigs.getSize() << " finalContigs: " << finalContigs.getSize());
		setGlobalReadSetOffsets(world, changedContigs);
		setGlobalReadSetOffsets(world, finalContigs);
		LOG_VERBOSE_OPTIONAL(1, world.rank() == 0, "Changed contigs: " << changedContigs.getGlobalSize() << " finalContigs: " << finalContigs.getGlobalSize());

		std::string oldFinalContigFile = finalContigFile;
		std::string oldContigFile = contigFile;
		{
			// write out the state of the contig files (so far) so we do not loose them
			DistributedOfstreamMap om(world,
					Options::getOptions().getOutputFile(), "");
			om.setBuildInMemory();
			if (finalContigs.getGlobalSize() > 0) {
				std::string fileKey = "final-" + boost::lexical_cast<
						std::string>(iteration);
				finalContigs.writeAll(om.getOfstream(fileKey),
						FormatOutput::Fasta());
				finalContigFile = om.getRealFilePath(fileKey);
			}
			if (changedContigs.getGlobalSize() > 0) {
				std::string filekey = "-inputcontigs-" + boost::lexical_cast<
						std::string>(iteration) + ".fasta";
				changedContigs.writeAll(om.getOfstream(filekey),
						FormatOutput::Fasta());
				contigFile = om.getRealFilePath(filekey);
			}
			contigs = changedContigs;
		}

		matcher->recordTime("writeFinalTime", MPI_Wtime());

		if (!Log::isDebug(1) && world.rank() == 0) {
			// remove most recent contig files (if not debugging)
			if (!oldFinalContigFile.empty()) {
				LOG_VERBOSE_OPTIONAL(1, true, "Removing " << oldFinalContigFile);
				unlink(oldFinalContigFile.c_str());
			}

			if (DistributedNucleatingAssemblerOptions::getOptions().getContigFile().compare(
					oldContigFile) != 0) {
				LOG_VERBOSE_OPTIONAL(1, true, "Removing " << oldContigFile);
				unlink(oldContigFile.c_str());
			}
		}

		if (changedContigs.getGlobalSize() == 0) {
			LOG_VERBOSE_OPTIONAL(1, world.rank() == 1, "No more contigs to extend " << changedContigs.getSize());
			break;
		}

		matcher->recordTime("finishIteration", MPI_Wtime());
		if (Log::isVerbose(1)) {
			Log::Verbose(matcher->getTimes("") + MemoryUtils::getMemoryUsage(), false);
		}
	}

	if (world.rank() == 0 && !Log::isDebug(1)) {
		if (DistributedNucleatingAssemblerOptions::getOptions().getContigFile().compare(
				contigFile) != 0) {
			LOG_DEBUG_OPTIONAL(1, true, "Removing " << contigFile);
			unlink(contigFile.c_str());
		}
	}

	{
		// write final contigs (and any unfinished contigs still remaining)
		std::string tmpFinalFile = finalContigFile;
		DistributedOfstreamMap om(world, Options::getOptions().getOutputFile(),
				"");
		om.setBuildInMemory();
		finalContigs.append(contigs);
		std::string fileKey = "";
		finalContigs.writeAll(om.getOfstream(fileKey), FormatOutput::Fasta());
		om.clear();

		if (world.rank() == 0 && !finalContigFile.empty()) {
			LOG_DEBUG_OPTIONAL(1, true, "Removing " << finalContigFile);
			unlink(finalContigFile.c_str());
		}
		finalContigFile = om.getRealFilePath(fileKey);
	}
	LOG_VERBOSE_OPTIONAL(1, world.rank() == 0, "Final contigs are in: " << finalContigFile);

	LOG_VERBOSE_OPTIONAL(1, world.rank() == 0, "Finished");
	MPI_Finalize();

	return 0;
}

