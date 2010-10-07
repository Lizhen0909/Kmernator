/*
 * FilterReads.h
 *
 *  Created on: Oct 6, 2010
 *      Author: regan
 */

#ifndef FILTERREADS_H_
#define FILTERREADS_H_


#include <iostream>
#include <cstdlib>
#include <cstring>

#include "config.h"
#include "Options.h"
#include "ReadSet.h"
#include "FilterKnownOddities.h"
#include "KmerSpectrum.h"
#include "ReadSelector.h"
#include "Utils.h"
#include "Log.h"

#include <boost/lexical_cast.hpp>

using namespace std;

typedef TrackingDataMinimal4f DataType;
typedef KmerSpectrum<DataType, DataType> KS;
typedef ReadSelector<DataType> RS;

// TODO add outputformat of fasta
class FilterReadsOptions : Options {
public:
	static int getMaxKmerDepth() {
		return getVarMap()["max-kmer-output-depth"].as<int> ();
	}
	static int getPartitionByDepth() {
		return getVarMap()["partition-by-depth"].as<int> ();
	}
	static bool getBothPairs() {
		return getVarMap()["min-passing-in-pair"].as<int>() == 2;
	}
	static bool parseOpts(int argc, char *argv[]) {
		// set options specific to this program
		getPosDesc().add("kmer-size", 1);
		getPosDesc().add("input-file", -1);

		getDesc().add_options()

		("max-kmer-output-depth", po::value<int>()->default_value(-1),
				"maximum number of times a kmer will be output among the selected reads (mutually exclusive with partition-by-depth).  This is not a criteria on the kmer spectrum, just a way to reduce the redundancy of the output")

		("partition-by-depth", po::value<int>()->default_value(-1),
				"partition filtered reads by powers-of-two coverage depth (mutually exclusive with max-kmer-depth)")

		("min-passing-in-pair", po::value<int>()->default_value(1),
				"1 or 2 reads in a pair must pass filters");

		bool ret = Options::parseOpts(argc, argv);

		if (ret) {
			// verify mutually exclusive options are not set
			if ( (getMaxKmerDepth() > 0 && getPartitionByDepth() >  0) )
			{
				throw std::invalid_argument("You can not specify both max-kmer-depth and partition-by-depth");
			}
			if (Options::getOutputFile().empty())
			{
				LOG_WARN(1, "no output file specified... This is a dry run!");
			}
		}
		return ret;
	}
};

long selectReads(unsigned int minDepth, ReadSet &reads, KS &spectrum, std::string outputFilename)
{
	LOG_VERBOSE(1, "Trimming reads: ");
	RS selector(reads, spectrum.weak, minDepth);
	LOG_VERBOSE(1, MemoryUtils::getMemoryUsage());
	LOG_VERBOSE(1, "Picking reads: ");

	long oldPicked = 0;
	long picked = 0;

	int maximumKmerDepth = FilterReadsOptions::getMaxKmerDepth();

	OfstreamMap ofmap(outputFilename, ".fastq");

	if (maximumKmerDepth > 0) {
		for (int depth = 1; depth < maximumKmerDepth; depth++) {
			LOG_VERBOSE(1, "Picking depth " << depth << " layer of reads");
			if (reads.hasPairs())
				picked += selector.pickBestCoveringSubsetPairs(depth,
						minDepth, Options::getMinReadLength(), FilterReadsOptions::getBothPairs());
			else
				picked += selector.pickBestCoveringSubsetReads(depth,
						minDepth, Options::getMinReadLength());
			LOG_VERBOSE(1, MemoryUtils::getMemoryUsage());
		}

		if (picked > 0 && !outputFilename.empty()) {
			LOG_VERBOSE(1, "Writing " << picked << " reads to output file(s)");
			selector.writePicks(ofmap, oldPicked);
		}
		LOG_VERBOSE(1, MemoryUtils::getMemoryUsage());
		oldPicked += picked;


	} else {

		int maxDepth = FilterReadsOptions::getPartitionByDepth();
		if (maxDepth < 0) {
			maxDepth = 1;
		}

		for (unsigned int depth = maxDepth; depth >= 1; depth /= 2) {

			string ofname = outputFilename;
			if (maxDepth > 1) {
				ofname += "-PartitionDepth" + boost::lexical_cast< string >( depth );
			}
			ofmap = OfstreamMap(ofname, ".fastq");
			float tmpMinDepth = std::max(minDepth, depth);
			if (Options::getKmerSize() == 0) {
				tmpMinDepth = 0;
				depth = 0;
			}
			LOG_VERBOSE(1, "Selecting reads over depth: " << depth << " (" << tmpMinDepth << ") ");

			if (reads.hasPairs()) {
				picked = selector.pickAllPassingPairs(tmpMinDepth,
						Options::getMinReadLength(),
						FilterReadsOptions::getBothPairs());
			} else {
				picked = selector.pickAllPassingReads(tmpMinDepth,
						Options::getMinReadLength());
			}
			LOG_VERBOSE(1, "At or above coverage: " << depth << " Picked " << picked
			<< " / " << reads.getSize() << " reads");
			LOG_VERBOSE(1, MemoryUtils::getMemoryUsage());

			if (picked > 0 && !outputFilename.empty()) {
				LOG_VERBOSE(1, "Writing " << picked << " reads  to output files");
				selector.writePicks(ofmap, oldPicked);
			}
			oldPicked += picked;

			if (minDepth > depth) {
				break;
			}
		}
	}
	ofmap.clear();
	LOG_VERBOSE(1, "Done.  Cleaning up. " << MemoryUtils::getMemoryUsage());
	selector.clear();

	return oldPicked;
}


#endif /* FILTERREADS_H_ */
