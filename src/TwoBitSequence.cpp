// $Header: /repository/PI_annex/robsandbox/KoMer/src/TwoBitSequence.cpp,v 1.21 2010-04-21 00:33:20 regan Exp $
//

#include <cstring>
#include "TwoBitSequence.h"

void boost::intrusive_ptr_add_ref(TwoBitSequenceBase::_TwoBitEncodingPtr* r)
{
    r->increment();
}

void boost::intrusive_ptr_release(TwoBitSequenceBase::_TwoBitEncodingPtr* r)
{
    if (r->decrement())
    	TwoBitSequenceBase::_TwoBitEncodingPtr::release(r);
}

static unsigned char compressBase(char base) {
	switch (base) {
	case 'A':
		return 0;
	case 'C':
		return 1;
	case 'G':
		return 2;
	case 'T':
		return 3;
	case '\0':
		return 254;
	default:
		return 255;
	}
}

// initialize the singleton
TwoBitSequence TwoBitSequence::singleton = TwoBitSequence();
TwoBitSequence::TwoBitSequence() {
	TwoBitSequence::initReverseComplementTable();
	TwoBitSequence::initPermutationsTable();
}

/* initialize reverse complement table
 00000000 -> 11111111
 10000000 -> 11111110
 01000000 -> 11111101
 11000000 -> 11111100
 ...
 11111110 -> 10000000
 11111111 -> 00000000
 */
TwoBitEncoding TwoBitSequence::reverseComplementTable[256];
void TwoBitSequence::initReverseComplementTable() {
	for (int c = 0; c < 256; c++) {
		TwoBitEncoding i = c;
		TwoBitEncoding complement = ~i;
		reverseComplementTable[i] = ((complement << 6) & (0x03 << 6))
				| ((complement << 2) & (0x03 << 4)) | ((complement >> 2)
				& (0x03 << 2)) | ((complement >> 6) & (0x03 << 0));
	}
}

/*
 initialize permutations table - [256][12]
 AAAA -> CAAA GAAA TAAA  ACAA AGAA ATAA  AACA AAGA AATA  AAAC AAAG AAAT
 AAAC -> CAAC GAAC TAAC  ACAC ...
 ...
 TTTT -> ATTT CTTT GTTT  TATT ...
 */
TwoBitEncoding TwoBitSequence::permutations[256* 12 ];
void TwoBitSequence::initPermutationsTable() {
	for (int i = 0; i < 256; i++) {
		TwoBitEncoding tb = i;
		int j = 0;
		for (int baseIdx = 0; baseIdx < 4; baseIdx++) {
			for (int k = 0; k < 4; k++) {
				TwoBitEncoding newBase = k << (6 - baseIdx * 2);
				TwoBitEncoding baseMask = 0x03 << (6 - baseIdx * 2);
				TwoBitEncoding test = (tb & (~baseMask)) | newBase;
				if (test != i)
					permutations[i * 12 + j++] = test;
			}
		}
	}
}

// NULL for out is okay to just get markups
BaseLocationVectorType TwoBitSequence::compressSequence(const char *bases,
		TwoBitEncoding *out) {
	BaseLocationVectorType otherBases;
	SequenceLengthType offset = 0;
	while (bases[offset] != '\0') {
		TwoBitEncoding c = 0;
		for (int i = 6; i >= 0 && *bases; i -= 2) {
			TwoBitEncoding cbase = compressBase(bases[offset]);
			if (cbase == 254)
				break;

			if (cbase == 255) {
				otherBases.push_back(BaseLocationType(bases[offset], offset));
				cbase = 0;
			}
			offset++;
			c |= cbase << i;
		}
		if (out != NULL)
		  *out++ = c;
	}

	return otherBases;
}

void TwoBitSequence::uncompressSequence(const TwoBitEncoding *in,
		int num_bases, char *bases) {
	static char btable[4] = { 'A', 'C', 'G', 'T' };
	while (num_bases) {
		for (int i = 6; i >= 0 && num_bases; i -= 2) {
			char base = btable[(*in >> i) & 3];
			*bases++ = base;
			num_bases--;
		}
		in++;
	}
	*bases = '\0';
}

void TwoBitSequence::applyMarkup(std::string &bases,
	const BaseLocationVectorType &markupBases) {
	for (BaseLocationVectorType::const_iterator ptr = markupBases.begin(); ptr
			!= markupBases.end(); ptr++) {
                if (ptr->second >= bases.length())
                   break;
		bases[ptr->second] = ptr->first;
        }
}

void TwoBitSequence::applyMarkup(std::string &bases,
	SequenceLengthType markupBasesSize, const BaseLocationType *markupBases) {
	if (markupBasesSize > 0) {
		for (const BaseLocationType *ptr = markupBases; ptr < markupBases
				+ markupBasesSize && ptr->second < bases.length(); ptr++) {
                  if (ptr->second >= bases.length())
                      break;
			bases[ptr->second] = ptr->first;
                }
	}
}

std::string TwoBitSequence::getFasta(const TwoBitEncoding *in,
		SequenceLengthType length) {

	char buffer[length + 1];
	uncompressSequence(in, length, buffer);

	return std::string(buffer);
}

void TwoBitSequence::reverseComplement(const TwoBitEncoding *in,
		TwoBitEncoding *out, SequenceLengthType length) {
	SequenceLengthType twoBitLength = fastaLengthToTwoBitLength(length);

	out += twoBitLength;
	unsigned long bitShift = length % 4;

	for (SequenceLengthType i = 0; i < twoBitLength; i++)
		*(--out) = reverseComplementTable[*in++];

	if (bitShift > 0) {
		shiftLeft(out, out, twoBitLength, 4 - bitShift);
	}
}

void TwoBitSequence::shiftLeft(const void *twoBitIn, void *twoBitOut,
		SequenceLengthType twoBitLength, unsigned char shiftAmountInBases,
		bool hasExtraByte) {
	TwoBitEncoding *in = (TwoBitEncoding*) twoBitIn;
	TwoBitEncoding *out = (TwoBitEncoding*) twoBitOut;

	if (shiftAmountInBases == 0 && (in != out)) {
		memcpy(out, in, twoBitLength);
		return;
	}

	if (shiftAmountInBases > 3) {
		throw ;
	}

	in+= twoBitLength;
	out += twoBitLength;

	unsigned short buffer;
	if (hasExtraByte)
	buffer = *((unsigned short *)--in);
	else
	buffer = *(--in);

	const int shift = (8-shiftAmountInBases*2);
	for (SequenceLengthType i = 0; i < twoBitLength; i++) {
		TwoBitEncoding byte = ((buffer >> 8) | (buffer << 8)) >> shift;
		if (i < twoBitLength -1)
		buffer = *((unsigned short *)--in);
		*--out = byte;
	}

}

TwoBitSequenceBase::MarkupElementSizeType TwoBitSequence::getMarkupElementSize(const BaseLocationVectorType &markups) {
	long totalMarkupSize;
	return getMarkupElementSize(markups, totalMarkupSize);
}
TwoBitSequenceBase::MarkupElementSizeType TwoBitSequence::getMarkupElementSize(const BaseLocationVectorType &markups, long &totalMarkupSize) {
	MarkupElementSizeType markupElementSize(0,0);
	SequenceLengthType size = markups.size();
	if (size > 0) {
		if (size < 255 && markups[size-1].second < 255) {
			markupElementSize.first = sizeof(SequenceLengthType1);
			markupElementSize.second = sizeof(BaseLocationType1);
		} else if (size < 65535 && markups[size-1].second < 65535) {
			markupElementSize.first = sizeof(SequenceLengthType2);
			markupElementSize.second = sizeof(BaseLocationType2);
		} else {
			markupElementSize.first = sizeof(SequenceLengthType);
			markupElementSize.second = sizeof(BaseLocationType);
		}
	    totalMarkupSize = markupElementSize.first + markupElementSize.second * size;
	} else {
		totalMarkupSize = 0;
	}
	return markupElementSize;
}

void TwoBitSequence::permuteBase(const TwoBitEncoding *in, TwoBitEncoding *out1, TwoBitEncoding *out2, TwoBitEncoding *out3,
		SequenceLengthType sequenceLength, SequenceLengthType permuteBaseIdx) {

	assert(permuteBaseIdx < sequenceLength);

	SequenceLengthType bytes = fastaLengthToTwoBitLength(sequenceLength);
    SequenceLengthType permuteByteIdx = fastaLengthToTwoBitLength(permuteBaseIdx+1) - 1;

    // start off identical
    memcpy(out1, in, bytes);
    memcpy(out2, in, bytes);
    memcpy(out3, in, bytes);

    SequenceLengthType j = (permuteBaseIdx % 4) * 3;

    unsigned int arrayIdx = *(in + permuteByteIdx) * 12;

    TwoBitEncoding *ptr;

    ptr = out1 + permuteByteIdx;
    *ptr = permutations[ arrayIdx + j ];

    ptr = out2 + permuteByteIdx;
    *ptr = permutations[ arrayIdx + j + 1 ];

    ptr = out3 + permuteByteIdx;
    *ptr = permutations[ arrayIdx + j + 2 ];

}


//
// $Log: TwoBitSequence.cpp,v $
// Revision 1.21  2010-04-21 00:33:20  regan
// merged with branch to detect duplicated fragment pairs with edit distance
//
// Revision 1.20.2.1  2010-04-19 18:20:51  regan
// refactored base permutation
//
// Revision 1.20  2010-04-16 22:44:18  regan
// merged HEAD with changes for mmap and intrusive pointer
//
// Revision 1.19.2.3.2.2  2010-04-16 17:43:19  regan
// changed allocation / deallocation rules to hide counter
//
// Revision 1.19.2.3.2.1  2010-04-16 05:30:00  regan
// checkpoint.. broke it
//
// Revision 1.19.2.3  2010-04-14 03:51:19  regan
// checkpoint. compiles but segfaults
//
// Revision 1.19.2.2  2010-04-04 16:22:31  regan
// bugfix
//
// Revision 1.19.2.1  2010-04-04 15:29:28  regan
// migrated markup types and methods
//
// Revision 1.19  2010-03-02 14:27:00  regan
// reformatted
//
// Revision 1.18  2010-02-26 13:01:16  regan
// reformatted
//
// Revision 1.17  2010-01-13 23:34:59  regan
// made const class modifications
//
// Revision 1.16  2009-12-24 00:55:57  regan
// made const iterators
// fixed some namespace issues
// added support to output trimmed reads
//
// Revision 1.15  2009-11-06 16:59:11  regan
// added base substitution/permutations table and build function
//
// Revision 1.14  2009-11-02 18:24:29  regan
// *** empty log message ***
//
// Revision 1.13  2009-10-31 00:16:35  regan
// minor changes and optimizations
//
// Revision 1.12  2009-10-27 22:13:41  cfurman
// removed bit shift table
//
// Revision 1.11  2009-10-26 17:38:43  regan
// moved KmerSizer to Kmer.h
//
// Revision 1.10  2009-10-23 23:22:41  regan
// checkpoint
//
// Revision 1.9  2009-10-23 01:24:53  cfurman
// ReadSet test created
//
// Revision 1.8  2009-10-23 00:13:54  cfurman
// reverse complement now works
//
// Revision 1.7  2009-10-22 21:46:49  regan
// fixed ushort to ulong conversion problems
//
// Revision 1.6  2009-10-22 20:49:15  cfurman
// tests added
//
// Revision 1.5  2009-10-22 07:04:06  regan
// added a few unit tests
// minor refactor
//
// Revision 1.4  2009-10-22 01:39:43  cfurman
// bug fix in kmer.h
//
// Revision 1.3  2009-10-22 00:07:43  cfurman
// more kmer related classes added
//
// Revision 1.2  2009-10-21 18:44:20  regan
// checkpoint
//
// Revision 1.1  2009-10-21 06:51:34  regan
// bug fixes
// build lookup tables for twobitsequence
//
//
