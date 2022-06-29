/*
 * Copyright (c) 2021, Dylam De La Torre <dyxel04@gmail.com>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include <array>
#include <cstring> // std::memcpy
#include <fstream>
#include <google/protobuf/stubs/common.h>
#include <iostream>
#include <limits> // std::numeric_limits
#include <lzma.h>
#include <vector>

#include "parser.hpp"

namespace
{

template<typename T>
constexpr auto read(uint8_t const*& ptr) -> T
{
	T value{};
	std::memcpy(&value, ptr, sizeof(T));
	ptr += sizeof(T);
	return value;
}

template<typename T>
constexpr auto read(uint8_t*& ptr) -> T
{
	return read<T>(const_cast<uint8_t const*&>(ptr));
}

// From Multirole/YGOPro/Replay.cpp
// -----------------------------------------------------------------------------

enum ReplayTypes
{
	REPLAY_YRP1 = 0x31707279,
	REPLAY_YRPX = 0x58707279
};

enum ReplayFlags
{
	REPLAY_COMPRESSED = 0x1,
	REPLAY_TAG = 0x2,
	REPLAY_DECODED = 0x4,
	REPLAY_SINGLE_MODE = 0x8,
	REPLAY_LUA64 = 0x10,
	REPLAY_NEWREPLAY = 0x20,
	REPLAY_HAND_TEST = 0x40,
	REPLAY_DIRECT_SEED = 0x80,
	REPLAY_64BIT_DUELFLAG = 0x100,
};

struct ReplayHeader
{
	uint32_t type;    // See ReplayTypes
	uint32_t version; // Unused atm, should be set to YGOPro::ClientVersion
	uint32_t flags;   // See ReplayFlags
	uint32_t seed;    // Unix timestamp for YRPX. Core duel seed for YRP
	uint32_t size;    // Uncompressed size of whatever is after this header
	uint32_t hash;    // Unused
	uint8_t props[8]; // Used for LZMA compression (check their apis)
};

// -----------------------------------------------------------------------------

} // namespace

auto main(int argc, char* argv[]) -> int
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	if(argc < 2)
	{
		std::cerr << "yrp: No input file, yrpX file expected.\n";
		return 1;
	}
	std::fstream f(argv[1], std::ios_base::binary | std::ios_base::in);
	if(!f.is_open())
	{
		std::cerr << "yrp: Could not open file.\n";
		return 2;
	}
	f.ignore(std::numeric_limits<std::streamsize>::max());
	auto const f_size = static_cast<size_t>(f.gcount());
	if(f_size < sizeof(ReplayHeader))
	{
		std::cerr << "yrp: File too small.\n";
		return 3;
	}
	f.clear();
	f.seekg(0, std::ios_base::beg);
	ReplayHeader header;
	f.read(reinterpret_cast<char*>(&header), sizeof(ReplayHeader));
	if(header.type != REPLAY_YRPX)
	{
		std::cerr << "yrp: Not a yrpX file.\n";
		return 4;
	}
	auto pth_buf = [&]() -> std::vector<uint8_t>
	{
		std::vector<uint8_t> ret(header.size);
		if((header.flags & REPLAY_COMPRESSED) == 0)
		{
			assert(f_size - sizeof(ReplayHeader) == header.size);
			f.read(reinterpret_cast<char*>(ret.data()), ret.size());
			return ret;
		}
		// Decompress.
		auto fail = [&ret](std::string_view e) -> std::vector<uint8_t>
		{
			std::cerr << "yrp: Error decompressing replay: " << e << ".\n";
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
			std::memcpy(ret_header.data(), header.props, 5U);
			for(unsigned i = 0U; i <= 3U; ++i)
				ret_header[i + 5U] = (header.size >> (8U * i)) & 0xFFU;
			return ret_header;
		}();
		lzma_stream stream = LZMA_STREAM_INIT;
		stream.avail_in = fake_header.size();
		stream.next_in = fake_header.data();
		stream.avail_out = header.size;
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
		std::array<uint8_t, 2048U> buffer;
		stream.next_in = buffer.data();
		for(;;)
		{
			f.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
			stream.avail_in = f.gcount();
			stream.next_in = buffer.data();
			auto const step = lzma_code(&stream, LZMA_RUN);
			if(step == LZMA_STREAM_END || f.eof())
				break;
			if(step != LZMA_OK)
			{
				if(step == LZMA_DATA_ERROR && stream.total_out == header.size)
					break; // Ignore error so long the total size matches.
				return fail("Stream decoding failed");
			}
		}
		if(stream.total_out != header.size)
			return fail("Total decompressed size mismatch");
		return ret;
	}();
	if(pth_buf.size() == 0U)
		return 5;
	auto ptr_to_msgs = [&]() -> uint8_t*
	{
		auto* ptr = pth_buf.data();
		if((header.flags & REPLAY_SINGLE_MODE) != 0U)
		{
			ptr += 40U * 2U; // Assume only 2 duelists.
		}
		else
		{
			ptr += read<uint32_t>(ptr) * 40U; // Duelists team 1.
			ptr += read<uint32_t>(ptr) * 40U; // Duelists team 2.
		}
		// Duel flags.
		if((header.flags & REPLAY_64BIT_DUELFLAG) != 0U)
			read<uint64_t>(ptr);
		else
			read<uint32_t>(ptr);
		return ptr;
	}();
	size_t msg_buffer_size = pth_buf.size() - (ptr_to_msgs - pth_buf.data());
	auto const replay_bin = analyze(ptr_to_msgs, msg_buffer_size);
	std::fstream{std::string{argv[1]} + ".pb",
	             std::ios_base::binary | std::ios_base::out}
		<< replay_bin;
	google::protobuf::ShutdownProtobufLibrary();
	return 0;
}
