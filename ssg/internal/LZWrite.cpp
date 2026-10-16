/* Shared DAT compression and container encoding. MIT license, see LICENSE. */
#include "ssg/internal/LZ.hpp"

void BIT_BUFFER_WRITE::PutBit(uint8_t bit)
{
	auto& byte = ((bit_cursor == 0)
		? buffer.emplace_back(0x00)
		: buffer.back()
	);
	byte |= ((bit & 1) << (7 - bit_cursor));
	bit_cursor++;
}

void BIT_BUFFER_WRITE::PutBits(uint32_t bits,unsigned int bitcount)
{
	uint32_t mask = (1 << (bitcount - 1));
	for(decltype(bitcount) i = 0; i < bitcount; i++) {
		PutBit((bits & mask) != 0);
		mask >>= 1;
	}
}

BUFFER_GROWABLE Compress(BUFFER_BORROWED buffer)
{
	constexpr auto DICT_WINDOW = ((1 << LZSS_DICT_BITS) - LZSS_SEQ_MAX);

	BIT_BUFFER_WRITE device;
	fil_size_t in_i = 0;

	while(in_i < buffer.size()) {
		unsigned int seq_offset = 0;
		unsigned int seq_length = 0;
		unsigned int dict_i = ((in_i > DICT_WINDOW) ? (in_i - DICT_WINDOW) : 1);
		while((dict_i < in_i) && (seq_length < LZSS_SEQ_MAX)) {
			unsigned int length_new = 0;
			while(length_new < LZSS_SEQ_MAX) {
				if((in_i + length_new) >= buffer.size()) {
					break;
				}
				if(buffer[dict_i + length_new] != buffer[in_i + length_new]) {
					break;
				}
				length_new++;
			}
			if((length_new > seq_length)) {
				// ([seq_offset] == LZSS_DICT_MASK) would cause the in-file
				// offset to overflow back to 0, which is interpreted as the
				// "sentinel offset" that causes the original game to stop
				// decompressing.
				// (That's why offsets are 1-based to begin with.)
				if((dict_i & LZSS_DICT_MASK) != LZSS_DICT_MASK) {
					seq_length = length_new;
					seq_offset = dict_i;
				}
			}
			dict_i++;
		}
		if(seq_length < LZSS_SEQ_MIN) {
			auto literal = buffer[in_i];
			device.PutBit(true);
			device.PutBits(literal, 8);
			in_i++;
		} else {
			device.PutBit(false);
			device.PutBits((seq_offset + 1), LZSS_DICT_BITS);
			device.PutBits((seq_length - LZSS_SEQ_MIN), LZSS_SEQ_BITS);
			in_i += seq_length;
		}
	}

	// Write the sentinel offset
	device.PutBit(false);
	device.PutBits(0, LZSS_DICT_BITS);

	return device.buffer;
}

BUFFER_GROWABLE FilEncode(std::span<const BUFFER_BORROWED> files)
{
	if(files.size() > UINT32_MAX / sizeof(PBG_FILEINFO) - 1) return {};
	PBG_FILEHEAD head = { .n = static_cast<fil_no_t>(files.size()) };
	std::vector<PBG_FILEINFO> info(files.size());
	BUFFER_GROWABLE output(sizeof(head) + info.size() * sizeof(PBG_FILEINFO));
	fil_checksum_t sum = 0;
	for(size_t i = 0; i < files.size(); i++) {
		if(files[i].size() > UINT32_MAX || output.size() > UINT32_MAX) return {};
		auto compressed = Compress(files[i]);
		info[i].offset = static_cast<fil_size_t>(output.size());
		info[i].size_uncompressed = static_cast<fil_size_t>(files[i].size());
		info[i].checksum_compressed = FilChecksumAddFile(sum, info[i].offset,
			info[i].size_uncompressed, compressed);
		output.insert(output.end(), compressed.begin(), compressed.end());
	}
	head.sum = sum;
	std::memcpy(output.data(), &head, sizeof(head));
	std::memcpy(output.data() + sizeof(head), info.data(), info.size() * sizeof(PBG_FILEINFO));
	return output;
}
