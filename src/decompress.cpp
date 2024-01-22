/*
 * Copyright (c) 2022, Dylam De La Torre <dyxel04@gmail.com>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "decompress.hpp"

#include <array>
#include <cstring> // std::memcpy
#include <iostream>
#include <lzma.h>

auto decompress(std::string_view exe, ExtendedReplayHeader const& header,
                uint8_t* const& replay_buffer, size_t replay_buffer_size,
                size_t max_size) noexcept
	-> std::vector<uint8_t>
{
	std::vector<uint8_t> ret(max_size);
	// Decompress data in LZMA1 format.
	auto fail = [&](std::string_view e) -> std::vector<uint8_t>&
	{
		std::cerr << exe << ": Error decompressing replay: " << e << ".\n";
		ret.clear();
		return ret;
	};
	// We trick liblzma into believing that it is decompressing a .lzma
	// file as opposed to a raw stream from 7zip SDK by passing this crafted
	// header first to the decode stream. It consists of:
	//   1 byte   LZMA properties byte that encodes lc/lp/pb
	//   4 bytes  dictionary size as little endian uint32_t
	//   8 bytes  uncompressed size as little endian uint64_t
	// With the first 5 bytes corresponding to the "props" stored in the
	// replay header.
	auto const fake_header = [&]()
	{
		std::array<uint8_t, 1U + 4U + 8U> ret_header{};
		std::memcpy(ret_header.data(), header.base.props, 5U);
		for(unsigned i = 0U; i <= 3U; ++i)
			ret_header[i + 5U] = (header.base.size >> (8U * i)) & 0xFFU;
		return ret_header;
	}();
	lzma_stream stream = LZMA_STREAM_INIT;
	stream.avail_in = fake_header.size();
	stream.next_in = fake_header.data();
	stream.avail_out = max_size;
	stream.next_out = ret.data();
	if(lzma_alone_decoder(&stream, UINT64_MAX) != LZMA_OK)
		return fail("Unable to initialize decode stream");
	struct End // Close the stream regardless of how we end decompression.
	{
		lzma_stream& s;
		~End() { lzma_end(&s); }
	} _{stream};
	while(stream.avail_in != 0)
		if(lzma_code(&stream, LZMA_RUN) != LZMA_OK)
			return fail("Cannot decode header");
	if(stream.total_out != 0)
		return fail("Unexpected total decompressed size");
	stream.avail_in = replay_buffer_size;
	stream.next_in = replay_buffer;
	while(stream.avail_in != 0)
	{
		auto const step = lzma_code(&stream, LZMA_RUN);
		if(step == LZMA_STREAM_END)
			break;
		if(step != LZMA_OK)
		{
			if(step == LZMA_DATA_ERROR && stream.total_out == header.base.size)
				break; // Ignore error so long the total size matches.
			return fail("Stream decoding failed");
		}
	}
	if(stream.total_out != max_size)
		return fail("Total decompressed size mismatch");
	return ret;
}
