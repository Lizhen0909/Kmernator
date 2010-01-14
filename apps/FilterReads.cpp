// $Header: /repository/PI_annex/robsandbox/KoMer/apps/FilterReads.cpp,v 1.2 2010-01-14 01:17:53 regan Exp $
//

#include <iostream>
#include <cstdlib>
#include <cstring>

#include "config.h"
#include "Options.h"
#include "ReadSet.h"
#include "FilterKnownOddities.h"
#include "KmerSpectrum.h"
#include "ReadSelector.h"

using namespace std;

typedef TrackingDataMinimal8 DataType;
typedef KmerSpectrum<DataType,DataType> KS;
typedef ReadSelector<DataType> RS;

int main(int argc, char *argv[]) {
   if (!Options::parseOpts(argc,argv))
      throw std::invalid_argument("Please fix the command line arguments");

    cerr << MemoryUtils::getMemoryUsage() << endl;
	
	ReadSet reads;
    FilterKnownOddities filter;
    KmerSizer::set(Options::getKmerSize());
    
    Options::FileListType inputs     = Options::getInputFiles();
    cerr << "Reading Input Files" << endl;
    reads.appendAllFiles(inputs);
    cerr << "loaded " << reads.getSize() << " Reads, " << reads.getBaseCount() << " Bases " << endl;
    cerr << MemoryUtils::getMemoryUsage() << endl;    
    
    cerr << "Identifying Pairs: ";
    long numPairs =  reads.identifyPairs();
    cerr << numPairs << endl;
    cerr << MemoryUtils::getMemoryUsage() << endl;
    
    cerr << "Applying filter to Input Files" << endl;
    unsigned long filtered = filter.applyFilter(reads);
    cerr << "filter affected " << filtered << " Reads " << endl;
    cerr << MemoryUtils::getMemoryUsage() << endl;

    long numBuckets = KS::estimateWeakKmerBucketSize( reads, 64 );
    cerr << "targetting " << numBuckets << " buckets for reads " << endl;

    KS spectrum(numBuckets);
    cerr << MemoryUtils::getMemoryUsage() << endl;

    TrackingData::minimumWeight = 0.25;

    spectrum.buildKmerSpectrum( reads );
    cerr << MemoryUtils::getMemoryUsage() << endl;

    cerr << "Picking reads: " << endl;
    RS selector(reads, spectrum.weak);
    long picked;
    if (reads.hasPairs())
      picked = selector.pickAllPassingPairs(Options::getMinDepth());
    else
      picked = selector.pickAllPassingReads(Options::getMinDepth());
    cerr << "Picked " << picked << " / " << reads.getSize() << " reads" << endl;
    
    std::string outputFile = Options::getOutputFile();
    if (!outputFile.empty()) {
    	cerr << "Writing reads to output file: " << outputFile << endl;
    	ofstream out;
    	out.open(outputFile.c_str());
    	selector.writePicks(out);
    	out.close();
    }
    
}
