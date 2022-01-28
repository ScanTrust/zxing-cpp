// -*- mode:c++; tab-width:2; indent-tabs-mode:nil; c-basic-offset:2 -*-
/*
 *  DecodedBitStreamParser.cpp
 *  zxing
 *
 *  Created by Christian Brunschen on 20/05/2008.
 *  Copyright 2008 ZXing authors All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <zxing/qrcode/decoder/DecodedBitStreamParser.h>
#include <zxing/common/CharacterSetECI.h>
#include <zxing/FormatException.h>
#include <zxing/common/StringUtils.h>
#include <iostream>
#ifndef NO_ICONV
#include <iconv.h>
#endif

// Required for compatibility. TODO: test on Symbian
#ifdef ZXING_ICONV_CONST
#undef ICONV_CONST
#define ICONV_CONST const
#endif

#ifndef ICONV_CONST
#define ICONV_CONST /**/
#endif

using namespace std;
using namespace zxing;
using namespace zxing::qrcode;
using namespace zxing::common;

const char DecodedBitStreamParser::ALPHANUMERIC_CHARS[] =
  { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B',
    'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', ' ', '$', '%', '*', '+', '-', '.', '/', ':'
  };

namespace {int GB2312_SUBSET = 1;}

bool DecodedBitStreamParser::append(std::string &result,
                                    string const& in,
                                    const char *src) {
  return append(result, (char const*)in.c_str(), in.length(), src);
}

bool DecodedBitStreamParser::append(std::string &result,
                                    const char *bufIn,
                                    size_t nIn,
                                    const char *src) {
#ifndef NO_ICONV
  if (nIn == 0) {
    return true;
  }

  iconv_t cd = iconv_open(StringUtils::UTF8, src);
  if (cd == (iconv_t)-1) {
    result.append((const char *)bufIn, nIn);
    return true;
  }

  const int maxOut = 4 * nIn + 1;
  char* bufOut = new char[maxOut];

  ICONV_CONST char *fromPtr = (ICONV_CONST char *)bufIn;
  size_t nFrom = nIn;
  char *toPtr = (char *)bufOut;
  size_t nTo = maxOut;

  while (nFrom > 0) {
    size_t oneway = iconv(cd, &fromPtr, &nFrom, &toPtr, &nTo);
    if (oneway == (size_t)(-1)) {
      iconv_close(cd);
      delete[] bufOut;
      return false;
//      throw ReaderException("error converting characters");
    }
  }
  iconv_close(cd);

  int nResult = maxOut - nTo;
  bufOut[nResult] = '\0';
  result.append((const char *)bufOut);
  delete[] bufOut;
#else
  result.append((const char *)bufIn, nIn);
#endif
  return true;
}

bool DecodedBitStreamParser::decodeHanziSegment(Ref<BitSource> bits_,
                                                string& result,
                                                int count) {
  BitSource& bits (*bits_);
  // Don't crash trying to read more bits than we have available.
  if (count * 13 > bits.available()) {
      return false;
//    throw FormatException();
  }

  // Each character will require 2 bytes. Read the characters as 2-byte pairs
  // and decode as GB2312 afterwards
  size_t nBytes = 2 * count;
  char* buffer = new char[nBytes];
  int offset = 0;
  while (count > 0) {
    // Each 13 bits encodes a 2-byte character
    int twoBytes = bits.readBits(13);
    int assembledTwoBytes = ((twoBytes / 0x060) << 8) | (twoBytes % 0x060);
    if (assembledTwoBytes < 0x003BF) {
      // In the 0xA1A1 to 0xAAFE range
      assembledTwoBytes += 0x0A1A1;
    } else {
      // In the 0xB0A1 to 0xFAFE range
      assembledTwoBytes += 0x0A6A1;
    }
    buffer[offset] = (char) ((assembledTwoBytes >> 8) & 0xFF);
    buffer[offset + 1] = (char) (assembledTwoBytes & 0xFF);
    offset += 2;
    count--;
  }

  if (!append(result, buffer, nBytes, StringUtils::GB2312)) {
      delete [] buffer;
      return false;
  }

//  try {
//    append(result, buffer, nBytes, StringUtils::GB2312);
//  } catch (ReaderException const& ignored) {
//    (void)ignored;
//    delete [] buffer;
//    throw FormatException();
//  }

  delete [] buffer;
  return true;
}

bool DecodedBitStreamParser::decodeKanjiSegment(Ref<BitSource> bits, std::string &result, int count) {
  // Each character will require 2 bytes. Read the characters as 2-byte pairs
  // and decode as Shift_JIS afterwards
  size_t nBytes = 2 * count;
  char* buffer = new char[nBytes];
  int offset = 0;
  while (count > 0) {
    // Each 13 bits encodes a 2-byte character

    int twoBytes = bits->readBits(13);
    int assembledTwoBytes = ((twoBytes / 0x0C0) << 8) | (twoBytes % 0x0C0);
    if (assembledTwoBytes < 0x01F00) {
      // In the 0x8140 to 0x9FFC range
      assembledTwoBytes += 0x08140;
    } else {
      // In the 0xE040 to 0xEBBF range
      assembledTwoBytes += 0x0C140;
    }
    buffer[offset] = (char)(assembledTwoBytes >> 8);
    buffer[offset + 1] = (char)assembledTwoBytes;
    offset += 2;
    count--;
  }
  if (!append(result, buffer, nBytes, StringUtils::SHIFT_JIS)) {
      delete[] buffer;
      return false;
  }
//  try {
//    append(result, buffer, nBytes, StringUtils::SHIFT_JIS);
//  } catch (ReaderException const& ignored) {
//    (void)ignored;
//    delete [] buffer;
//    throw FormatException();
//  }
  delete[] buffer;
  return true;
}

bool DecodedBitStreamParser::decodeByteSegment(Ref<BitSource> bits_,
                                               string& result,
                                               int count,
                                               CharacterSetECI* currentCharacterSetECI,
                                               ArrayRef< ArrayRef<char> >& byteSegments,
                                               Hashtable const& hints) {
  int nBytes = count;
  BitSource& bits (*bits_);
  // Don't crash trying to read more bits than we have available.
  if (count << 3 > bits.available()) {
      return false;
//    throw FormatException();
  }

  ArrayRef<char> bytes_ (count);
  char* readBytes = &(*bytes_)[0];
  for (int i = 0; i < count; i++) {
    readBytes[i] = (char) bits.readBits(8);
  }
  string encoding;
  if (currentCharacterSetECI == 0) {
    // The spec isn't clear on this mode; see
    // section 6.4.5: t does not say which encoding to assuming
    // upon decoding. I have seen ISO-8859-1 used as well as
    // Shift_JIS -- without anything like an ECI designator to
    // give a hint.
    encoding = StringUtils::guessEncoding(readBytes, count, hints);
  } else {
    encoding = currentCharacterSetECI->name();
  }

  if (!append(result, readBytes, nBytes, encoding.c_str())) {
      return false;
  }

//  try {
//    append(result, readBytes, nBytes, encoding.c_str());
//  } catch (ReaderException const& ignored) {
//    (void)ignored;
//    throw FormatException();
//  }
  byteSegments->values().push_back(bytes_);
  return true;
}

bool DecodedBitStreamParser::decodeNumericSegment(Ref<BitSource> bits, std::string &result, int count) {
  int nBytes = count;
  char* bytes = new char[nBytes];
  int i = 0;
  // Read three digits at a time
  while (count >= 3) {
    // Each 10 bits encodes three digits
    if (bits->available() < 10) {
      delete[] bytes;
      return false;
//      throw ReaderException("format exception");
    }
    int threeDigitsBits = bits->readBits(10);
    if (threeDigitsBits >= 1000) {
      delete[] bytes;
      return false;
//      ostringstream s;
//      s << "Illegal value for 3-digit unit: " << threeDigitsBits;
//      throw ReaderException(s.str().c_str());
    }
    bytes[i++] = ALPHANUMERIC_CHARS[threeDigitsBits / 100];
    bytes[i++] = ALPHANUMERIC_CHARS[(threeDigitsBits / 10) % 10];
    bytes[i++] = ALPHANUMERIC_CHARS[threeDigitsBits % 10];
    count -= 3;
  }
  if (count == 2) {
    if (bits->available() < 7) {
      delete[] bytes;
      return false;
//      throw ReaderException("format exception");
    }
    // Two digits left over to read, encoded in 7 bits
    int twoDigitsBits = bits->readBits(7);
    if (twoDigitsBits >= 100) {
      delete[] bytes;
      return false;
//      ostringstream s;
//      s << "Illegal value for 2-digit unit: " << twoDigitsBits;
//      throw ReaderException(s.str().c_str());
    }
    bytes[i++] = ALPHANUMERIC_CHARS[twoDigitsBits / 10];
    bytes[i++] = ALPHANUMERIC_CHARS[twoDigitsBits % 10];
  } else if (count == 1) {
    if (bits->available() < 4) {
      delete[] bytes;
      return false;
//      throw ReaderException("format exception");
    }
    // One digit left over to read
    int digitBits = bits->readBits(4);
    if (digitBits >= 10) {
      delete[] bytes;
      return false;
//      ostringstream s;
//      s << "Illegal value for digit unit: " << digitBits;
//      throw ReaderException(s.str().c_str());
    }
    bytes[i++] = ALPHANUMERIC_CHARS[digitBits];
  }
  append(result, bytes, nBytes, StringUtils::ASCII);
  delete[] bytes;
  return true;
}

bool DecodedBitStreamParser::toAlphaNumericChar(size_t value, char* out) {
  if (value >= sizeof(DecodedBitStreamParser::ALPHANUMERIC_CHARS)) {
      return false;
//    throw FormatException();
  }
  *out = ALPHANUMERIC_CHARS[value];
  return true;
}

bool DecodedBitStreamParser::decodeAlphanumericSegment(Ref<BitSource> bits_,
                                                       string& result,
                                                       int count,
                                                       bool fc1InEffect) {
  BitSource& bits (*bits_);
  ostringstream bytes;
  // Read two characters at a time
  while (count > 1) {
    if (bits.available() < 11) {
        return false;
//      throw FormatException();
    }
    int nextTwoCharsBits = bits.readBits(11);
    char byte;
    if (!toAlphaNumericChar(nextTwoCharsBits / 45, &byte)) {
        return false;
    }

    bytes << byte;

    if (!toAlphaNumericChar(nextTwoCharsBits % 45, &byte)) {
        return false;
    }
    bytes << byte;
//    bytes << toAlphaNumericChar(nextTwoCharsBits / 45);
//    bytes << toAlphaNumericChar(nextTwoCharsBits % 45);
    count -= 2;
  }
  if (count == 1) {
    // special case: one character left
    if (bits.available() < 6) {
        return false;
//      throw FormatException();
    }
    char byte;
    if (!toAlphaNumericChar(bits.readBits(6), &byte)) {
        return false;
    }
    bytes << byte;
  }
  // See section 6.4.8.1, 6.4.8.2
  string s = bytes.str();
  if (fc1InEffect) {
    // We need to massage the result a bit if in an FNC1 mode:
    ostringstream r;
    for (size_t i = 0; i < s.length(); i++) {
      if (s[i] != '%') {
        r << s[i];
      } else {
        if (i < s.length() - 1 && s[i + 1] == '%') {
          // %% is rendered as %
          r << s[i++];
        } else {
          // In alpha mode, % should be converted to FNC1 separator 0x1D
          r << (char)0x1D;
        }
      }
    }
    s = r.str();
  }
  append(result, s, StringUtils::ASCII);
  return true;
}

namespace {
  bool parseECIValue(BitSource& bits, int* out) {
    int firstByte = bits.readBits(8);
    if ((firstByte & 0x80) == 0) {
      // just one byte
      *out = firstByte & 0x7F;
      return true;
    }
    if ((firstByte & 0xC0) == 0x80) {
      // two bytes
      int secondByte = bits.readBits(8);
      *out = ((firstByte & 0x3F) << 8) | secondByte;
      return true;
    }
    if ((firstByte & 0xE0) == 0xC0) {
      // three bytes
      int secondThirdBytes = bits.readBits(16);
      *out = ((firstByte & 0x1F) << 16) | secondThirdBytes;
      return true;
    }
    return false;
  }
}

Ref<DecoderResult>
DecodedBitStreamParser::decode(ArrayRef<char> bytes,
                               Version* version,
                               ErrorCorrectionLevel const& ecLevel,
                               Hashtable const& hints) {
  Ref<BitSource> bits_ (new BitSource(bytes));
  BitSource& bits (*bits_);
  string result;
  result.reserve(50);
  ArrayRef< ArrayRef<char> > byteSegments (0);
//  try {
    CharacterSetECI* currentCharacterSetECI = 0;
    bool fc1InEffect = false;
    Mode* mode = 0;
    do {
      // While still another segment to read...
      if (bits.available() < 4) {
        // OK, assume we're done. Really, a TERMINATOR mode should have been recorded here
        mode = &Mode::TERMINATOR;
      } else {
          mode = &Mode::forBits(
                  bits.readBits(4)  // throw IllegalArg
                  ); // mode is encoded by 4 bits // throw ReaderException
      }
      if (mode != &Mode::TERMINATOR) {
        if ((mode == &Mode::FNC1_FIRST_POSITION) || (mode == &Mode::FNC1_SECOND_POSITION)) {
          // We do little with FNC1 except alter the parsed result a bit according to the spec
          fc1InEffect = true;
        } else if (mode == &Mode::STRUCTURED_APPEND) {
          if (bits.available() < 16) {
              return Ref<DecoderResult>();
//            throw FormatException();
          }
          // not really supported; all we do is ignore it
          // Read next 8 bits (symbol sequence #) and 8 bits (parity data), then continue
          bits.readBits(16); // throw IllegalArg
        } else if (mode == &Mode::ECI) {
          // Count doesn't apply to ECI
          int value;
          if (!parseECIValue(bits, &value)) {
              return Ref<DecoderResult>();
          }
          currentCharacterSetECI = CharacterSetECI::getCharacterSetECIByValue(value);
          if (currentCharacterSetECI == 0) {
              return Ref<DecoderResult>();
//            throw FormatException();
          }
        } else {
          // First handle Hanzi mode which does not start with character count
          if (mode == &Mode::HANZI) {
            //chinese mode contains a sub set indicator right after mode indicator
            int subset = bits.readBits(4); // throw IllegalArg
            int countHanzi = bits.readBits(mode->getCharacterCountBits(version)); // throw IllegalArg
            if (subset == GB2312_SUBSET) {
              if (!decodeHanziSegment(bits_, result, countHanzi)) {
                  return Ref<DecoderResult>();
              }
            }
          } else {
            // "Normal" QR code modes:
            // How many characters will follow, encoded in this mode?
            int count = bits.readBits(mode->getCharacterCountBits(version)); // throw IllegalArg
            if (mode == &Mode::NUMERIC) {
              if (!decodeNumericSegment(bits_, result, count)) {
                  return Ref<DecoderResult>();
              }
            } else if (mode == &Mode::ALPHANUMERIC) {
              if (!decodeAlphanumericSegment(bits_, result, count, fc1InEffect)) {
                  return Ref<DecoderResult>();
              }
            } else if (mode == &Mode::BYTE) {
              if (!decodeByteSegment(bits_, result, count, currentCharacterSetECI, byteSegments, hints)) {
                  return Ref<DecoderResult>();
              }
            } else if (mode == &Mode::KANJI) {
              if (!decodeKanjiSegment(bits_, result, count)) {
                  return Ref<DecoderResult>();
              }
            } else {
                return Ref<DecoderResult>();
//                throw FormatException();
            }
          }
        }
      }
    } while (mode != &Mode::TERMINATOR);
//  } catch (IllegalArgumentException const& iae) {
//    (void)iae;
//    // from readBits() calls
//    throw FormatException();
//  }
  
  return Ref<DecoderResult>(new DecoderResult(bytes, Ref<String>(new String(result)), byteSegments, (string)ecLevel));
}

