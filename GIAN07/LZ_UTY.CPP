/*
 *   Packfiles and compression
 *
 */

#include <SDL3/SDL_iostream.h>

#include "LZ_UTY.H"
#include "hatoyama/app/file.h"
#include "ssg/internal/LZ.hpp"
#include "hatoyama/logic/defer.h"

bool BIT_DEVICE_WRITE::Write(const char8_t *s) const
{
	return SDL_SaveFile(s, buffer.data(), buffer.size());
}

bool FilWrite(
	const char8_t *s,
	std::span<BUFFER_BORROWED> files,
	std::unique_ptr<FILE_TIMESTAMPS> maybe_timestamps
)
{
	const auto encoded = FilEncode(files);
	if(encoded.empty()) return false;
	auto *stream = SDL_IOFromFile(s, "wb");
	if(!stream) return false;
	defer(File_CloseWithTimestamps(std::move(stream), std::move(maybe_timestamps)));
	return SDL_MustWriteIO(stream, encoded.data(), encoded.size());
}
