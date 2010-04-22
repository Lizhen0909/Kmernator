// $Header: /repository/PI_annex/robsandbox/KoMer/src/ReadFileReader.h,v 1.3 2010-04-21 23:39:04 regan Exp $
//

#ifndef _READ_FILE_READER_H
#define _READ_FILE_READER_H
#include <string>
#include <cstdlib>

#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>

#include "config.h"

using namespace std;

class ReadFileReader {
public:
	typedef KoMer::RecordPtr RecordPtr;
	typedef KoMer::MmapSource MmapSource;
	typedef KoMer::MmapIStream MmapIStream;

    class SequenceStreamParser;
	typedef boost::shared_ptr< SequenceStreamParser > SequenceStreamParserPtr;

private:
	SequenceStreamParserPtr _parser;
	string _path;
	ifstream _ifs;
	ifstream _qs;
	istringstream _iss;

public:
	ReadFileReader(): _parser() {}

	ReadFileReader(string fastaFilePath, string qualFilePath) :
	    _parser(), _path(fastaFilePath) {

        _ifs.open(fastaFilePath.c_str());

		if (_ifs.fail())
			throw runtime_error("Could not open : " + fastaFilePath);

		if (Options::getIgnoreQual())
			qualFilePath.clear();

		if (!qualFilePath.empty()) {
			_qs.open(qualFilePath.c_str());
			if (_qs.fail())
				throw runtime_error("Could not open : " + qualFilePath);
		} else {
		     if (!Options::getIgnoreQual()) {
			   // test for an implicit qual file
			   _qs.open((fastaFilePath + ".qual").c_str());
			 }
		}
		setParser(_ifs, _qs);
	}

	ReadFileReader(string &fasta) :
		_iss(fasta) {
		_parser = SequenceStreamParserPtr(new FastaStreamParser(_iss));
	}

	ReadFileReader(MmapSource &mmap) {
		setReader(mmap);
	}
	ReadFileReader(MmapSource &mmap1, MmapSource &mmap2) {
		setReader(mmap1,mmap2);
	}

	~ReadFileReader() {
		_ifs.close();
		_qs.close();
	}

	void setReader(MmapSource &mmap) {
		assert(mmap.is_open());
		setParser(mmap, *mmap.data());
	}
	void setReader(MmapSource &mmap1, MmapSource &mmap2) {
		assert(mmap1.is_open());
		assert(mmap2.is_open());
		setParser(mmap1,mmap2);
	}
	void setParser(istream &fs1) {
		if (!fs1.good())
			throw;
		setParser(fs1, fs1.peek());
	}
	template<typename U> void setParser(U &data, char marker) {
		if (marker == '@')
		   _parser = SequenceStreamParserPtr(new FastqStreamParser(data));
		else if (marker == '>')
		   _parser = SequenceStreamParserPtr(new FastaStreamParser(data));
		else
			throw std::invalid_argument("Unknown file format");
	}
	SequenceStreamParserPtr getParser() {
		return _parser;
	}
	void setParser(istream &fs1, istream &fs2) {
		if (!fs2.good()) {
			setParser(fs1);
		} else {
			_parser = SequenceStreamParserPtr(new FastaQualStreamParser(fs1, fs2));
		}
	}
	void setParser(MmapSource &mmap1, MmapSource &mmap2) {
		_parser = SequenceStreamParserPtr(new FastaQualStreamParser(mmap1, mmap2));
	}

	bool isMmaped() {
		return _parser->isMmaped();
	}
	RecordPtr getStreamRecordPtr() const {
		return _parser->getStreamRecordPtr();
	}
	RecordPtr getStreamQualRecordPtr() const {
		return _parser->getStreamQualRecordPtr();
	}
	MmapSource getMmap() const {
		return _parser->getMmap();
	}
	MmapSource getQualMmap() const {
		return _parser->getQualMmap();
	}
	const string &getLastName() const {
		return _parser->getName();
	}
	const string &getLastBases() const {
		return _parser->getBases();
	}
	const string &getLastQuals() const {
		return _parser->getQuals();
	}
	bool getLastMultiline() const {
		return _parser->isMultiline();
	}
	bool nextRead(RecordPtr &recordStart) {
		try {
			assert(recordStart == _parser->getStreamRecordPtr());
			RecordPtr endPtr = _parser->readRecord();
			if (_parser->getName().empty()) {
				recordStart = NULL;
				return false;
			} else {
				recordStart = endPtr;
				return true;
			}
		} catch (runtime_error &e) {
			stringstream error;
			error << e.what() << " in file '" << _path << "' at line "
								<< _parser->lineNumber() << " position:" << _parser->tellg() << " " \
								<< _parser->getName() << _parser->getBases() << _parser->getQuals() \
								<< " '" << _parser->getLineBuffer() << _parser->nextLine() << "'";
			throw runtime_error(error.str());
		}
	}
	bool nextRead(RecordPtr &recordStart, string &name, string &bases, string &quals, bool &isMultiline) {
		bool passed = nextRead(recordStart);
		if (passed) {
		  name = getLastName();
		  bases = getLastBases();
		  quals = getLastQuals();
		  isMultiline = getLastMultiline();
		}
		return passed;
	}
	bool nextRead(string &name, string &bases, string &quals) {
		try {
			_parser->readRecord();
			name = _parser->getName();
			if (name.empty())
				return false;

			bases = _parser->getBases();
			std::transform(bases.begin(), bases.end(), bases.begin(), ::toupper);

			quals = _parser->getQuals();

			if (quals.length() != bases.length())
				if (!(quals.length() == 1 && quals[0] == Read::REF_QUAL))
					throw runtime_error((string("Number of bases and quals not equal: ") + bases + " " + quals).c_str());
			return true;
		}

		catch (runtime_error &e) {
			stringstream error;
			error << e.what() << " in file '" << _path << "' at line "
					<< _parser->lineNumber() << " position:" << _parser->tellg() << " " \
					<< _parser->getName() << _parser->getBases() << _parser->getQuals() \
					<< " '" << _parser->getLineBuffer() << _parser->nextLine() << "'";
			throw runtime_error(error.str());
		}
	}

	unsigned long getFileSize() {
		if (_parser->isMmaped()) {
			return _parser->getMmapFileSize();
		} else {
		    return getFileSize(_ifs);
		}
	}
	static unsigned long getFileSize(string &filePath) {
		ifstream ifs(filePath.c_str());
		return getFileSize(ifs);
	}
	static unsigned long getFileSize(ifstream &ifs) {
		ifstream::streampos current = ifs.tellg();
		ifs.seekg(0, ios_base::end);
		unsigned long size = ifs.tellg();
		ifs.seekg(current);
		return size;
	}

	unsigned long getBlockSize(unsigned int numThreads) {
		return getFileSize() / numThreads;
	}

	inline unsigned long getPos() {
		return _parser->getPos();
	}

	void seekToNextRecord(unsigned long minimumPos) {
		_parser->seekToNextRecord(minimumPos);
	}
	int getType() const {
		return _parser->getType();
	}

public:

	class SequenceStreamParser {
	private:
		istream * _stream;
		unsigned long _line;
		unsigned long _pos;

	protected:
		char _marker;
		ReadFileReader::MmapSource _mmap;
		ReadFileReader::RecordPtr _lastPtr;
		bool _freeStream;
		mutable std::vector<string> _nameBuffer;
		mutable std::vector<string> _basesBuffer;
		mutable std::vector<string> _qualsBuffer;
		mutable std::vector<string> _lineBuffer;
		mutable std::vector<bool>   _isMultiline;

	public:
		// returns a termination pointer (the start of next record or lastByte+1 at end)
		virtual RecordPtr readRecord(RecordPtr recordPtr) const = 0;
		virtual RecordPtr readRecord() = 0;
		RecordPtr readRecord(RecordPtr recordPtr, string &name, string &bases, string &quals) const {
			RecordPtr end = readRecord(recordPtr);
			if (end != NULL) {
			  name = getName();
			  bases = getBases();
			  quals = getQuals();
			}
			return end;
		}

		static inline string &nextLine(string &buffer, RecordPtr &recordPtr) {
			return SequenceRecordParser::nextLine(buffer, recordPtr);
		}
		inline string &nextLine(string &buffer) {
			_line++;
			buffer.clear();
			getline(*_stream, buffer);
			_pos += buffer.length() + 1;
			return buffer;
		}
		inline string &nextLine() {
			int threadNum = omp_get_thread_num();
			nextLine(_lineBuffer[threadNum]);
			return _lineBuffer[threadNum];
		}

		SequenceStreamParser(istream &stream, char marker) :
			_stream(&stream), _line(0), _pos(0), _marker(marker), _mmap(), _lastPtr(NULL), _freeStream(false) {
			setBuffers();
		}
		SequenceStreamParser(ReadFileReader::MmapSource &mmap, char marker) :
		    _stream(NULL), _line(0), _pos(0), _marker(marker), _mmap(mmap), _lastPtr(NULL), _freeStream(false) {
			_stream = new MmapIStream(_mmap);
			_lastPtr = _mmap.data() + _mmap.size();
			_freeStream = true;
			setBuffers();
		}
		void setBuffers() {
			_nameBuffer.assign(OMP_MAX_THREADS, string());
			_basesBuffer.assign(OMP_MAX_THREADS, string());
			_qualsBuffer.assign(OMP_MAX_THREADS, string());
			_lineBuffer.assign(OMP_MAX_THREADS, string());
			_isMultiline.assign(OMP_MAX_THREADS, false);
		}

		virtual ~SequenceStreamParser() {
			if (_freeStream)
				free(_stream);
		}

		unsigned long lineNumber() {
			return _line;
		}
		unsigned long tellg() {
			return _stream->tellg();
		}
		void seekg(unsigned long pos) {
			_pos = pos;
			_stream->seekg(_pos);
		}
		bool endOfStream() {
			return _stream->eof();
		}
		int peek() {
			return _stream->peek();
		}

		string &getLineBuffer() const {
			int threadNum = omp_get_thread_num();
			return _lineBuffer[threadNum];
		}
		string &getName() const {
			int threadNum = omp_get_thread_num();
			return _nameBuffer[threadNum];
		}
		string &getBases() const {
			int threadNum = omp_get_thread_num();
			return _basesBuffer[threadNum];
		}
		string &getQuals() const {
			int threadNum = omp_get_thread_num();
			return _qualsBuffer[threadNum];
		}
		bool isMultiline() const {
			int threadNum = omp_get_thread_num();
			return _isMultiline[threadNum];
		}

		virtual string &readName() {
			int threadNum = omp_get_thread_num();
			nextLine(_nameBuffer[threadNum]);

			while (_nameBuffer[threadNum].length() == 0) // skip empty lines at end of stream
			{
				if (endOfStream()) {
					_nameBuffer[threadNum].clear();
					return _nameBuffer[threadNum];
				}
				nextLine(_nameBuffer[threadNum]);
			}

			if (_nameBuffer[threadNum][0] != _marker)
				throw runtime_error(
						(string("Missing name marker '") + _marker + "'").c_str());

			return _nameBuffer[threadNum].erase(0, 1);
		}


		virtual int getType() const = 0;

		unsigned long getPos() const {
			return _pos;
		}
		bool isMmaped() const {
			return _mmap.is_open();
		}
		unsigned long getMmapFileSize() const {
			return _mmap.size();
		}
		RecordPtr getStreamRecordPtr() const {
			if (_mmap.is_open()) {
				return _mmap.data() + getPos();
			} else {
				return NULL;
			}
		}
		MmapSource getMmap() const {
			return _mmap;
		}
		virtual MmapSource getQualMmap() const = 0;
		RecordPtr getLastRecordPtr() const {
			return _lastPtr;
		}
		virtual RecordPtr getStreamQualRecordPtr() const = 0;
		virtual RecordPtr getLastQualRecordPtr() const = 0;

		virtual void seekToNextRecord(unsigned long minimumPos) {
			int threadNum = omp_get_thread_num();
			// get to the first line after the pos
			if (minimumPos > 0) {
				seekg(minimumPos - 1);
				if (endOfStream())
					return;
				if (peek() == '\n') {
					seekg(minimumPos);
				} else
					nextLine();
			} else {
				seekg(0);
			}
			if (Options::getDebug()) {
#pragma omp critical
				{
					std::cerr << omp_get_thread_num() << " seeked to " << tellg() << std::endl;
				}
			}
			while ( (!endOfStream()) && _stream->peek() != _marker) {
				nextLine();
			}
			if (_marker == '@' && (!endOfStream())) {
				// since '@' is a valid quality character in a FASTQ (sometimes)
				// verify that the next line is not also starting with '@', as that would be the true start of the record
				unsigned long tmpPos = tellg();
				string current = _lineBuffer[threadNum];
				string tmp = nextLine();
				if (endOfStream() || _stream->peek() != _marker) {
					seekg(tmpPos);
					_lineBuffer[threadNum] = current;

					if (Options::getDebug()) {
					  std::cerr << "Correctly picked record break point to read fastq in parallel: "
					  << current << std::endl << tmp << " " << omp_get_thread_num() << " " << tellg() << std::endl;
					}

				} else {
					if (Options::getDebug()) {
					  std::cerr << "Needed to skip an extra line to read fastq in parallel: "
					  << current << std::endl << tmp << " " << omp_get_thread_num() << " " << tellg() << std::endl;
					}
				}
			}

		}

	};

	class FastqStreamParser: public SequenceStreamParser {

	public:
		FastqStreamParser(istream &s) :
			SequenceStreamParser(s, '@') {
		}
		FastqStreamParser(ReadFileReader::MmapSource &mmap) :
			SequenceStreamParser(mmap, '@') {

		}
		RecordPtr readRecord() {
			if (readName().empty()) {
				int threadNum = omp_get_thread_num();
				_basesBuffer[threadNum].clear();
				_qualsBuffer[threadNum].clear();
			} else {
				readBases();
				readQuals();
			}

			return getStreamRecordPtr();
		}
		RecordPtr readRecord(RecordPtr recordPtr) const {
			int threadNum = omp_get_thread_num();
			if (*(recordPtr++) != _marker) {
			    // skip the first character
				throw;
			}
			nextLine(_nameBuffer[threadNum], recordPtr);  // name
			nextLine(_basesBuffer[threadNum], recordPtr); // fasta
			nextLine(_lineBuffer[threadNum], recordPtr);  // qual name
			nextLine(_qualsBuffer[threadNum], recordPtr); // quals
			return recordPtr;
		}
		string &readBases() {
			int threadNum = omp_get_thread_num();
			nextLine(_basesBuffer[threadNum]);

			if (_basesBuffer[threadNum].empty() || _basesBuffer[threadNum].length()
					== _basesBuffer[threadNum].max_size())
				throw runtime_error("Missing or too many bases");

			string &qualName = nextLine();
			if (qualName.empty() || qualName[0] != '+')
				throw runtime_error((string("Missing '+' in fastq, got: ") + _basesBuffer[threadNum] + " " + qualName).c_str());

			_isMultiline[threadNum] = false;
			return _basesBuffer[threadNum];
		}

		string &readQuals() {
			int threadNum = omp_get_thread_num();
			nextLine(_qualsBuffer[threadNum]);
			if (Options::getIgnoreQual()) {
				_qualsBuffer[threadNum].assign(_qualsBuffer[threadNum].length(), Read::REF_QUAL);
			}
			return _qualsBuffer[threadNum];
		}
		int getType() const {
			return 0;
		}
		RecordPtr getStreamQualRecordPtr() const { return NULL; }
		RecordPtr getLastQualRecordPtr() const { return NULL; }
		MmapSource getQualMmap() const { return MmapSource(); }
	};

	class FastaStreamParser: public SequenceStreamParser {
		static const char fasta_marker = '>';

	public:

		FastaStreamParser(istream &s) :
			SequenceStreamParser(s, fasta_marker) {
		}
		FastaStreamParser(ReadFileReader::MmapSource &mmap) :
			SequenceStreamParser(mmap, fasta_marker) {
		}

		RecordPtr readRecord() {
			if (readName().empty()) {
				int threadNum = omp_get_thread_num();
				_basesBuffer[threadNum].clear();
				_qualsBuffer[threadNum].clear();
			} else {
				readBases();
			}
			return getStreamRecordPtr();
		}
		RecordPtr readRecord(RecordPtr recordPtr) const {
			int threadNum = omp_get_thread_num();
			if (*(recordPtr++) != _marker) {
				// skip the first character
				throw;
			}
			nextLine(_nameBuffer[threadNum], recordPtr);  // name
			_basesBuffer[threadNum].clear();
			_isMultiline[threadNum] = false;
			long count = 0;
			while (recordPtr < _lastPtr && *recordPtr != fasta_marker) {
				_basesBuffer[threadNum] += nextLine(_lineBuffer[threadNum], recordPtr);
				if (++count > 1)
					_isMultiline[threadNum] = true;
			}
			_qualsBuffer[threadNum].assign(_basesBuffer[threadNum].length(), Read::REF_QUAL);

			return recordPtr;
		}

		string &readBases() {
			int threadNum = omp_get_thread_num();
			string &bases = getBasesOrQuals();
			_qualsBuffer[threadNum].assign(bases.length(), Read::REF_QUAL);
			return bases;
		}


		virtual string &getBasesOrQuals() {
			int threadNum = omp_get_thread_num();
			_basesBuffer[threadNum].clear();
			_isMultiline[threadNum] = false;
			long count = 0;
			while (!endOfStream()) {
				nextLine();

				if (_lineBuffer[threadNum].empty())
					break;

				_basesBuffer[threadNum] += _lineBuffer[threadNum];
				if (++count > 1)
					_isMultiline[threadNum] = true;
				if (_basesBuffer[threadNum].size() == _basesBuffer[threadNum].max_size())
					throw runtime_error("Sequence/Qual too large to read");

				if (peek() == fasta_marker)
					break;
			}
			return _basesBuffer[threadNum];
		}
		int getType() const {
			return 1;
		}
		RecordPtr getStreamQualRecordPtr() const { return NULL; }
		RecordPtr getLastQualRecordPtr() const { return NULL; }
		MmapSource getQualMmap() const { return MmapSource(); }
	};

	class FastaQualStreamParser: public FastaStreamParser {
		FastaStreamParser _qualParser; // odd, but it works
		boost::unordered_map<RecordPtr, RecordPtr> translate;

	public:
		FastaQualStreamParser(istream &fastaStream, istream &qualStream) :
			FastaStreamParser(fastaStream), _qualParser(qualStream) {
		}
		FastaQualStreamParser(ReadFileReader::MmapSource &mmap, ReadFileReader::MmapSource &qualMmap) :
			FastaStreamParser(mmap), _qualParser(qualMmap) {
		}

		RecordPtr readRecord() {
			int threadNum = omp_get_thread_num();
			RecordPtr fastaPtr = getStreamRecordPtr();
     		RecordPtr qualPtr = _qualParser.getStreamRecordPtr();
     		translate[fastaPtr] = qualPtr;
			if (readName().empty()) {
				_basesBuffer[threadNum].clear();
				_qualsBuffer[threadNum].clear();
			} else {
			    readBases();
			    readQuals();
			    _isMultiline[threadNum] = _isMultiline[threadNum] || _qualParser.isMultiline();
			}
			return getStreamRecordPtr();
		}
		RecordPtr readRecord(RecordPtr recordPtr) const {
			int threadNum = omp_get_thread_num();
			RecordPtr qualPtr = translate.find(recordPtr)->second;
			FastaStreamParser::readRecord(recordPtr);
			_qualParser.readRecord( qualPtr );
			_qualsBuffer[threadNum] = _qualParser.getQuals();
			_isMultiline[threadNum] = _isMultiline[threadNum] || _qualParser.isMultiline();
			return recordPtr;
		}

		string &readName() {
			string &qualName = _qualParser.readName();
			string &name = FastaStreamParser::readName();
			if (name != qualName)
				throw runtime_error("fasta and quals have different names");
			return name;
		}

		string &readBases() {
			return getBasesOrQuals();
		}

		string &readQuals() {
			int threadNum = omp_get_thread_num();
			// odd, but it works
			string &qualInts = _qualParser.getBasesOrQuals();
			_qualsBuffer[threadNum] = SequenceRecordParser::convertQualIntsToChars(qualInts);
			return _qualsBuffer[threadNum];
		}
		int getType() const {
			return 1;
		}
		RecordPtr getStreamQualRecordPtr() const { return _qualParser.getStreamRecordPtr(); }
		RecordPtr getLastQualRecordPtr() const { return _qualParser.getLastRecordPtr(); }
		MmapSource getQualMmap() const { return _qualParser.getMmap(); }
	};


};

#endif
