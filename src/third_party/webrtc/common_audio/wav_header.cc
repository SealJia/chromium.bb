/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Based on the WAV file format documentation at
// https://ccrma.stanford.edu/courses/422/projects/WaveFormat/ and
// http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html

#include "webrtc/common_audio/wav_header.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include "webrtc/base/checks.h"
#include "webrtc/common_audio/include/audio_util.h"

namespace webrtc {
namespace {

struct ChunkHeader {
  uint32_t ID;
  uint32_t Size;
};
COMPILE_ASSERT(sizeof(ChunkHeader) == 8, chunk_header_size);

// We can't nest this definition in WavHeader, because VS2013 gives an error
// on sizeof(WavHeader::fmt): "error C2070: 'unknown': illegal sizeof operand".
struct FmtSubchunk {
  ChunkHeader header;
  uint16_t AudioFormat;
  uint16_t NumChannels;
  uint32_t SampleRate;
  uint32_t ByteRate;
  uint16_t BlockAlign;
  uint16_t BitsPerSample;
};
COMPILE_ASSERT(sizeof(FmtSubchunk) == 24, fmt_subchunk_size);
const uint32_t kFmtSubchunkSize = sizeof(FmtSubchunk) - sizeof(ChunkHeader);

struct WavHeader {
  struct {
    ChunkHeader header;
    uint32_t Format;
  } riff;
  FmtSubchunk fmt;
  struct {
    ChunkHeader header;
  } data;
};
COMPILE_ASSERT(sizeof(WavHeader) == kWavHeaderSize, no_padding_in_header);

}  // namespace

bool CheckWavParameters(int num_channels,
                        int sample_rate,
                        WavFormat format,
                        int bytes_per_sample,
                        uint32_t num_samples) {
  // num_channels, sample_rate, and bytes_per_sample must be positive, must fit
  // in their respective fields, and their product must fit in the 32-bit
  // ByteRate field.
  if (num_channels <= 0 || sample_rate <= 0 || bytes_per_sample <= 0)
    return false;
  if (static_cast<uint64_t>(sample_rate) > std::numeric_limits<uint32_t>::max())
    return false;
  if (static_cast<uint64_t>(num_channels) >
      std::numeric_limits<uint16_t>::max())
    return false;
  if (static_cast<uint64_t>(bytes_per_sample) * 8 >
      std::numeric_limits<uint16_t>::max())
    return false;
  if (static_cast<uint64_t>(sample_rate) * num_channels * bytes_per_sample >
      std::numeric_limits<uint32_t>::max())
    return false;

  // format and bytes_per_sample must agree.
  switch (format) {
    case kWavFormatPcm:
      // Other values may be OK, but for now we're conservative:
      if (bytes_per_sample != 1 && bytes_per_sample != 2)
        return false;
      break;
    case kWavFormatALaw:
    case kWavFormatMuLaw:
      if (bytes_per_sample != 1)
        return false;
      break;
    default:
      return false;
  }

  // The number of bytes in the file, not counting the first ChunkHeader, must
  // be less than 2^32; otherwise, the ChunkSize field overflows.
  const uint32_t max_samples =
      (std::numeric_limits<uint32_t>::max()
       - (kWavHeaderSize - sizeof(ChunkHeader))) /
      bytes_per_sample;
  if (num_samples > max_samples)
    return false;

  // Each channel must have the same number of samples.
  if (num_samples % num_channels != 0)
    return false;

  return true;
}

#ifdef WEBRTC_ARCH_LITTLE_ENDIAN
static inline void WriteLE16(uint16_t* f, uint16_t x) { *f = x; }
static inline void WriteLE32(uint32_t* f, uint32_t x) { *f = x; }
static inline void WriteFourCC(uint32_t* f, char a, char b, char c, char d) {
  *f = static_cast<uint32_t>(a)
      | static_cast<uint32_t>(b) << 8
      | static_cast<uint32_t>(c) << 16
      | static_cast<uint32_t>(d) << 24;
}

static inline uint16_t ReadLE16(uint16_t x) { return x; }
static inline uint32_t ReadLE32(uint32_t x) { return x; }
static inline std::string ReadFourCC(uint32_t x) {
  return std::string(reinterpret_cast<char*>(&x), 4);
}
#else
#error "Write be-to-le conversion functions"
#endif

static inline uint32_t RiffChunkSize(uint32_t bytes_in_payload) {
  return bytes_in_payload + kWavHeaderSize - sizeof(ChunkHeader);
}

static inline uint32_t ByteRate(int num_channels, int sample_rate,
                                int bytes_per_sample) {
  return static_cast<uint32_t>(num_channels) * sample_rate * bytes_per_sample;
}

static inline uint16_t BlockAlign(int num_channels, int bytes_per_sample) {
  return num_channels * bytes_per_sample;
}

void WriteWavHeader(uint8_t* buf,
                    int num_channels,
                    int sample_rate,
                    WavFormat format,
                    int bytes_per_sample,
                    uint32_t num_samples) {
  CHECK(CheckWavParameters(num_channels, sample_rate, format,
                           bytes_per_sample, num_samples));

  WavHeader header;
  const uint32_t bytes_in_payload = bytes_per_sample * num_samples;

  WriteFourCC(&header.riff.header.ID, 'R', 'I', 'F', 'F');
  WriteLE32(&header.riff.header.Size, RiffChunkSize(bytes_in_payload));
  WriteFourCC(&header.riff.Format, 'W', 'A', 'V', 'E');

  WriteFourCC(&header.fmt.header.ID, 'f', 'm', 't', ' ');
  WriteLE32(&header.fmt.header.Size, kFmtSubchunkSize);
  WriteLE16(&header.fmt.AudioFormat, format);
  WriteLE16(&header.fmt.NumChannels, num_channels);
  WriteLE32(&header.fmt.SampleRate, sample_rate);
  WriteLE32(&header.fmt.ByteRate, ByteRate(num_channels, sample_rate,
                                           bytes_per_sample));
  WriteLE16(&header.fmt.BlockAlign, BlockAlign(num_channels, bytes_per_sample));
  WriteLE16(&header.fmt.BitsPerSample, 8 * bytes_per_sample);

  WriteFourCC(&header.data.header.ID, 'd', 'a', 't', 'a');
  WriteLE32(&header.data.header.Size, bytes_in_payload);

  // Do an extra copy rather than writing everything to buf directly, since buf
  // might not be correctly aligned.
  memcpy(buf, &header, kWavHeaderSize);
}

bool ReadWavHeader(const uint8_t* buf,
                   int* num_channels,
                   int* sample_rate,
                   WavFormat* format,
                   int* bytes_per_sample,
                   uint32_t* num_samples) {
  WavHeader header;
  memcpy(&header, buf, kWavHeaderSize);

  // Parse needed fields.
  *format = static_cast<WavFormat>(ReadLE16(header.fmt.AudioFormat));
  *num_channels = ReadLE16(header.fmt.NumChannels);
  *sample_rate = ReadLE32(header.fmt.SampleRate);
  *bytes_per_sample = ReadLE16(header.fmt.BitsPerSample) / 8;
  const uint32_t bytes_in_payload = ReadLE32(header.data.header.Size);
  if (*bytes_per_sample <= 0)
    return false;
  *num_samples = bytes_in_payload / *bytes_per_sample;

  // Sanity check remaining fields.
  if (ReadFourCC(header.riff.header.ID) != "RIFF")
    return false;
  if (ReadFourCC(header.riff.Format) != "WAVE")
    return false;
  if (ReadFourCC(header.fmt.header.ID) != "fmt ")
    return false;
  if (ReadFourCC(header.data.header.ID) != "data")
    return false;

  if (ReadLE32(header.riff.header.Size) != RiffChunkSize(bytes_in_payload))
    return false;
  if (ReadLE32(header.fmt.header.Size) != kFmtSubchunkSize)
    return false;
  if (ReadLE32(header.fmt.ByteRate) !=
      ByteRate(*num_channels, *sample_rate, *bytes_per_sample))
    return false;
  if (ReadLE16(header.fmt.BlockAlign) !=
      BlockAlign(*num_channels, *bytes_per_sample))
    return false;

  return CheckWavParameters(*num_channels, *sample_rate, *format,
                            *bytes_per_sample, *num_samples);
}


}  // namespace webrtc
