/*
 * Cap3.h
 *
 *  Created on: Sep 7, 2011
 *      Author: regan
 */
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

#ifndef CAP3_H_
#define CAP3_H_

#include "Options.h"
#include "Utils.h"
#include "ReadSet.h"
#include "Kmer.h"
#include "KmerSpectrum.h"
#include "Utils.h"
#include "ExternalAssembler.h"

class _Cap3Options : public OptionsBaseInterface {
public:
	_Cap3Options() : cap3Path() {}
	virtual ~_Cap3Options() {}
	std::string &getCap3Path() {
		return cap3Path;
	}
	void _resetDefaults() {
		Options::getOptions().getMmapInput() = false;
	}
	void _setOptions(po::options_description &desc,	po::positional_options_description &p) {
		po::options_description opts("Cap3 Options");

		opts.add_options()
				("cap3-path", po::value<std::string>()->default_value(cap3Path), "if set, cap3 will be used to extend contigs")

				;

		desc.add(opts);
	}
	bool _parseOptions(po::variables_map &vm) {
		bool ret = true;
		setOpt("cap3-path", cap3Path);

		return ret;
	}
protected:
	std::string cap3Path;
};
typedef OptionsBaseTemplate< _Cap3Options > Cap3Options;

class Cap3 : public ExternalAssembler {
public:
	Cap3() : ExternalAssembler("Cap3", 2, 25000, 10000) {}
	virtual ~Cap3() {}

	Read extendContig(const Read &oldContig, const ReadSet &inputReads) {

		std::string prefix = "/.cap3-assembly";
		std::string outputDir = Cleanup::makeTempDir(Options::getOptions().getTmpDir(), prefix);
		LOG_VERBOSE_OPTIONAL(1, !GeneralOptions::getOptions().getKeepTempDir().empty(), "Saving Cap3 working directory for " << oldContig.getName()
				<< " to " << GeneralOptions::getOptions().getKeepTempDir() << outputDir.substr(outputDir.find(prefix)));
		std::string baseName = outputDir + "/input.fa";
		writeReads(inputReads, oldContig, baseName, FormatOutput::FastaUnmasked());
		std::string log = baseName + ".log";
		std::string cmd = Cap3Options::getOptions().getCap3Path() + " " + baseName + " > " + log + " 2>&1";
		LOG_DEBUG_OPTIONAL(1, true, "Executing cap3 for " << oldContig.getName() << "(" << inputReads.getSize() << " read pool): " << cmd);

		std::string newContigFile = baseName + ".cap.contigs";
		Read bestRead = executeAssembly(cmd, newContigFile, oldContig);

		if (bestRead.empty())
			LOG_WARN(1, "Could not assemble " << oldContig.getName() << " with pool of " << inputReads.getSize() << " reads: " << FileUtils::dumpFile(log));

		Cleanup::removeTempDir(outputDir);
		return bestRead;
	}

};

#endif /* CAP3_H_ */

