//
// Kmernator/src/KmerTrackingData.h
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

#ifndef _KMER_TRACKING_DATA_H
#define _KMER_TRACKING_DATA_H

#include <iomanip>
#include <boost/shared_ptr.hpp>

#include "config.h"
#include "Log.h"

class Extension {
public:
	enum ExtensionType {A = 0, C, G, T, N, X, MAX_EXTENSIONS};

	static char getBase(ExtensionType e) {
		static const char ExtensionToBase[MAX_EXTENSIONS] = {'A', 'C', 'G', 'T', 'N', 'X'};
		return ExtensionToBase[e];
	}
	static ExtensionType getExtension(int b) {
		ExtensionType e;
		switch(b) {
		case A : e = A; break;
		case C : e = C; break;
		case G : e = G; break;
		case T : e = T; break;
		case N : e = N; break;
		case X : e = X; break;
		default: e = MAX_EXTENSIONS;
		}
		return e;
	}
	static ExtensionType getReverseComplement(ExtensionType b) {
		assert(b < MAX_EXTENSIONS);
		if (b == A)
			return T;
		else if (b == C)
			return G;
		else if (b == G)
			return C;
		else if (b == T)
			return A;
		else
			return b;
	}
	static ExtensionType getReverseComplement(int b) {
		return getReverseComplement(getExtension(b));
	}

public:
	Extension() : _base(MAX_EXTENSIONS), _quality(0) {}
	Extension(char c, unsigned int quality) : _base(MAX_EXTENSIONS), _quality((unsigned char) quality) {
		switch(c) {
		case 'A' : case 'a' : _base = A; break;
		case 'C' : case 'c' : _base = C; break;
		case 'G' : case 'g' : _base = G; break;
		case 'T' : case 't' : _base = T; break;
		case 'X' : case 'x' : _base = X; break;
		case 'N' : case 'n' :
		default :  _base = N; break;
		}
		if (quality > 255 || quality < 0)
			LOG_THROW("Invalid Extension quality: " << quality << " type: " << _base << " " << c);
	}
	Extension(ExtensionType e, unsigned int quality) : _base(e), _quality((unsigned char) quality) {
		if (quality > 255 || quality < 0)
			LOG_THROW("Invalid Extension quality: " << quality << " type: " << _base);
	}
	Extension(const Extension &copy) {
		*this = copy;
	}
	~Extension() {}
	Extension &operator=(const Extension &copy) {
		_base = copy._base;
		_quality = copy._quality;
		return *this;
	}

	bool isValid() const {
		return _base < MAX_EXTENSIONS;
	}
	bool isBase() const {
		return _base <= T;
	}
	ExtensionType getExtension() const {
		return _base;
	}
	char getBase() const {
		return getBase(_base);
	}
	unsigned char getQuality() const {
		return _quality;
	}
	Extension getReverseComplement() const {
		Extension revComp = *this;
		revComp._base = getReverseComplement(_base);
		return revComp;
	}

private:
	ExtensionType _base;
	unsigned char _quality;
};


class ExtensionTracking {
public:
	enum DirectionType {Left = 0, Right, MAX_DIRECTIONS};

	static unsigned char &getMinQuality() {
		static unsigned char _minQuality = 20;
		return _minQuality;
	}
	static void setMinQuality(unsigned char minQuality) {
		getMinQuality() = minQuality;
	}

	ExtensionTracking() {
		reset();
	}
	ExtensionTracking(const ExtensionTracking &copy) {
		*this = copy;
	}
	ExtensionTracking &operator=(const ExtensionTracking &copy) {
		for(int j = 0; j < MAX_DIRECTIONS; j++)
			for(int i = 0; i < Extension::MAX_EXTENSIONS; i++)
				_extensionCounts[j][i] = copy._extensionCounts[j][i];
		return *this;
	}
	~ExtensionTracking() {}
	void reset() {
		for(int j = 0; j < MAX_DIRECTIONS; j++)
			for(int i = 0; i < Extension::MAX_EXTENSIONS; i++)
				_extensionCounts[j][i] = 0;
	}

	ExtensionTracking &operator+(const ExtensionTracking &other) {
		for(int j = 0; j < MAX_DIRECTIONS; j++)
			for(int i = 0; i < Extension::MAX_EXTENSIONS; i++)
				_extensionCounts[j][i] += other._extensionCounts[j][i];
		return *this;
	}

	void trackExtensions(Extension left, Extension right) {
		trackExtension(left, Left);
		trackExtension(right, Right);
	}
	void trackExtension(Extension ext, DirectionType dir) {
		if (ext.isValid() && ( ext.getQuality() >= getMinQuality() || !ext.isBase())) {
			unsigned int &extension = getExtensionCount(ext.getExtension(), dir);
#pragma omp atomic
			extension++;
		}
	}
	unsigned int &getExtensionCount(Extension::ExtensionType ext, DirectionType dir) {
		assert(dir < MAX_DIRECTIONS);
		return _extensionCounts[dir][ext];
	}

	std::string toTextValues() const {
		std::stringstream ss;
		for(int j = 0; j < MAX_DIRECTIONS; j++) {
			for(int i = 0; i < Extension::MAX_EXTENSIONS; i++) {
				if (i + j != 0)
					ss << " ";
				ss << _extensionCounts[j][i];
			}
		}
		ss << " " << 0;
		return ss.str();
	}
	ExtensionTracking getReverseComplement() const {
		ExtensionTracking rev;
		for(int i = 0; i < Extension::MAX_EXTENSIONS; i++) {
			rev._extensionCounts[Left][ Extension::getReverseComplement(i) ] = _extensionCounts[Right][i];
			rev._extensionCounts[Right][ Extension::getReverseComplement(i) ] = _extensionCounts[Left][i];
		}
		return rev;
	}

private:
	unsigned int _extensionCounts[MAX_DIRECTIONS][Extension::MAX_EXTENSIONS];
};

class ExtensionMessagePacket {
public:
	ExtensionMessagePacket() : _leftB('\0'), _rightB('\0'), _leftQ(0), _rightQ(0) {}
	ExtensionMessagePacket(const ExtensionMessagePacket &copy) {
		*this = copy;
	}
	~ExtensionMessagePacket() {}
	ExtensionMessagePacket &operator=(const ExtensionMessagePacket &copy) {
		_leftB = copy._leftB;
		_rightB = copy._rightB;
		_leftQ = copy._leftQ;
		_rightQ = copy._rightQ;
		return *this;
	}
	void reset() {
		_leftB = _rightB = '\0';
		_leftQ = _rightQ = 0;
	}

	Extension getLeft() const {
		return Extension(_leftB, _leftQ);
	}
	Extension getRight() const {
		return Extension(_rightB, _rightQ);
	}
	void setExtensions(Extension left, Extension right) {
		_leftB = left.getBase();
		_leftQ = left.getQuality();
		_rightB = right.getBase();
		_rightQ = right.getQuality();
	}
	void setExtensions(ExtensionTracking &extTrack) {
		int assignCount = 0;
		for(int dir = 0; dir < ExtensionTracking::MAX_DIRECTIONS; dir++) {
			for(int ext = 0; ext < Extension::MAX_EXTENSIONS; ext++) {
				Extension::ExtensionType e = (Extension::ExtensionType) ext;
				ExtensionTracking::DirectionType d = (ExtensionTracking::DirectionType) dir;
				if (extTrack.getExtensionCount(e, d) > 0) {
					if (d == ExtensionTracking::Left) {
						_leftB = Extension::getBase(e);
						_leftQ = ExtensionTracking::getMinQuality();
						assignCount++;
					} else {
						_rightB = Extension::getBase(e);
						_rightQ = ExtensionTracking::getMinQuality();
						assignCount++;
					}
				}
			}
		}
		assert(assignCount <= 2);
	}
protected:
	char _leftB, _rightB;
	unsigned char _leftQ, _rightQ;

};

class ExtensionTrackingInterface {
public:
	void trackExtensions(Extension left, Extension right) {}
	ExtensionTracking getExtensionTracking() const { return ExtensionTracking(); }
};

class TrackingDataSingleton;
class TrackingDataWithAllReads;

class TrackingData : public ExtensionTrackingInterface {
public:
	typedef Kmernator::UI16 CountType;
	typedef Kmernator::ReadSetSizeType ReadIdType;
	typedef Kmernator::SequenceLengthType PositionType;
	typedef float WeightType;

	static const CountType    MAX_COUNT    = MAX_UI16;
	static const ReadIdType   MAX_READ_ID  = MAX_READ_SET_SIZE;
	static const PositionType MAX_POSITION = MAX_SEQUENCE_LENGTH;

	class ReadPosition {
	public:
		ReadIdType readId;
		PositionType position;

		ReadPosition(ReadIdType _read = 0, PositionType _pos = 0) :
			readId(_read), position(_pos) {
		}
	};

	class ReadPositionWeight: public ReadPosition {
	public:
		WeightType weight;

		ReadPositionWeight(ReadIdType _read = 0, PositionType _pos = 0,
				WeightType _weight = 0.0) :
					ReadPosition(_read, _pos), weight(_weight) {
		}
	};
	typedef std::vector<ReadPositionWeight> ReadPositionWeightVector;


	static void resetGlobalCounters() {
		discarded = 0;
		maxCount = 0;
		maxWeightedCount = 0.0;
		totalCount = 0;
		totalWeight = 0.0;
	}
	static void resetForGlobals(CountType count) {
	}
	static void setGlobals(CountType count, WeightType weightedCount) {
#pragma omp atomic
		totalCount += count;
#pragma omp atomic
		totalWeight += weightedCount;

		if (count > maxCount) {
			maxCount = count;
		}
		if (weightedCount > maxWeightedCount) {
			maxWeightedCount = weightedCount;
		}
	}
	static inline bool isDiscard(WeightType weight) {
		if (weight > minimumWeight) {
			return false;
		} else {

#pragma omp atomic
			discarded++;

			return true;
		}
	}
	static double getErrorRate() {
		return 1.0 - (totalWeight / (double) (totalCount+discarded));
	}
	static inline bool useWeighted() {
		return useWeightedByDefault;
	}
	static void setUseWeighted(bool _useWeighted = true) {
		useWeightedByDefault = _useWeighted;
	}
	static void setMinimumWeight(WeightType _minimumWeight) {
		minimumWeight = _minimumWeight;
	}
	static inline WeightType getMinimumWeight() {
		return minimumWeight;
	}
	static void setMinimumDepth(CountType _minimumDepth) {
		minimumDepth = _minimumDepth;
	}
	static inline CountType getMinimumDepth() {
		return minimumDepth;
	}
	static void discard() {
#pragma omp atomic
		discarded++;
	}
	static unsigned long getDiscarded() {
		return discarded;
	}

	private:
	static WeightType minimumWeight;
	static CountType minimumDepth;
	static unsigned long discarded;
	static unsigned long totalCount;
	static double totalWeight;

	static CountType maxCount;
	static WeightType maxWeightedCount;
	static bool useWeightedByDefault;

protected:
	CountType count;
	WeightType weightedCount;


	public:
	TrackingData() :
		count(0),  weightedCount(0.0) {
	}

	void reset() {
		resetForGlobals(getCount());
		count = 0;
		weightedCount = 0.0;
	}
	inline bool operator==(const TrackingData &other) const {
		return getCount() == other.getCount();// && weightedCount == other.weightedCount;
	}
	inline bool operator<(const TrackingData &other) const {
		return getCount() < other.getCount();// || (count == other.count && weightedCount < other.weightedCount);
	}

	bool track(double weight, bool forward, ReadIdType readIdx = 0,
			PositionType readPos = 0) {
		assert(weight <= 1.0);

		if (isDiscard(weight))
			return false;

		if (count < MAX_COUNT) {
			{
#pragma omp atomic
				count++;
#pragma omp atomic
				weightedCount += weight;
			}

			setGlobals(getCount(), getWeightedCount());

			return true;
		} else
			return false;

	}

	inline unsigned long getCount() const {
		return count;
	}
	inline unsigned long getDirectionBias() const {
		return count / 2;
	}
	inline double getWeightedCount() const {
		return weightedCount;
	}
	inline double getAverageWeight() const {
		return weightedCount / count;
	}
	inline double getNormalizedDirectionBias() const {
		return 0.5;
	}

	ReadPositionWeightVector getEachInstance() const {
		//returns count entries from read -1, position 0
		ReadPositionWeight dummy1((ReadIdType) -1, 0, getWeightedCount()
				/ getCount());
		ReadPositionWeightVector dummy(getCount(), dummy1);
		return dummy;
	}

	std::string toString() const {
		std::stringstream ss;
		ss << count << ":" << std::fixed << std::setprecision(2)
		<< getNormalizedDirectionBias();
		ss << ':' << std::fixed << std::setprecision(2)
		<< ((double) weightedCount / (double) count);
		return ss.str();
	}

	// cast operator
	operator unsigned long() const {
		return getCount();
	}

	template<typename U>
	TrackingData &add(const U &other) {
		count += other.getCount();
		weightedCount += other.getWeightedCount();
		return *this;
	}
	template<typename U>
	TrackingData &operator=(const U &other) {
		count = other.getCount();
		weightedCount = other.getWeightedCount();
		return *this;
	};
};

class TrackingDataWithDirection: public TrackingData {
public:

protected:
	using TrackingData::count;
	using TrackingData::weightedCount;
	CountType directionBias;

public:
	TrackingDataWithDirection(): TrackingData(), directionBias(0) {}
	void reset() {
		TrackingData::reset();
		directionBias = 0;
	}

	bool track(double weight, bool forward, ReadIdType readIdx, PositionType readPos) {
		assert(weight <= 1.0);

		bool ret = TrackingData::track(weight,forward);

		if (ret) {
			if (forward) {
#pragma omp atomic
				directionBias++;
			}
		}
		return ret;
	}
	inline unsigned long getDirectionBias() const {
		return directionBias;
	}
	inline double getNormalizedDirectionBias() const {
		return (double) directionBias / (double) count;
	}

	template<typename U>
	TrackingDataWithDirection &add(const U &other) {
		TrackingData::add(other);
		directionBias += other.getDirectionBias();
		return *this;
	};
	template<typename U>
	TrackingDataWithDirection &operator=(const U &other) {
		count = other.getCount();
		weightedCount = other.getWeightedCount();
		directionBias = other.getDirectionBias();
		return *this;
	}

};

class TrackingDataWithLastRead: public TrackingDataWithDirection {
public:

protected:
	using TrackingDataWithDirection::count;
	using TrackingDataWithDirection::weightedCount;
	using TrackingDataWithDirection::directionBias;
	ReadPosition readPosition;

public:
	TrackingDataWithLastRead() :
		TrackingDataWithDirection(), readPosition() {
	}

	void reset() {
		TrackingDataWithDirection::reset();
		readPosition = ReadPosition();
	}
	bool track(double weight, bool forward, ReadIdType readIdx,
			PositionType readPos) {
		assert(weight <= 1.0);

		bool ret = TrackingDataWithDirection::track(weight, forward, readIdx, readPos);
		if (ret)
			readPosition = ReadPosition(readIdx, readPos);
		return ret;
	}
	ReadPositionWeightVector getEachInstance() const {
		//returns count entries from read -1, position 0
		ReadPositionWeight dummy1(readPosition.readId, readPosition.position,
				getWeightedCount() / getCount());
		ReadPositionWeightVector dummy(getCount(), dummy1);
		return dummy;
	}

	template<typename U>
	TrackingDataWithLastRead &add(const U &other) {
		TrackingDataWithDirection::add(other);
		ReadPositionWeightVector v = other.getEachInstance();
		if (v.size() > 0) {
			ReadPositionWeight &rpw = v[v.size()-1];
			readPosition.position = rpw.position;
			readPosition.readId = rpw.readId;
		}
		return *this;
	}
	template<typename U>
	TrackingDataWithDirection &operator=(const U &other) {
		count = other.getCount();
		weightedCount = other.getWeightedCount();
		directionBias = other.getDirectionBias();
		ReadPositionWeight v = other.getEachInstance()[0];
		readPosition.position = v.position;
		readPosition.readId = v.readId;
		return *this;
	}


};

class TrackingDataSingleton : public ExtensionTrackingInterface {
public:
	typedef TrackingData::PositionType PositionType;
	typedef TrackingData::ReadPositionWeight ReadPositionWeight;
	typedef TrackingData::ReadPositionWeightVector ReadPositionWeightVector;
	typedef TrackingData::CountType CountType;
	typedef TrackingData::WeightType WeightType;
	typedef TrackingData::ReadIdType ReadIdType;

protected:
	unsigned char _weight;

public:
	TrackingDataSingleton() : _weight(0) {}
	TrackingDataSingleton &operator=(const TrackingDataSingleton &other) {
		_weight = other._weight;
		return *this;
	}
	void reset() {
		TrackingData::resetForGlobals(getCount());
		_weight = 0;
	}
	inline bool operator==(const TrackingDataSingleton &other) const {
		return getCount() == other.getCount();
	}
	inline bool operator<(const TrackingDataSingleton &other) const {
		return getCount() < other.getCount();
	}
	bool track(double weight, bool forward, ReadIdType readIdx = 0, PositionType readPos = 0) {
		assert(weight <= 1.0);

		if (TrackingData::isDiscard(weight))
			return false;
		_weight = (unsigned char) ((weight * 254.0)) + 1;
		TrackingData::setGlobals(1, weight);
		return true;
	}

	inline unsigned long getCount() const {
		return _weight == 0 ? 0 : 1;
	}
	inline unsigned long getDirectionBias() const {
		return 0;
	}
	inline double getWeightedCount() const {
		return _weight == 0 ? 0.0 : (_weight - 1 ) / 254.0;
	}
	inline double getNormalizedDirectionBias() const {
		return 0.5;
	}

	ReadPositionWeightVector getEachInstance() const {
		//returns count entries from read -1, position 0
		ReadPositionWeight dummy1((ReadIdType) -1, 0, getWeightedCount()
				/ getCount());
		ReadPositionWeightVector dummy(getCount(), dummy1);
		return dummy;
	}

	// cast operator
	operator unsigned long() const {
		return getCount();
	}

	std::string toString() const {
		std::stringstream ss;
		ss << getCount() << ":" << std::fixed << std::setprecision(2)
		<< getNormalizedDirectionBias();
		ss << ':' << std::fixed << std::setprecision(2)
		<< ((double) getWeightedCount() / (double) getCount());
		return ss.str();
	}

};

class TrackingDataSingletonWithReadPosition : public ExtensionTrackingInterface {
public:
	typedef TrackingData::ReadPositionWeight ReadPositionWeight;
	typedef TrackingData::ReadPositionWeightVector ReadPositionWeightVector;
	typedef TrackingData::CountType CountType;
	typedef TrackingData::WeightType WeightType;
	typedef TrackingData::ReadIdType ReadIdType;
	typedef TrackingData::PositionType PositionType;

protected:
	ReadPositionWeight instance; // save space and store direction within sign of weight.

public:
	TrackingDataSingletonWithReadPosition() :
		instance(0, 0, 0.0) {
	}
	TrackingDataSingletonWithReadPosition &operator=(const TrackingDataSingletonWithReadPosition &other) {
		instance = other.instance;
		return *this;
	}

	void reset() {
		// destructor should *not* call this
		TrackingData::resetForGlobals(getCount());
		instance = ReadPositionWeight(0, 0, 0.0);
	}
	inline bool operator==(const TrackingDataSingletonWithReadPosition &other) const {
		return getCount() == other.getCount();// && weightedCount == other.weightedCount;
	}
	inline bool operator<(const TrackingDataSingletonWithReadPosition &other) const {
		return getCount() < other.getCount();// || (count == other.count && weightedCount < other.weightedCount);
	}

	bool track(double weight, bool forward, ReadIdType readIdx = 0,
			PositionType readPos = 0) {
		assert(weight <= 1.0);

		if (TrackingData::isDiscard(weight))
			return false;
#ifdef _USE_THREADSAFE_KMER
#pragma omp critical (TrackingDataSingletonWithReadPosition)
#endif
		instance = ReadPositionWeight(readIdx, readPos, forward ? weight : -1.0
				* weight);

		TrackingData::setGlobals(getCount(), getWeightedCount());

		return true;
	}

	inline unsigned long getCount() const {
		return instance.weight == 0.0 ? 0 : 1;
	}
	inline unsigned long getDirectionBias() const {
		return instance.weight > 0 ? 1 : 0;
	}
	inline double getWeightedCount() const {
		return instance.weight > 0 ? instance.weight : -1.0 * instance.weight;
	}
	inline double getNormalizedDirectionBias() const {
		return (double) getDirectionBias() / (double) getCount();
	}

	inline ReadPositionWeightVector getEachInstance() const {
		return ReadPositionWeightVector(1, instance);
	}

	inline ReadIdType getReadId() const {
		return instance.readId;
	}
	inline PositionType getPosition() const {
		return instance.position;
	}

	// cast operator
	operator unsigned long() const {
		return getCount();
	}

	std::string toString() const {
		std::stringstream ss;
		ss << getCount() << ":" << std::fixed << std::setprecision(2)
		<< getNormalizedDirectionBias();
		ss << ':' << std::fixed << std::setprecision(2)
		<< ((double) getWeightedCount() / (double) getCount());
		return ss.str();
	}


};

class TrackingDataWithAllReads : public ExtensionTrackingInterface {
public:
	typedef TrackingData::ReadPositionWeight ReadPositionWeight;
	typedef TrackingData::ReadPositionWeightVector ReadPositionWeightVector;
	typedef TrackingData::CountType CountType;
	typedef TrackingData::WeightType WeightType;
	typedef TrackingData::ReadIdType ReadIdType;
	typedef TrackingData::PositionType PositionType;

protected:
	boost::shared_ptr< ReadPositionWeightVector > instances;
	unsigned int directionBias;
	WeightType weightedCount; // for performance reasons

public:
	TrackingDataWithAllReads() :
		instances(new ReadPositionWeightVector()), directionBias(0), weightedCount(0.0) {
	}
	void reset() {
		TrackingData::resetForGlobals(getCount());
		instances->clear();
		directionBias = 0;
		weightedCount = 0.0;
	}
	inline bool operator==(const TrackingDataWithAllReads &other) const {
		return getCount() == other.getCount();// && weightedCount == other.weightedCount;
	}
	inline bool operator<(const TrackingDataWithAllReads &other) const {
		return getCount() < other.getCount();// || (count == other.count && weightedCount < other.weightedCount);
	}

	bool track(double weight, bool forward, ReadIdType readIdx = 0,
			PositionType readPos = 0) {
		assert(weight <= 1.0);
		assert(weight >= -1.0);
		if (forward)
			assert(weight >= 0.0);

		if (TrackingData::isDiscard(weight))
			return false;
		{
			if (forward) {
#pragma omp atomic
				directionBias++;
			}

			ReadPositionWeight rpw(readIdx, readPos, weight);

#ifdef _USE_THREADSAFE_KMER
#pragma omp critical (TrackingDataWithAllReads)
#endif
			{
				instances->push_back(rpw);
			}
#pragma omp atomic
			weightedCount += weight < 0.0 ? -weight : weight;
			assert(getCount() >= directionBias);
			assert(getCount() >= getWeightedCount());
		}
		TrackingData::setGlobals(getCount(), getWeightedCount());

		return true;
	}

	inline unsigned long getCount() const {
		return instances->size();
	}
	inline unsigned long getDirectionBias() const {
		return directionBias;
	}
	inline double getWeightedCount() const {
		return weightedCount;
	}
	double _getWeightedCount() const {
		double weightedCount = 0.0;
		for (ReadPositionWeightVector::const_iterator it = instances->begin(); it
		!= instances->end(); it++)
			if (it->weight < 0.0)
				weightedCount -= it->weight;
			else
				weightedCount += it->weight;
		return weightedCount;
	}
	inline double getNormalizedDirectionBias() const {
		return (double) getDirectionBias() / (double) getCount();
	}

	inline ReadPositionWeightVector getEachInstance() const {
		return *instances;
	}

	// cast operator
	operator unsigned long() const {
		return getCount();
	}

	std::string toString() const {
		std::stringstream ss;
		ss << getCount() << ":" << std::fixed << std::setprecision(2)
		<< getNormalizedDirectionBias();
		ss << ':' << std::fixed << std::setprecision(2)
		<< ((double) getWeightedCount() / (double) getCount());
		return ss.str();
	}
	void swap(TrackingDataWithAllReads &other) {
		instances.swap(other.instances);
		std::swap(directionBias, other.directionBias);
		std::swap(weightedCount, other.weightedCount);
	}
	TrackingDataWithAllReads &operator=(const TrackingDataWithAllReads &copy) {
		this->reset();
		return add(copy);
	}
	template<typename U>
	TrackingDataWithAllReads &operator=(const U &other) {
		this->reset();
		if (other.getCount() > 0) {
			add(other);
		}
		return *this;
	}
	TrackingDataWithAllReads &add(const TrackingDataWithAllReads &other) {
		if (other.getCount() > 0) {
			instances->insert(instances->end(), other.instances->begin(), other.instances->end());
			directionBias += other.getDirectionBias();
			weightedCount += other.getWeightedCount();
		}
		return *this;
	}

	template<typename U>
	TrackingDataWithAllReads &add(const U &other) {
		if (other.getCount() > 0) {
			ReadPositionWeightVector rpwv = other.getEachInstance();
			instances->insert(instances->end(), rpwv.begin(), rpwv.end());
			directionBias += other.getDirectionBias();
			weightedCount += other.getWeightedCount();
		}
		return *this;
	}

};

class TrackingDataSingletonWithReadPositionAndExtension : public TrackingDataSingletonWithReadPosition {
protected:
	using TrackingDataSingletonWithReadPosition::instance;
	ExtensionMessagePacket _extensionMsgPacket;

public:
	TrackingDataSingletonWithReadPositionAndExtension() : TrackingDataSingletonWithReadPosition(), _extensionMsgPacket() {}
	TrackingDataSingletonWithReadPositionAndExtension(const TrackingDataSingletonWithReadPositionAndExtension &copy) {
		*this = copy;
	}
	TrackingDataSingletonWithReadPositionAndExtension &operator=(const TrackingDataSingletonWithReadPositionAndExtension &copy) {
		*((TrackingDataSingletonWithReadPosition*)this) = (TrackingDataSingletonWithReadPosition&) copy;
		_extensionMsgPacket = copy._extensionMsgPacket;
		return *this;
	}
	~TrackingDataSingletonWithReadPositionAndExtension() {}
	void reset() {
		TrackingDataSingletonWithReadPosition::reset();
		_extensionMsgPacket.reset();
	}

	// override ExtenstionTrackingInterface noop methods
	void trackExtensions(Extension left, Extension right) {
		_extensionMsgPacket.setExtensions(left, right);
	}
	ExtensionTracking getExtensionTracking() const {
		ExtensionTracking extTrack;
		extTrack.trackExtensions(_extensionMsgPacket.getLeft(), _extensionMsgPacket.getRight());
		return extTrack;
	}

};
class TrackingDataWithAllReadsAndExtensions : public TrackingDataWithAllReads {
protected:
	using TrackingDataWithAllReads::instances;
	using TrackingDataWithAllReads::directionBias;
	using TrackingDataWithAllReads::weightedCount;
	ExtensionTracking _extensionTracking;

public:
	TrackingDataWithAllReadsAndExtensions() : TrackingDataWithAllReads(), _extensionTracking() {}
	TrackingDataWithAllReadsAndExtensions(const TrackingDataWithAllReadsAndExtensions &copy) {
		*this = copy;
	}
	TrackingDataWithAllReadsAndExtensions &operator=(const TrackingDataWithAllReadsAndExtensions &copy) {
		*((TrackingDataWithAllReads*)this) = (TrackingDataWithAllReads&) copy;
		_extensionTracking = copy._extensionTracking;
		return *this;
	}
	TrackingDataWithAllReadsAndExtensions &operator=(const TrackingDataSingletonWithReadPositionAndExtension &copy) {
		*((TrackingDataWithAllReads*)this) = (TrackingDataSingletonWithReadPosition&) copy;
		_extensionTracking = copy.getExtensionTracking();
		return *this;
	}

	~TrackingDataWithAllReadsAndExtensions() {}
	void reset() {
		TrackingDataWithAllReads::reset();
		_extensionTracking.reset();
	}

	// override ExtenstionTrackingInterface noop methods
	void trackExtensions(Extension left, Extension right) {
		_extensionTracking.trackExtensions(left,right);
	}
	ExtensionTracking getExtensionTracking() const {
		return _extensionTracking;
	}
	TrackingDataWithAllReadsAndExtensions &add(const TrackingDataWithAllReadsAndExtensions &other) {
		TrackingDataWithAllReads::add((TrackingDataWithAllReads&)other);
		_extensionTracking = _extensionTracking + other.getExtensionTracking();
		return *this;
	}

};

std::ostream &operator<<(std::ostream &stream, TrackingData &ob);
std::ostream &operator<<(std::ostream &stream, TrackingDataSingleton &ob);
std::ostream &operator<<(std::ostream &stream, TrackingDataWithAllReads &ob);

class WeightedExtensionMessagePacket : public ExtensionMessagePacket {
public:
	typedef TrackingData::WeightType WeightType;

	WeightedExtensionMessagePacket() : ExtensionMessagePacket(), _weight(0) {}
	WeightedExtensionMessagePacket(const WeightedExtensionMessagePacket &copy) {
		*this = copy;
	}
	~WeightedExtensionMessagePacket() {}
	WeightedExtensionMessagePacket &operator=(const WeightedExtensionMessagePacket &copy) {
		*((ExtensionMessagePacket*)this) = (ExtensionMessagePacket) copy;
		_weight = copy._weight;
		return *this;
	}
	WeightType getWeight() const {
		return _weight;
	}
	void setWeight(WeightType weight) {
		_weight = weight;
	}

protected:
	WeightType _weight;
};

class ExtensionTrackingData : public TrackingDataWithDirection {
public:
protected:
	ExtensionTracking _extensionTracking;

public:
	ExtensionTrackingData(): TrackingDataWithDirection(), _extensionTracking() {}
	ExtensionTrackingData(const ExtensionTrackingData &copy) {
		*this = copy;
	}
	~ExtensionTrackingData() {}

	void reset() {
		TrackingDataWithDirection::reset();
	}

	bool track(double weight, bool forward, ReadIdType readIdx, PositionType readPos) {
		assert(weight <= 1.0);

		bool ret = TrackingDataWithDirection::track(weight,forward,readIdx,readPos);

		return ret;
	}
	// override default methods
	void trackExtensions(Extension left, Extension right) {
		_extensionTracking.trackExtensions(left, right);
	}
	ExtensionTracking getExtensionTracking() const {
		return _extensionTracking;
	}

	template<typename U>
	ExtensionTrackingData &add(const U &other) {
		TrackingDataWithDirection::add(other);
		_extensionTracking = _extensionTracking + other.getExtensionTracking();

		return *this;
	};

	template<typename U>
	ExtensionTrackingData &operator=(const U &other) {
		*((TrackingDataWithDirection*) this) = other;
		_extensionTracking = other.getExtensionTracking();

		return *this;
	};

};
std::ostream &operator<<(std::ostream &stream, ExtensionTrackingData &ob);


class ExtensionTrackingDataSingleton : public TrackingDataSingleton {
public:
	ExtensionTrackingDataSingleton() : TrackingDataSingleton(), _extensionMsgPacket() {}
	ExtensionTrackingDataSingleton(const ExtensionTrackingDataSingleton &copy) {
		*this = copy;
	}
	~ExtensionTrackingDataSingleton() {}


	void reset() {
		TrackingDataSingleton::reset();
	}

	bool track(double weight, bool forward, ReadIdType readIdx, PositionType readPos) {
		assert(weight <= 1.0);

		bool ret = TrackingDataSingleton::track(weight,forward,readIdx,readPos);

		return ret;
	}
	// override default methods
	void trackExtensions(Extension left, Extension right) {
		_extensionMsgPacket.setExtensions(left, right);
	}
	ExtensionTracking getExtensionTracking() const {
		ExtensionTracking extTrack;
		extTrack.trackExtensions(_extensionMsgPacket.getLeft(), _extensionMsgPacket.getRight());
		return extTrack;
	}

	template<typename U>
	ExtensionTrackingDataSingleton &add(const U &other) {
		TrackingDataWithDirection::add(other);
		ExtensionTracking extensionTracking = other.getExtensionTracking();
		_extensionMsgPacket.setExtensions(extensionTracking);
		return *this;
	};

	template<typename U>
	ExtensionTrackingDataSingleton &operator=(const U &other) {
		*((TrackingDataWithDirection*) this) = other;
		ExtensionTracking extensionTracking = other.getExtensionTracking();
		_extensionMsgPacket.setExtensions(extensionTracking);
		return *this;
	};

protected:
	ExtensionMessagePacket _extensionMsgPacket;
};

std::ostream &operator<<(std::ostream &stream, ExtensionTrackingDataSingleton &ob);


template<typename T>
class TrackingDataMinimal : public ExtensionTrackingInterface {
public:
	typedef T DataType;
	typedef	typename TrackingData::CountType CountType;
	typedef typename TrackingData::WeightType WeightType;
	typedef typename TrackingData::ReadIdType ReadIdType;
	typedef typename TrackingData::PositionType PositionType;
	typedef typename TrackingData::ReadPositionWeightVector ReadPositionWeightVector;

private:
	DataType count;

public:
	TrackingDataMinimal() : count(0) {}
	void reset() {
		TrackingData::resetForGlobals(getCount());
		count = 0;
	}

	bool track(double weight, bool forward, ReadIdType readIdx, PositionType readPos)
	{
		assert(weight <= 1.0);
		if (TrackingData::isDiscard(weight)) {
			return false;
		}
		DataType test = (DataType) weight;
		// handle both integers and floats "properly"
		if (test < weight) {
			count += 1;
		} else {
			count += (DataType) weight;
		}
		TrackingData::setGlobals(getCount(), getWeightedCount());
		return true;
	}
	inline unsigned long getCount() const { unsigned long tmp = count; return (tmp<count) ? tmp+1 : tmp; }
	inline unsigned long getDirectionBias() const {return count / 2;}
	inline double getWeightedCount() const {return count;}
	inline double getNormalizedDirectionBias() const {return 0.5;}
	ReadPositionWeightVector getEachInstance() const
	{
		return ReadPositionWeightVector(0);
	}

	// cast operator
	operator unsigned long() const {
		return getCount();
	}

	std::string toString() const {
		std::stringstream ss;
		ss << getCount();
		return ss.str();
	}
	template<typename U>
	TrackingDataMinimal &operator=(const U &other) {
		double weight = other.getWeightedCount();
		DataType weightD = weight;
		if (weightD < weight)
			count = other.getCount();
		else
			count = weightD;
		return *this;
	};

	template<typename U>
	TrackingDataMinimal &add(const U &other) {
		double weight = other.getWeightedCount();
		DataType weightD = weight;
		if (weightD < weight)
			count += other.getCount();
		else
			count += weightD;
		return *this;
	};

};

typedef TrackingDataMinimal<unsigned char> TrackingDataMinimal1;
typedef TrackingDataMinimal<unsigned short> TrackingDataMinimal2;
typedef TrackingDataMinimal<unsigned int> TrackingDataMinimal4;
typedef TrackingDataMinimal<float> TrackingDataMinimal4f;
typedef TrackingDataMinimal<double> TrackingDataMinimal8;

template<typename T>
std::ostream &operator<<(std::ostream &stream, TrackingDataMinimal<T> &ob) {
	stream << ob.toString();
	return stream;
};


#endif
