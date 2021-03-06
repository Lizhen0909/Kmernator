//
// Kmernator/apps/TnfDistance.cpp
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

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

#include "config.h"
#include "Options.h"
#include "ReadSet.h"
#include "KmerSpectrum.h"
#include "Utils.h"
#include "Log.h"


using namespace std;
typedef TrackingDataMinimal4 DataType;
typedef KmerMap<DataType> KM;
typedef KmerSpectrum<KM, KM> KS;

struct DistanceFormula {
	enum Enum{EUCLIDEAN = 0, SPEARMAN, MAX};
};

class _TnfDistanceBaseOptions : public OptionsBaseInterface {
public:
	_TnfDistanceBaseOptions() :
		kmerSize(4),
		interDistanceFile(),
		intraInterFile(),
		clusterFile(),
		includeIntraInterDataFile(false),
		likelihoodBins(100),
		windowSize(10000),
		windowStep(1000),
		window2Size(-1),
		window2Step(1000),
		maxSamples(MAX_I64),
		clusterThresholdDistance(.175),
		referenceFiles(),
		distanceFormula(DistanceFormula::EUCLIDEAN) {}
	virtual ~_TnfDistanceBaseOptions() {}

	int &getKmerSize() {
		return kmerSize;
	}

	string &getInterFile() {
		return interDistanceFile;
	}
	string &getIntraInterFile() {
		return intraInterFile;
	}
	bool &getIncludeIntraInterDataFile() {
		return includeIntraInterDataFile;
	}
	int &getLikelihoodBins() {
		return likelihoodBins;
	}
	string &getClusterFile() {
		return clusterFile;
	}
	int &getWindowSize() {
		return windowSize;
	}
	int &getWindowStep() {
		return windowStep;
	}
	int &getWindow2Size() {
		return window2Size;
	}
	int &getWindow2Step() {
		return window2Step;
	}
	long &getMaxSamples() {
		return maxSamples;
	}
	float &getClusterThreshold() {
		return clusterThresholdDistance;
	}

	enum DistanceFormula::Enum &getDistanceFormula() {
		return distanceFormula;
	}

	FileListType &getReferenceFiles()
	{
		return referenceFiles;
	}


	void _resetDefaults() {
		GeneralOptions::_resetDefaults();
		GeneralOptions::getOptions().getVerbose() = 1;
		GeneralOptions::getOptions().getMmapInput() = false;
	}
	void _setOptions(po::options_description &desc, po::positional_options_description &p) {
		// set options specific to this program
		po::options_description opts("Tetra Nucleotide Frequency (TNF) Distance Options");
		opts.add_options()

		        ("kmer-size", po::value<int>()->default_value(kmerSize), "set to 4 for tetra-mer, 5 penta-mer, 1 for AT/GC ...")

				("reference-file", po::value<FileListType>(), "set reference file(s).  If set calculate distance of each input to this reference TNF vector")

				("inter-distance-file", po::value<string>()->default_value(interDistanceFile), "output inter-distance LT matrix (one line per input fasta) to this filename")

				("intra-inter-file", po::value<string>()->default_value(intraInterFile), "output two discrete likelihood functions of intra vs inter-distances between all windows fasta between separate files.  Assumes intra are calculated within a file and inter between files")

				("include-intra-inter-data-file", po::value<bool>()->default_value(includeIntraInterDataFile), "if set then intra-inter-file.data will be created too (VERY LARGE) one per thread")

				("likelihood-bins", po::value<int>()->default_value(likelihoodBins), "How many bins to create for the discrete likelihood functions")

				("window-size", po::value<int>()->default_value(windowSize), "size of adjacent intra/inter-distance windows")

				("window-step", po::value<int>()->default_value(windowStep), "step size of adjacent intra/inter-distance windows")

				("window2-size", po::value<int>()->default_value(window2Size), "if specified then calcuate intra and inter distances betweeen two different window sizes")

				("window2-step", po::value<int>()->default_value(window2Step), "step size of the second window adjacent intra/inter-distance windows")

				("max-samples", po::value<long>()->default_value(maxSamples), "maximum number of samples of intra and inter distances")

				("cluster-file", po::value<string>()->default_value(clusterFile), "cluster output filename")

				("cluster-threshold-distance", po::value<float>()->default_value(clusterThresholdDistance), "Euclidean distance threshold for clusters")

				("distance-formula", po::value<int>()->default_value(distanceFormula), "0 - Euclidean, 1 - Spearman")

				;


		desc.add(opts);

		GeneralOptions::_setOptions(desc, p);
	}
	bool _parseOptions(po::variables_map &vm) {
		bool ret = true;
		ret |= GeneralOptions::_parseOptions(vm);

		setOpt("kmer-size", kmerSize);
		setOpt("inter-distance-file", interDistanceFile);
		setOpt("intra-inter-file", intraInterFile);
		setOpt("include-intra-inter-data-file", includeIntraInterDataFile);
		setOpt("likelihood-bins", likelihoodBins);
		setOpt("window-size", windowSize);
		setOpt("window-step", windowStep);
		setOpt("window2-size", window2Size);
		setOpt("window2-step", window2Step);
		if (window2Size < windowSize) {
			window2Size = windowSize;
			window2Step = windowStep;
			LOG_VERBOSE_OPTIONAL(1, Log::printOptions() && Logger::isMaster(), "Overriding window2-size to:" << windowSize);
			LOG_VERBOSE_OPTIONAL(1, Log::printOptions() && Logger::isMaster(), "Overriding window2-step to:" << windowStep);
		}
		setOpt("max-samples", maxSamples);
		setOpt("cluster-file", clusterFile);
		setOpt("cluster-threshold-distance", clusterThresholdDistance);
		setOpt2("reference-file", referenceFiles);
		int df;
		setOpt("distance-formula", df);
		if (df < 0 || df >= DistanceFormula::MAX) {
			setOptionsErrorMsg("Invalid --distance-formula.  Please choose either 0 or 1");
			ret = false;
		} else {
			distanceFormula = (DistanceFormula::Enum) df;
		}

		return ret;
	}
private:
	int kmerSize;
	string interDistanceFile, intraInterFile, clusterFile;
	bool includeIntraInterDataFile;
	int likelihoodBins;
	int windowSize, windowStep, window2Size, window2Step;
	long maxSamples;
	float clusterThresholdDistance;
	FileListType referenceFiles;
	DistanceFormula::Enum distanceFormula;

};
typedef OptionsBaseTemplate< _TnfDistanceBaseOptions > TnfDistanceBaseOptions;

class _TnfDistanceOptions : public OptionsBaseInterface {
public:
	_TnfDistanceOptions() {}
	virtual ~_TnfDistanceOptions() {}
	void _resetDefaults() {
		TnfDistanceBaseOptions::_resetDefaults();
	}
	void _setOptions(po::options_description &desc, po::positional_options_description &p) {
		p.add("input-file", -1);
		po::options_description opts("TnfDistance <options> input.fa");
		desc.add(opts);

		TnfDistanceBaseOptions::_setOptions(desc,p);

	}
	bool _parseOptions(po::variables_map &vm) {
		bool ret = true;
		ret |= TnfDistanceBaseOptions::_parseOptions(vm);
		if (GeneralOptions::getOptions().getInputFiles().empty()) {
			ret = false;
			setOptionsErrorMsg("You must specify at least one input!");
		}
		return ret;
	}
};

typedef OptionsBaseTemplate< _TnfDistanceOptions > TnfDistanceOptions;
typedef std::vector<std::string> FastaVector;

class TNF {
public:
	typedef vector<float> Vector;
	typedef RankVector<float> RVector;

	static KM stdMap;
	static long stdSize;
	static DistanceFormula::Enum distanceFormula;

	static void init(int kmerSize, DistanceFormula::Enum df = DistanceFormula::EUCLIDEAN) {
		distanceFormula = df;
		KmerSizer::set(kmerSize);
		TEMP_KMER(kmer);
		TEMP_KMER(lc);
		kmer.clear();
		char fasta[kmerSize];
		strcpy(fasta, kmer.toFasta().c_str());

		do {
			kmer.set(fasta);
			kmer.buildLeastComplement(lc);

			bool exists = stdMap.exists(lc);
			LOG_DEBUG_OPTIONAL(2, exists, lc.toFasta() << "\t" << "\t" << kmer.toFasta());
			if ( !exists ) {

				stdMap.insert(lc, DataType());
			}
		} while(TwoBitSequence::permuteFasta(fasta, kmerSize));

		stdSize = stdMap.size();
		LOG_DEBUG(1, "Found " << stdSize << " unique " << kmerSize << "-mers");
	}
	static std::string toHeader() {
		assert(stdSize > 0);
		std::stringstream ss;
		ss << "Count\tLength";
		for(KM::Iterator it = stdMap.begin(); it != stdMap.end(); it++) {
			ss << "\t" << it->key().toFasta();
		}
		return ss.str();
	}
private:
	Vector tnfValues;
	RVector *rvector;
	float length;
public:
	TNF() : rvector(NULL), length(1.0) {
		assert(stdSize > 0);
		tnfValues.resize(stdSize, 0);
	}
	TNF(const TNF &copy) : rvector(NULL), length(1.0) {
		assert(stdSize > 0);
		*this = copy;
	}
	TNF(const KM &map) :  rvector(NULL), length(1.0) {
		assert(stdSize > 0);
		buildTnf(map);
		buildLength();
		setRankVector();
	}
	TNF &operator=(const TNF &copy) {
		if (this == &copy)
			return *this;
		length = copy.length;
		tnfValues = copy.tnfValues;
		setRankVector();
		return *this;
	}
	virtual ~TNF() {
		clear();
	}
	void reset() {
		clear();
		tnfValues.resize(stdSize, 0);
	}
	void clear() {
		length = 1.0;
		tnfValues.clear();
		clearRankVector();
	}
	void clearRankVector() {
		if (rvector != NULL) {
			delete rvector;
			rvector = NULL;
		}
	}
	void setRankVector(bool force = false) {
		if (force || distanceFormula == DistanceFormula::SPEARMAN) {
			if (rvector == NULL)
				rvector = new RVector(tnfValues);
			else
				*rvector = RVector(tnfValues);
		}
	}
	RVector &getRankVector() const {
		assert(rvector != NULL);
		return *rvector;
	}
	TNF &operator*(float factor) {
		for(unsigned int i = 0; i < tnfValues.size(); i++) {
			tnfValues[i] *= factor;
		}
		buildLength();
		setRankVector();
		return *this;
	}
	TNF &operator+(const TNF &rh) {
		for(unsigned int i = 0; i < tnfValues.size(); i++) {
			tnfValues[i] = tnfValues[i] + rh.tnfValues[i];
		}
		buildLength();
		setRankVector();
		return *this;
	}
	std::string toString() const {
		std::stringstream ss;
		ss << getCount() << "\t" << getLength();
		for(unsigned int i = 0 ; i < tnfValues.size(); i++) {
			ss << "\t" << tnfValues[i] / length;
		}
		return ss.str();
	}

	double getCount() const {
		double count = 0.0;
		for(unsigned int i = 0 ; i < tnfValues.size(); i++) {
			count += tnfValues[i];
		}
		return count;
	}
	double getLength() const {
		return length;
	}
	double calcLength() const {
		double l = 0.0;
		bool debug = Log::isDebug(2);
		ostream *debugPtr = NULL;
		if (debug) {
			debugPtr = Log::Debug("buildLength()").getOstreamPtr();
		}
		for(unsigned int i = 0 ; i < tnfValues.size(); i++) {
			l += tnfValues[i] * tnfValues[i];
			if (debug) *debugPtr << tnfValues[i] << "\t";
		}
		l = sqrt(l);

		if (debug) *debugPtr << l << endl;
		return l;
	}
	void buildLength() {
		length = calcLength();
		if (length == 0.0) // avoid any divide by zero problems
			length = 1.0;
	}
	void buildTnf(const KM & map) {
		if ((long) tnfValues.size() != stdSize) {
			tnfValues.clear();
			tnfValues.resize(stdSize, 0);
		}
		int i = 0;
		for(KM::Iterator it = stdMap.begin(); it != stdMap.end(); it++) {
			KM::ElementType elem = map.getElementIfExists(it->key());
			float t = 0.0;
			if (elem.isValid()) {
				t = elem.value();
			}
			tnfValues[i++] = t;
		}
		buildLength();
		setRankVector();
	}
	float getDistance(const TNF &other) const {
		float dist = 0.0;
		if (distanceFormula == DistanceFormula::EUCLIDEAN) {
			for(unsigned int i = 0 ; i < tnfValues.size(); i++) {
				float d = tnfValues[i] / getLength() - other.tnfValues[i] / other.getLength();
				dist += d*d;
			}
			dist = sqrt(dist);
		} else if (distanceFormula == DistanceFormula::SPEARMAN) {
			dist = RVector::getSpearmanDistance(getRankVector(), other.getRankVector());
		}
		return dist;
	}
	float getDistance(const KM &query) const {
		TNF other(query);
		return getDistance(other);
	}
	static float getDistance(const KM &target, const KM &query) {
		TNF tgt(target), qry(query);
		return tgt.getDistance(qry);
	}

};
KM TNF::stdMap(2112); // initialize room for hexamers
long TNF::stdSize = 0;
DistanceFormula::Enum TNF::distanceFormula;

typedef std::vector<TNF> TNFS;
TNFS buildTnfs(const ReadSet &reads) {
	TNFS tnfs;
	long size = reads.getSize();
	tnfs.resize(size);
	int targetBucketElementCount = 8;

#pragma omp parallel for schedule(dynamic,1)
	for(long readIdx = 0; readIdx < size; readIdx++) {
		KS ksReads;
		ksReads.setSolidOnly();
		ksReads.solid.resizeBuckets(TNF::stdSize / targetBucketElementCount + 1, targetBucketElementCount);
		ReadSet singleRead;
		Read read = reads.getRead(readIdx);
		LOG_DEBUG(1, "buildTnfs() for: " << read.getName());
		singleRead.append(read);

		ksReads.buildKmerSpectrum(singleRead, true);

		TNF tnf(ksReads.solid);
		tnfs[readIdx] = tnf;
	}
	return tnfs;
}

void shredReadByWindow(const Read &read, ReadSet &shrededReads, long window, long step) {
	string fullFasta = read.getFasta();
	long length = fullFasta.length();
	for(long i = 0 ; i < length - window; i+= step) {
		string fasta = fullFasta.substr(i, window);
		Read newRead(read.getName() + ":" + boost::lexical_cast<string>(i) + "-" + boost::lexical_cast<string>(i+fasta.length()), fasta, string(fasta.length(), Kmernator::PRINT_REF_QUAL), "");
		shrededReads.append(newRead);
	}
}

TNFS buildIntraTNFs(const ReadSet &reads) {
	return buildTnfs(reads);
}

TNFS buildIntraTNFs(const Read &read, long window, long step) {
	ReadSet reads;
	shredReadByWindow(read, reads, window, step);
	return buildIntraTNFs(reads);
}
// use this to remove TNFs that are missing too much of the expected or required data.
void purgeShortTNFS(TNFS &tnfs, int minimumCount) {
	long oldSize = tnfs.size();
	long size=oldSize;
	for(long readIdx = 0; readIdx < size; readIdx++) {
		if (tnfs[readIdx].getCount() < minimumCount) {
			std::swap(tnfs[readIdx], tnfs[size-1]);
			tnfs.pop_back();
			size--;
			readIdx--;
		}
	}
	LOG_DEBUG_OPTIONAL(1, oldSize != size, "purgeShortTNFS(" << oldSize << "," << minimumCount << "): purged " << oldSize - size << " short TNFs, remaining: " << size);
}

class Result {
public:
	float distance;
	string label;
	TNF tnf;
	Result() : distance(0.0) {}
	Result(float _dist, string _label) : distance(_dist), label(_label) {}
	Result(float _dist, string _label, TNF _tnf) :  distance(_dist), label(_label), tnf(_tnf) {}
	Result(const Result &copy) {
		*this = copy;
	}
	Result &operator=(const Result &copy) {
		if (this == &copy)
			return *this;
		distance = copy.distance;
		label = copy.label;
		tnf = copy.tnf;
		return *this;
	}
};
typedef std::vector< Result > Results;
class ResultCompare {
public:
	bool operator() (const Result &a, const Result &b) { return (a.distance < b.distance); }
};


Results calculateDistances(const TNF &refTnf, const ReadSet &reads, const TNFS &tnfs) {
	Results results;
	long size = reads.getSize();
	results.resize(size);

#pragma omp parallel for
	for(long readIdx = 0; readIdx < size; readIdx++) {

		ReadSet singleRead;
		Read read = reads.getRead(readIdx);
		string name = read.getName();

		float dist = refTnf.getDistance(tnfs[readIdx]);
		if (dist <= 0.20) {
			string fullFasta = read.getFasta();
			long len = fullFasta.length();
			int divs = 5;
			TNFS intraTnfs = buildIntraTNFs(read,len/divs, len/divs);

			stringstream ss;
			ss.precision(3);
			ss << name;
			for(long i = 0 ; i < (long) intraTnfs.size() ; i++) {
				float distPart = refTnf.getDistance(intraTnfs[i]);
				ss << "\t" << fixed << distPart;
			}
			name = ss.str();
		}
		results[readIdx] = Result( dist, name );
	}
	return results;
}
Results calculateDistances(const TNF &refTNF, const ReadSet &reads) {
	return calculateDistances(refTNF, reads, buildTnfs(reads));
}


typedef vector<float> DVector;
typedef vector< DVector > DMatrix;
class MinVecOper {
public:
	bool operator()( const DVector::iterator &a, const DVector::iterator &b) const {
		return (*a < *b);
	}
} mvo;

typedef GenericHistogram<float, long> GH;

int main(int argc, char *argv[]) {

	if (!TnfDistanceOptions::parseOpts(argc, argv)) exit(1);

	Cleanup::prepare();

	ReadSet refs;
	ReadSet reads;
	TNF::init(TnfDistanceBaseOptions::getOptions().getKmerSize(),
			TnfDistanceBaseOptions::getOptions().getDistanceFormula());

	OptionsBaseInterface::FileListType &inputs = Options::getOptions().getInputFiles();
	LOG_VERBOSE(1, "Loading " << inputs.size() << " input files");
	reads.appendAllFiles(inputs);

	KS ksRef;
	ksRef.setSolidOnly();

	LOG_VERBOSE(1, "Calculating TNFs for " << reads.getSize() << " input reads");
	TNFS readTnfs = buildTnfs(reads);

	ostream *out = &cout;
	OfstreamMap om(Options::getOptions().getOutputFile(), "");
	if (!Options::getOptions().getOutputFile().empty()) {
		out = &om.getOfstream("");
	}
	OptionsBaseInterface::FileListType referenceInputs = TnfDistanceBaseOptions::getOptions().getReferenceFiles();
	if (!referenceInputs.empty()) {
		// compare distances from reference to each read in the input
		LOG_VERBOSE(1, "Loading reference input");
		refs.appendAllFiles(referenceInputs);
		ksRef.buildKmerSpectrum(refs, true);
		TNF refTnf(ksRef.solid);

		LOG_VERBOSE(1, "Calculating distances from input to reference");
		Results results = calculateDistances(refTnf, reads, readTnfs);
		std::sort(results.begin(), results.end(), ResultCompare());
		for(Results::iterator it = results.begin(); it != results.end(); it++) {
			*out << it->distance << "\t" << it->label << endl;
		}
	} else {
		// output TNF matrix for each read in the input

		LOG_VERBOSE(1, "Outputting TNF vectors from input sequences");
		*out << "Label\t" << TNF::toHeader() << endl;
		for(ReadSet::ReadSetSizeType readIdx = 0; readIdx < reads.getSize(); readIdx++) {

			Read read = reads.getRead(readIdx);

			*out << read.getName() << "\t" << readTnfs[readIdx].toString() << endl;
		}

	}

	string interFile = TnfDistanceBaseOptions::getOptions().getInterFile();
	if (!interFile.empty()) {
		LOG_VERBOSE(1, "Outputting Lower Triangle of inter-TNF distances between inputs sequences.");
		OfstreamMap om(interFile, "");
		ostream &os = om.getOfstream("");
		for(ReadSet::ReadSetSizeType readIdxi = 0; readIdxi < reads.getSize(); readIdxi++) {
			os << reads.getRead(readIdxi).getName();
			for(ReadSet::ReadSetSizeType readIdxj = 0; readIdxj < readIdxi; readIdxj++) { // LT matrix only
				float dist = readTnfs[readIdxi].getDistance(readTnfs[readIdxj]);
				os << "\t" << dist;
			}
			os << endl;
		}
	}

	string intraInterFile = TnfDistanceBaseOptions::getOptions().getIntraInterFile();
	if (!intraInterFile.empty()) {
		long window = TnfDistanceBaseOptions::getOptions().getWindowSize();
		long step = TnfDistanceBaseOptions::getOptions().getWindowStep();
		long window2 = TnfDistanceBaseOptions::getOptions().getWindow2Size();
		long step2 = TnfDistanceBaseOptions::getOptions().getWindow2Step();
		int numBins = TnfDistanceBaseOptions::getOptions().getLikelihoodBins();
		bool dataFile = TnfDistanceBaseOptions::getOptions().getIncludeIntraInterDataFile();

		float maxDistance = (TNF::distanceFormula == DistanceFormula::EUCLIDEAN) ? sqrt(2.0) : 1.0;
		GH intraHist(0.0, maxDistance, numBins, true),
		   interHist(0.0, maxDistance, numBins, true),
		   intraVsWholeHist(0.0, maxDistance, numBins, true),
		   interVsWholeHist(0.0, maxDistance, numBins, true);

		std::vector< TNFS > interTnfs, interTnfs2;
		interTnfs.resize( reads.getReadFileNum( reads.getSize() - 1 ));
		interTnfs2.resize( reads.getReadFileNum( reads.getSize() - 1 ));
		assert(interTnfs.size() == inputs.size());
		TNFS wholeTnfs;
		wholeTnfs.resize( interTnfs.size() );

		LOG_VERBOSE(1, "Creating intra cluster TNF distance histogram for " << interTnfs.size() << " different sets.");
		LOG_DEBUG(1, MemoryUtils::getMemoryUsage());
		long maxSamples = TnfDistanceBaseOptions::getOptions().getMaxSamples();
		long maxIntraSamples = maxSamples / inputs.size();
		if (inputs.size() > 1)
			maxIntraSamples += 1;
		LongRand randGen[ omp_get_max_threads() ];
		ostream *dataPtr[omp_get_max_threads()];
		OfstreamMap om(intraInterFile, "");
#pragma omp parallel
		{
			int threadId = omp_get_thread_num();
			randGen[threadId] = LongRand();
			if (dataFile) {
				dataPtr[threadId] = & om.getOfstream(".data." +  boost::lexical_cast<string>( omp_get_thread_num() ));
				*dataPtr[threadId] << "Distance\tIntra\tInter\n";
			} else {
				dataPtr[threadId] = NULL;
			}
		}

		ReadSet shrededReads, shrededReads2;
		int thisFileIdx = 0;
		for(ReadSet::ReadSetSizeType readIdx = 0; readIdx < reads.getSize(); readIdx++) {
			assert(thisFileIdx == reads.getReadFileNum(readIdx) - 1);
			Read read = reads.getRead(readIdx);
			shredReadByWindow(read, shrededReads, window, step);
			if (window != window2)
				shredReadByWindow(read, shrededReads2, window2, step2);

			int nextFileIdx = reads.getReadFileNum(readIdx+1) - 1;
			wholeTnfs[ thisFileIdx ] = wholeTnfs[ thisFileIdx ] + readTnfs[ readIdx ];
			// if this was the last read or the next read is from a new file
			// process the intra
			if (readIdx+1 == reads.getSize() || thisFileIdx != nextFileIdx) {

				LOG_VERBOSE(1, "Creating intra cluster TNFs for fileNum: " << thisFileIdx << " with " << shrededReads.getSize() << " shreded reads (readIdx: " << readIdx << ")");
				LOG_DEBUG(1, MemoryUtils::getMemoryUsage());

				assert(thisFileIdx < (int) interTnfs.size());
				interTnfs[thisFileIdx] = buildIntraTNFs(shrededReads);
				purgeShortTNFS(interTnfs[thisFileIdx], window*3/4);
				if (window != window2) {
					interTnfs2[thisFileIdx] = buildIntraTNFs(shrededReads2);
					purgeShortTNFS(interTnfs2[thisFileIdx], window2*3/4);
					LOG_VERBOSE(1, "Window2 TNFS: " << shrededReads2.getSize() << " shreded reads");
				}
				TNFS &intraTnfs = interTnfs[thisFileIdx];
				long intraTnfsSize = intraTnfs.size();
				if (intraTnfsSize > 1) {


#pragma omp parallel for
					for(long i = 0 ; i < (long) intraTnfsSize; i++) {
						float dist = wholeTnfs[thisFileIdx].getDistance(intraTnfs[i]);
						intraVsWholeHist.observe(dist);
					}

					LowerTriangle<false, long> lt(intraTnfsSize);
					long numSamples = lt.getNumValues();
					assert(numSamples == (long) ( intraTnfsSize * (intraTnfsSize - 1) / 2) );

					if (window == window2) { // same sized windows

						if (numSamples < (long) maxIntraSamples) {
							// calculate everything
#pragma omp parallel for schedule(dynamic,1)
							for(long i = 0 ; i < (long) intraTnfs.size(); i++) {
								for(long j = 0 ; j < i ; j++) { // lower triangle only
									float dist = intraTnfs[i].getDistance(intraTnfs[j]);
									intraHist.observe(dist);
									if (dataPtr[omp_get_thread_num()] != NULL)
										*dataPtr[omp_get_thread_num()] << dist << "\t1\t0\n";
								}
							}
						} else {
							// subsample the full data set
							LOG_DEBUG(1, "Subsampling all possible: " << numSamples << " to " << maxIntraSamples);

#pragma omp parallel for
							for(long k = 0; k < (long) maxIntraSamples; k++) {
								long x,y, indexSample = randGen[omp_get_thread_num()].getRand() % numSamples;
								lt.getXY(indexSample, x, y);
								float dist = intraTnfs[x].getDistance(intraTnfs[y]);
								intraHist.observe(dist);
								if (dataPtr[omp_get_thread_num()] != NULL)
									*dataPtr[omp_get_thread_num()] << dist << "\t1\t0\n";
							}
						}

					} else { // different sized windows...
						TNFS &intraTnfs2 = interTnfs2[thisFileIdx];
						long intraTnfsSize2 = intraTnfs2.size();
						numSamples = intraTnfsSize * intraTnfsSize2;

						if (numSamples < (long) maxIntraSamples) {
							// calculate everything

#pragma omp parallel for schedule(dynamic,1)
							for(long i = 0 ; i < (long) intraTnfsSize; i++) {
								for(long j = 0 ; j < intraTnfsSize2 ; j++) {
									float dist = intraTnfs[i].getDistance(intraTnfs2[j]);
									intraHist.observe(dist);
									if (dataPtr[omp_get_thread_num()] != NULL)
										*dataPtr[omp_get_thread_num()] << dist << "\t1\t0\n";
								}
							}
						} else {
							// subsample the full data set
							LOG_DEBUG(1, "Subsampling all possible: " << numSamples << " to " << maxIntraSamples);

#pragma omp parallel for
							for(long k = 0; k < (long) maxIntraSamples; k++) {
								long x,y, indexSample = randGen[omp_get_thread_num()].getRand() % numSamples;
								x = indexSample / intraTnfsSize2;
								y = indexSample % intraTnfsSize2;
								assert(x < intraTnfsSize);
								float dist = intraTnfs[x].getDistance(intraTnfs2[y]);
								intraHist.observe(dist);
								if (dataPtr[omp_get_thread_num()] != NULL)
									*dataPtr[omp_get_thread_num()] << dist << "\t1\t0\n";
							}

						}
					}

				}
				shrededReads.clear();
				shrededReads2.clear();
				if (readIdx + 1 != reads.getSize()) {
					thisFileIdx = reads.getReadFileNum(readIdx+1) - 1;
				} else {
					assert(thisFileIdx == (int) inputs.size() - 1);
				}
			}
		}

		LOG_VERBOSE(1, "intra-histogram totalCount: " << intraHist.getTotalCount() << " outliers: " << intraHist.getOutlierCount());
		LOG_VERBOSE(1, "Creating inter cluster TNF distance histogram.");
		LOG_DEBUG(1, MemoryUtils::getMemoryUsage());

		long maxInterSamples = maxSamples / inputs.size();
		if (inputs.size() >= 2) {
			maxInterSamples = maxSamples / (inputs.size() * (inputs.size()-1) / 2);
			if (inputs.size() > 2)
				maxInterSamples += 1;
		}

		for(long fileIdxi = 0; fileIdxi < (long) interTnfs.size(); fileIdxi++) {
			TNFS &interi = interTnfs[fileIdxi];

#pragma omp parallel for
			for(long k = 0 ; k < (long) interi.size(); k++) {
				for(long fileIdxj = 0; fileIdxj < (int) interTnfs.size(); fileIdxj++) {
					if (fileIdxi == fileIdxj) continue; // intra already calculated
					float dist = wholeTnfs[fileIdxj].getDistance(interi[k]);
					interVsWholeHist.observe(dist);
				}
			}

			// if windows are the same size, just calc lower triangle
			long maxJ = (window == window2) ? fileIdxi : interTnfs2.size();

			for(long fileIdxj = 0 ; fileIdxj < maxJ; fileIdxj++) { // lower triangle only
				if (fileIdxj == fileIdxi)
					continue; // intra already calculated
				// choose which version to use window or window2
				TNFS &interj = (window == window2) ? interTnfs[fileIdxj] : interTnfs2[fileIdxj];

				long isize = interi.size(), jsize = interj.size();
				long numSamples = isize * jsize;
				if (numSamples < maxInterSamples) {
					// calculate everything
#pragma omp parallel for
					for(long k = 0 ; k < isize; k++) {
						for(long l = 0; l < jsize; l++) {
							float dist = interi[k].getDistance(interj[l]);
							interHist.observe(dist);
							if (dataPtr[omp_get_thread_num()] != NULL)
								*dataPtr[omp_get_thread_num()] << dist << "\t0\t1\n";
						}
					}
				} else {
					// subsample
#pragma omp parallel for
					for(long k = 0; k < maxInterSamples ; k++) {
						long indexSample = randGen[omp_get_thread_num()].getRand() % numSamples;
						long x = indexSample / jsize, y = indexSample % jsize;
						assert(x < isize);
						assert(y < jsize);
						float dist = interi[x].getDistance(interj[y]);
						interHist.observe(dist);
						if (dataPtr[omp_get_thread_num()] != NULL)
							*dataPtr[omp_get_thread_num()] << dist << "\t0\t1\n";
					}
				}
			}
		}

		LOG_VERBOSE(1, "inter-histogram totalCount: " << interHist.getTotalCount() << " outliers: " << interHist.getOutlierCount());

		assert(interHist.getOutlierCount() == 0);
		assert(intraHist.getOutlierCount() == 0);

		ostream &os = om.getOfstream("");

		double totalIntra = intraHist.getTotalCount(), totalInter = interHist.getTotalCount(),
			totalIntraVsWhole = intraVsWholeHist.getTotalCount(), totalInterVsWhole = interVsWholeHist.getTotalCount();
		os << "Distance\twindow\twindow2\tIntraLikelihood\tInterLikelihood\tIntraVsWholeLikelihood\tInterVsWholeLikelihood\n";
		for(int i = 0; i < numBins; i++) {
			GH::Bin intraBin = intraHist.getBin(i), interBin = interHist.getBin(i),
					intraVsWholeBin = intraVsWholeHist.getBin(i), interVsWholeBin = interVsWholeHist.getBin(i);
			assert(intraBin.binStart == interBin.binStart);
			os << intraBin.binStart << "\t" << window << "\t" << window2 << "\t" << intraBin.count/totalIntra << "\t" << interBin.count/totalInter << "\t"
					<< intraVsWholeBin.count / totalIntraVsWhole << "\t" << interVsWholeBin.count / totalInterVsWhole << "\n";
		}
	}

	string clusterFile = TnfDistanceBaseOptions::getOptions().getClusterFile();
	if (!clusterFile.empty()) {
		bool debug = Log::isDebug(1);
		long size = readTnfs.size();

		LOG_VERBOSE(1, "Building clusters");
		OfstreamMap om(clusterFile, "");
		ostream &os = om.getOfstream("");
		DMatrix distMatrix;
		distMatrix.resize( size );
		vector< vector< string > > clusterNames;
		clusterNames.resize( size );

		//TODO optimize this and the the while loop (use LT, and directed updates to minVec)
		vector< DVector::iterator > minVec;
		minVec.resize( size );
		float clusterThreshold = TnfDistanceBaseOptions::getOptions().getClusterThreshold();

		for(long i = 0; i < size; i++) {
			clusterNames[i].push_back( reads.getRead(i).getName() );
			DVector &vect = distMatrix[i];
			vect.resize(i+1);
			for(long j = 0 ; j <= i; j++) {
				if (j==i)
					vect[j] = clusterThreshold+1.0;
				else
					vect[j] = readTnfs[i].getDistance(readTnfs[j]);
			}
			minVec[i] = std::min_element( vect.begin(), vect.end() );
		}

		LOG_VERBOSE(1, "Finished dist matrix");
		if (debug) {
			ostream &debugOut = Log::Debug("DistMatrix");
			for(long i = 0 ; i < size ; i++) {
				debugOut << "distMatrix: " << i << "\t" << clusterNames[i][0];
				for(long j = 0 ; j <= i ; j++)
					debugOut << "\t" << distMatrix[i][j];
				debugOut << std::endl;
			}
		}


		TNFS readTnfs2 = TNFS(readTnfs);

		while( true ) {
			// set the thresholds for each remaining read

			vector< DVector::iterator >::iterator minElem = std::min_element(minVec.begin(), minVec.end(), mvo);
			if (**minElem > clusterThreshold)
				break;
			LOG_DEBUG(1, **minElem );

			unsigned long tgtj = minElem - minVec.begin();
			unsigned long tgti = *minElem - distMatrix[tgtj].begin();

			unsigned long lastIdx = minVec.size() - 1;
			if (debug) {
				ostream &debugOut = Log::Debug("MinValue");
				for(unsigned long i2 = 0 ; i2 < minVec.size(); i2++) {
					unsigned long j = (i2 <= tgti) ? i2 : tgti;
					unsigned long i = (i2 <= tgti) ? tgti : i2;

					debugOut << distMatrix[i][j] << "\t";
				}
				debugOut << endl;
			}
			clusterNames[tgti].insert( clusterNames[tgti].end(), clusterNames[tgtj].begin(), clusterNames[tgtj].end() );
			clusterNames[tgtj].clear();
			LOG_VERBOSE(1, "Cluster merged " << tgti << " with " << tgtj << " " << **minElem);
			readTnfs2[tgti] = readTnfs2[tgti] + readTnfs2[tgtj];

			// remove tgtj from vectors and matrix (1 row and 1 column);
			std::swap(minVec[tgtj], minVec[lastIdx]); minVec.pop_back();
			std::swap(clusterNames[tgtj], clusterNames[lastIdx]); clusterNames.pop_back();
			std::swap(distMatrix[tgtj], distMatrix[lastIdx]); distMatrix.pop_back();
			std::swap(readTnfs2[tgtj], readTnfs2[lastIdx]); readTnfs2.pop_back();

			for(unsigned long i = tgtj + 1; i < minVec.size(); i++) {
				float dist = distMatrix[tgtj][i];
				distMatrix[i][tgtj] = dist;

				if (*minVec[i] > dist) {
					minVec[i] = distMatrix[i].begin() + tgtj;
				}
			}
			distMatrix[tgtj].resize(tgtj+1);
			distMatrix[tgtj][tgtj] = clusterThreshold + 1.0;
			minVec[tgtj] = std::min_element( distMatrix[tgtj].begin(), distMatrix[tgtj].end() );

			// update row & column tgti and affected minVects
			for(unsigned long i = 0 ; i < minVec.size(); i++) {
				if (i == tgti)
					distMatrix[tgti][tgti] = clusterThreshold+1.0;
				else {
					float dist = readTnfs2[tgti].getDistance(readTnfs2[i]);
					if (i < tgti) {
						distMatrix[tgti][i] = dist;
					} else {
						distMatrix[i][tgti] = dist;
						if (*minVec[i] > dist) {
							minVec[i] = distMatrix[i].begin() + tgti;
						}
					}
				}
			}
			minVec[tgti] = std::min_element( distMatrix[tgti].begin(), distMatrix[tgti].end() );

		}

		for(unsigned long i = 0 ; i < minVec.size(); i++) {
			for(unsigned long j = 0 ; j < clusterNames[i].size(); j++)
				os << clusterNames[i][j] << "\t";
			if (!clusterNames[i].empty())
				os << endl;
			clusterNames[i].clear();
		}
		clusterNames.clear();

	}

	return 0;
}

