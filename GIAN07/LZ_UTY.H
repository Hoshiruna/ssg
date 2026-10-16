/*
 *   Bit stream writer and packfile compression
 *
 *   Reading and decompression is part of the game logic layer.
 */

#pragma once

#include "hatoyama/app/file.h"
#include "hatoyama/logic/buffer.h"

#include "ssg/internal/LZ.hpp"

struct BIT_DEVICE_WRITE : BIT_BUFFER_WRITE {
	bool Write(const char8_t *s) const;
};

bool FilWrite(
	const char8_t *s,
	std::span<BUFFER_BORROWED> files,
	std::unique_ptr<FILE_TIMESTAMPS> maybe_timestamps = nullptr
);
