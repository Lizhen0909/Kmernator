//
// Kmernator/apps/RandomlySample.cpp
//
// Author: Rob Egan
//
/*****************

Kmernator Copyright (c) 2012, The Regents of the University of California,
through Lawrence Berkeley National Laboratory (subject to receipt of any
required approvals from the U.S. Dept. of Energy).  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

(1) Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

(2) Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

(3) Neither the name of the University of California, Lawrence Berkeley
National Laboratory, U.S. Dept. of Energy nor the names of its contributors may
be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

You are under no obligation whatsoever to provide any bug fixes, patches, or
upgrades to the features, functionality or performance of the source code
("Enhancements") to anyone; however, if you choose to make your Enhancements
available either publicly, or directly to Lawrence Berkeley National
Laboratory, without imposing a separate written license agreement for such
Enhancements, then you hereby grant the following license: a  non-exclusive,
royalty-free perpetual license to install, use, modify, prepare derivative
works, incorporate into other computer software, distribute, and sublicense
such enhancements or derivative works thereof, in binary and source code form.

*****************/

#undef _USE_OPENMP

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include "config.h"
#include "Sequence.h"
#include "ReadSet.h"
#include "Options.h"
#include "Utils.h"
#include "Log.h"

using namespace std;

class _RSOptions : public OptionsBaseInterface {
public:
	_RSOptions() : byPair(true), numSamples(1000), minBytesPerRecord(2000), maxPercentForFseek(15) {}
	bool &getByPair() {
		return byPair;
	}
	int &getNumSamples() {
		return numSamples;
	}
	int &getMinBytesPerRecord() {
		return minBytesPerRecord;
	}
	int &getMaxPercentForFseek() {
		return maxPercentForFseek;
	}
	void _resetDefaults() {
		GeneralOptions::_resetDefaults();
		GeneralOptions::getOptions().getVerbose() = 0;
		GeneralOptions::getOptions().getMmapInput() = false;
	}
	void _setOptions(po::options_description &desc, po::positional_options_description &p) {
		p.add("input-file", -1);
		po::options_description opts("RandomlySample <options> [input-file]\n\tNote: --input-file can either be specified as a positional argument or within the options\n\nRandomly Sample Options");
		opts.add_options()
								("by-pair", po::value<bool>()->default_value(byPair), "If set, pairs are sampled, if not set, reads are sampled")
								("num-samples",  po::value<int>()->default_value(numSamples), "The number of samples to output")
								("min-bytes-per-record", po::value<int>()->default_value(minBytesPerRecord), "The minimum number of bytes between two records (should be >2x greatest record size)")
								("max-percent-for-fseek", po::value<int>()->default_value(maxPercentForFseek), "The estimated maximum % of reads to select by fseek instead of blocked reads");
		desc.add(opts);
		GeneralOptions::_setOptions(desc, p);
	}
	bool _parseOptions(po::variables_map &vm) {

		bool ret = GeneralOptions::_parseOptions(vm);

		setOpt("by-pair", byPair);
		setOpt("num-samples", numSamples);
		setOpt("min-bytes-per-record", minBytesPerRecord);
		setOpt("max-percent-for-fseek", maxPercentForFseek);

		if (Options::getOptions().getInputFiles().empty() || Options::getOptions().getInputFiles().size() > 1) {
			ret = false;
			LOG_ERROR(1, "Please specify at a single input file");
		}
		return ret;
	}
private:
	bool byPair;
	int numSamples, minBytesPerRecord, maxPercentForFseek;
};
typedef OptionsBaseTemplate< _RSOptions > RSOptions;

typedef std::vector<unsigned long> Positions;
Positions selectRandomDense(unsigned long numSamples, unsigned long limit) {

	LongRand randomGenerator;

	if (numSamples > limit) {
		LOG_WARN(1, "Requesting more samples than available.  requested samples: " << numSamples << " available: " << limit);
		Positions positions;
		positions.reserve(limit);
		for(unsigned long i = 0; i < limit; i++)
			positions.push_back(i);
		return positions;
	}
	std::set<unsigned long> positions;
	if (numSamples * 2 > limit) {
		// select positions to remove
		for(unsigned long i = 0; i < limit ; i++) {
			positions.insert(i);
		}
		while (positions.size() > numSamples) {
			positions.erase( (unsigned long) (randomGenerator.getRand() % limit) );
		}

	} else {
		// select positions to add

		while (positions.size() < numSamples) {
			positions.insert( (unsigned long) (randomGenerator.getRand() % limit) );
		}
	}
	Positions list(positions.begin(), positions.end());
	return list;
}

Positions selectRandom(unsigned long numSamples, unsigned long limit, unsigned long minSpacing, unsigned long maxEdgeSpacing) {
	if (minSpacing == 1 && numSamples > limit / 2) {
		return selectRandomDense(numSamples, limit);
	}
	LOG_DEBUG(1, "selectRandom(" << numSamples << ", " << limit << ", " << minSpacing << ", " << maxEdgeSpacing << ")");
	Positions positions;
	positions.reserve(numSamples);
	LongRand randomGenerator;
	unsigned long attempts = 0;
	unsigned long maxAttempts =  (log(numSamples)/log(2)+3)*2;
	while (positions.size() < numSamples && attempts++ < maxAttempts) {
		long newSamples = numSamples - positions.size();
		for(long i = 0; i < newSamples; i++)
			positions.push_back( randomGenerator.getRand() % limit );
		std::sort(positions.begin(), positions.end());
		unsigned long lastPos = positions[0];
		std::vector<long> deleteThese;
		for(long i = 1 ; i < (long) positions.size(); i++) {
			if (positions[i] < lastPos + minSpacing || positions[i] >= limit - (maxEdgeSpacing)) {
				deleteThese.push_back(i);
			} else {
				lastPos = positions[i];
			}
		}
		for(long i = deleteThese.size() - 1 ; i >= 0; i--) {
			std::swap(positions[deleteThese[i]], positions.back());
			LOG_DEBUG(2, "attempt " << attempts << " size " << positions.size() << " removing " << positions.back() << " from " << deleteThese[i]);
			positions.pop_back();
		}
		LOG_DEBUG(1, "Selected " <<  positions.size() << " after removing " << deleteThese.size() << " attempt " << attempts << " of " << maxAttempts);
	}
	std::sort(positions.begin(), positions.end());

	if (positions.size() < numSamples) {
		LOG_WARN(1, "Could not find " << numSamples << ", attempting only " << positions.size());
	}
	return positions;
}

long pickByBlock(ReadFileReader &rfr, long numSamples) {
	bool byPair = RSOptions::getOptions().getByPair();
	long numBlocks = std::min((long) 100, numSamples / 5);
	if (numBlocks < 1)
		numBlocks = 1;
	long numPicksPerBlock = ((numSamples+numBlocks-1) / numBlocks);
	long count = 0;

	LOG_DEBUG(1, "pickByBlock: " << numSamples << " blocks: " << numBlocks << " picksPerBlock: " << numPicksPerBlock);

	OfstreamMap *ofm = NULL;
	if (!Options::getOptions().getOutputFile().empty()) {
		ofm = new OfstreamMap(Options::getOptions().getOutputFile(), "");
	}
	ostream &output = (ofm == NULL ? std::cout : ofm->getOfstream(""));

	for(long block = 0; block < numBlocks; block++) {
		if (numSamples <= 0)
			break;

		ReadSet reads;
		LOG_DEBUG(1, "Reading block " << block << " of " << numBlocks);
		reads.appendFasta(rfr, block, numBlocks);
		if (byPair)
			reads.identifyPairs();
		long numRead = byPair ? reads.getPairSize() : reads.getSize();
                numPicksPerBlock = ( (numSamples+numBlocks-block-1) / (numBlocks - block));
		long numPicks = std::min(numPicksPerBlock, numSamples);
		numPicks = std::min(numRead, numPicks);
		LOG_DEBUG(1, "Read " << numRead << (byPair?" pairs" : " reads") << " selecting " << numPicks << " remaining: " << numSamples);
		Positions positions = selectRandom(numPicks, numRead, 1, 0);
		// write
		LOG_DEBUG(1, "Writing " << positions.size());
		for(Positions::iterator it = positions.begin(); it != positions.end(); it++) {
			if (byPair)
				reads.write(output, reads.getPair(*it));
			else
				reads.write(output, *it);
		}
		count += positions.size();
		numSamples -= positions.size();
	}
	if (ofm != NULL)
		delete ofm;

	return count;
}
long pickBySeeks(ReadFileReader &rfr, unsigned long numSamples, unsigned long minBytes) {

	LOG_DEBUG(1, "pickBySeeks: " << numSamples << ", " << minBytes);
	unsigned long fileSize = rfr.getFileSize();
	Positions positions = selectRandom(numSamples, fileSize, minBytes, minBytes * 10);

	if (Log::isDebug(1)) {
		std::stringstream ss;
		for(long i = 0 ; i < (long) positions.size(); i++)
			ss << "\t" << positions[i];
		std::string s = ss.str();
		LOG_DEBUG(2, "Picked positions(" << positions.size() << "):" << s);
	}

	bool byPair = RSOptions::getOptions().getByPair();

	if (rfr.seekToNextRecord(0, true)) {
		LOG_DEBUG(2, "Reading first two records to determine inherent pairing");
		std::string name1, name2, bases1, bases2, quals1, quals2, comment1, comment2;
		rfr.nextRead(name1, bases1, quals1, comment1);
		rfr.nextRead(name2, bases2, quals2, comment2);

		bool isFilePaired= ReadSet::isPair(name1, name2, comment1, comment2);
		LOG_DEBUG(1, "Reading first two records to determine inherent pairing: " << isFilePaired << " " << name1 << " " << name2);
		byPair &= isFilePaired;
	}	

	LOG_DEBUG(1, "detecting by pair: " << byPair);
	OfstreamMap *ofm = NULL;
	if (!Options::getOptions().getOutputFile().empty()) {
		ofm = new OfstreamMap(Options::getOptions().getOutputFile(), "");
	}
	ostream &output = (ofm == NULL ? std::cout : ofm->getOfstream(""));

	long count = 0;
	unsigned long lastPos = fileSize;
	for(long i = 0; i < (long) positions.size(); i++) {
		if (rfr.eof())
			break;
		rfr.seekToNextRecord(positions[i], byPair);
		unsigned long myPos = rfr.getPos();
		while (lastPos < fileSize && lastPos >= myPos) {
			LOG_DEBUG(2, "Re-seeking from " << myPos << " because lastPos is larger " << lastPos);
			rfr.seekToNextRecord(myPos + 1, byPair);
			if (rfr.eof())
				break;
			myPos = rfr.getPos();
		}
		if (rfr.eof())
			break;
		lastPos = myPos;
		std::string name, bases, quals, comment;
		rfr.nextRead(name, bases, quals, comment);
		Read read(name, bases, quals, comment);
		read.write(output);
		if (byPair) {
			rfr.nextRead(name, bases, quals, comment);
			Read read2(name, bases, quals, comment);
			read2.write(output);
		}
		count++;
	}
	if (ofm != NULL)
		delete ofm;

	return count;
}

int main(int argc, char *argv[]) {

	if (!RSOptions::parseOpts(argc, argv)) exit(1);

	Cleanup::prepare();

	std::srand(static_cast<unsigned>(std::time(0)));
	OptionsBaseInterface::FileListType inputs = Options::getOptions().getInputFiles();
	std::string file = inputs[0];
	LOG_VERBOSE(1, "Selecting Input File positions");
	ReadFileReader rfr(file, "");

	unsigned long fileSize = rfr.getFileSize();
	LOG_DEBUG(1, "FileSize of " << file << " is " << fileSize);
	unsigned long minBytes = RSOptions::getOptions().getMinBytesPerRecord();
	unsigned long numSamples = RSOptions::getOptions().getNumSamples();

	// if more than 15% of the file is estimated to be requested, read it sequentially
	// and pick reads randomly
	unsigned long count = 0;
	if (numSamples * minBytes * 100 > fileSize * RSOptions::getOptions().getMaxPercentForFseek()) {
		count = pickByBlock(rfr, numSamples);
	} else {
		count = pickBySeeks(rfr, numSamples, minBytes);
	}
	if (count < numSamples) {
		LOG_WARN(1, "Could not select all samples. " << count << " selected.");
	}
	LOG_DEBUG(1, "wrote " << count);

}
