#pragma once

#include <vector>
#include <string>

class Sound
{
public:
	bool LoadSMP(const char* fileName);
	bool SaveSMP(const char* fileName) const;

	bool LoadWAV(const char* fileName);
	bool SaveWAV(const char* fileName) const;

private:
	bool Save(const char* fileName, bool asWav) const;

	static const uint32_t ChunkID_RIFF = 'FFIR';		// "RIFF"
	static const uint32_t ChunkID_WAVE = 'EVAW';		// "WAVE"
	static const uint32_t ChunkID_FORMAT = ' tmf';  // "fmt "
	static const uint32_t ChunkID_DATA = 'atad';  // "data"

	struct ChunkHeader
	{
		uint32_t chunkID = 0;
		uint32_t chunkSize = 0;
	};
	struct HeaderWAVFormat
	{
		ChunkHeader chunk = { ChunkID_FORMAT , 16 };
		int16_t audioFormat = 1;		// Audio format (1 for PCM)
		int16_t numChannels = 1;		// Number of channels (1 for mono, 2 for stereo, etc.)
		int32_t sampleRate = 11025;    // Sample rate (e.g., 44100)
		int32_t byteRate = 11025;      // Byte rate (sampleRate * numChannels * bitsPerSample / 8)
		int16_t blockAlign = 1;		// Block align (numChannels * bitsPerSample / 8)
		int16_t bitsPerSample = 8;		// Bits per sample (8, 16, etc.)
	};
	struct WAVHeader
	{
		ChunkHeader chunk = { ChunkID_RIFF , 0 };
		uint32_t wavId = ChunkID_WAVE;		// "WAVE"
		HeaderWAVFormat format;
		ChunkHeader dataChunk = { ChunkID_DATA, 0 };
	};

	std::vector<unsigned char> m_buffer;
};

