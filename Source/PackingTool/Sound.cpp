#include "Sound.h"
#include "FileUtils.h"

#include <iostream>

using namespace std;

bool Sound::LoadSMP(const char* fileName)
{
	OPEN_OR_RETURN(fileName, nullptr);

	int fileLength = FileUtils::GetFileSize(fIn);
	m_buffer.resize(fileLength);
	fread(m_buffer.data(), 1, fileLength, fIn);
	fclose(fIn);
	return true;
}

bool Sound::SaveSMP(const char* fileName) const
{
	return Save(fileName, false);
}


bool Sound::LoadWAV(const char* fileName)
{
	OPEN_OR_RETURN(fileName, nullptr);

	ChunkHeader chunk;
	fread(&chunk, 1, sizeof(chunk), fIn);
	if (chunk.chunkID != ChunkID_RIFF)
	{
		cerr << "file '" << fileName << "' is not a supported .wav file: Missing RIFF." << endl;
		return false;
	}
	uint32_t wavId;
	fread(&wavId, 1, sizeof(wavId), fIn);
	if (wavId != ChunkID_WAVE)
	{
		cerr << "file '" << fileName << "' is not a supported .wav file: Missing WAVE tag." << endl;
		return false;
	}
	bool dataFound = false;
	// parse chunks until we find 'data'
	while (!feof(fIn))
	{
		fread(&chunk, 1, sizeof(chunk), fIn);
		if (chunk.chunkID == ChunkID_FORMAT)
		{
			HeaderWAVFormat format, referenceFormat;
			if (chunk.chunkSize < format.chunk.chunkSize)
			{
				cerr << "file '" << fileName << "' is not a supported .wav file: Unexpected length of format chunk." << endl;
				return false;
			}
			fread(&format.audioFormat, 1, format.chunk.chunkSize, fIn);
			// The rest is extensible WAV header -> ignore
			if (chunk.chunkSize > format.chunk.chunkSize)
			{
				FileUtils::SkipBytes(fIn, chunk.chunkSize - format.chunk.chunkSize);
			}

			if (format.bitsPerSample != referenceFormat.bitsPerSample
				|| format.blockAlign != referenceFormat.blockAlign
				|| abs(format.sampleRate - referenceFormat.sampleRate) > 500 // allow for a bit of tolerance
				|| format.byteRate != format.sampleRate // these must be identical
				|| format.bitsPerSample != referenceFormat.bitsPerSample
				)
			{
				cerr << "file '" << fileName << "' is not a supported .wav file: Format properties must be 8bit mono 11025Hz." << endl;
				return false;
			}
		}
		else if (chunk.chunkID == ChunkID_DATA)
		{			
			m_buffer.resize(chunk.chunkSize);
			fread(m_buffer.data(), 1, chunk.chunkSize, fIn);
			dataFound = true;
			break; // we are done!
		}
		else
		{
			// ignore bytes
			FileUtils::SkipBytes(fIn, chunk.chunkSize);
		}
	}

	fclose(fIn);

	if (!dataFound)
	{
		cerr << "file '" << fileName << "' is not a supported .wav file: Missing data chunk." << endl;
		return false;
	}

	return true;
}

bool Sound::SaveWAV(const char* fileName) const
{
	return Save(fileName, true);
}

bool Sound::Save(const char* fileName, bool asWav) const
{
	CREATE_OR_RETURN(fileName, nullptr);

	int size = static_cast<int>(m_buffer.size());
	if (asWav)
	{
		WAVHeader header;
		header.chunk.chunkSize = size + sizeof(header) - 8;
		header.dataChunk.chunkSize = size;
		fwrite(&header, sizeof(header), 1, fOut);
	}
	fwrite(m_buffer.data(), size, 1, fOut);
	fclose(fOut);
	return true;
}
