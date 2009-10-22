// $Header: /repository/PI_annex/robsandbox/KoMer/src/TwoBitSequence.cpp,v 1.4 2009-10-22 01:39:43 cfurman Exp $
//

#include "TwoBitSequence.h"

static unsigned char compressBase(char base)
{
  switch (base) {
    case 'A' : return 0;
    case 'C' : return 1;
    case 'G' : return 2;
    case 'T' : return 3;
    default :  return 255;
  }
}

// initialize the singleton
TwoBitSequence TwoBitSequence::singleton = TwoBitSequence(); 
TwoBitSequence::TwoBitSequence()
{
	TwoBitSequence::initBitMasks();
	TwoBitSequence::initReverseComplementTable();
	TwoBitSequence::initBitShiftTable();
}

/* initialize bitMasks[]
  0: 00000001
  1: 00000010
  2: 00000100
  3: 00001000
  4: 00010000
  5: 00100000
  6: 01000000
  7: 10000000
  */
TwoBitEncoding TwoBitSequence::bitMasks[8];
void TwoBitSequence::initBitMasks() {
  TwoBitSequence::bitMasks[0] = 0x01;
  for(int i=1; i<8; i++)
    TwoBitSequence::bitMasks[i] = TwoBitSequence::bitMasks[i-1]<<1;
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
void TwoBitSequence::initReverseComplementTable() 
{
  for(TwoBitEncoding i=0; i<255; i++) {
  	TwoBitEncoding complement = ~i;
    TwoBitSequence::reverseComplementTable[i] = 0x00;
    for(int j=0; j<4; j++)
      TwoBitSequence::reverseComplementTable[i] |= (complement<<(7-2*j)) & TwoBitSequence::bitMasks[7-j];
    for(int j=4; j<8; j++)
       TwoBitSequence::reverseComplementTable[i] |= (complement>>(2*(j-4)+1)) & TwoBitSequence::bitMasks[7-j];
  }
}


/* initialize bit shift table
   TwoBitSequence::bitShiftTable
   
   every two-byte possibility (65536) x 3 entries (excluding trivial non-bit-shift)
   */
TwoBitEncoding TwoBitSequence::bitShiftTable[256*256*3];
void TwoBitSequence::initBitShiftTable()   
{
  unsigned short i=0;
  do {
  	const TwoBitEncoding *ptr = (TwoBitEncoding *) &i;
  	TwoBitSequence::bitShiftTable[i*3]   = ((*ptr)<<2 & 0xfc) | ((*(ptr+1))>>6 & 0x03);
  	TwoBitSequence::bitShiftTable[i*3+1] = ((*ptr)<<4 & 0xf0) | ((*(ptr+1))>>4 & 0x0f);
  	TwoBitSequence::bitShiftTable[i*3+2] = ((*ptr)<<6 & 0xc0) | ((*(ptr+1))>>2 & 0x3f);
  } while(++i != 0);
}

BaseLocationVectorType TwoBitSequence::compressSequence(const char *bases,  TwoBitEncoding *out)
{
  BaseLocationVectorType otherBases;
  SequenceLengthType offset = 0;
  while (bases[offset] != '\0') {
    TwoBitEncoding c = 0;
    for (int i = 6; i >= 0  && *bases; i-= 2) {
      TwoBitEncoding cbase = compressBase(bases[offset]);
      if (cbase == 255)
      {
         otherBases.push_back(BaseLocationType(bases[offset],offset));
         cbase = 0;
      }
      offset++;
      c |= cbase <<  i;
    }
    *out++ = c;
  }

  return otherBases;
}

void TwoBitSequence::uncompressSequence(const TwoBitEncoding *in , int num_bases, char *bases)
{
  static char btable[4] = { 'A','C','G','T'};
  while(num_bases) {
    for (int i = 6; i >= 0  && num_bases; i-= 2) {
      char base = btable[(*in >> i) & 3];
      *bases++ = base;
      num_bases--;
    }
    in++;
  }
  *bases = '\0';
}


std::string TwoBitSequence::getFasta(const TwoBitEncoding *in, SequenceLengthType length)
{
  char buffer[length+1];
  uncompressSequence(in,length, buffer);

  return std::string(buffer);
}

void TwoBitSequence::reverseComplement(const TwoBitEncoding *in, TwoBitEncoding *out, SequenceLengthType length)
{
  SequenceLengthType twoBitLength = (length+3)/4;
  out+=twoBitLength;
  for(SequenceLengthType i = 0; i<twoBitLength; i++)
    *out-- = TwoBitSequence::reverseComplementTable[*in++];
}


KmerSizer KmerSizer::singleton = KmerSizer(21,0);

//
// $Log: TwoBitSequence.cpp,v $
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
