/*
 * Copyright (c) 2021, Dylam De La Torre <dyxel04@gmail.com>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include <google/protobuf/stubs/common.h>

#include <cstring> // std::memcpy
#include <fstream>
#include <iostream>
#include <limits> // std::numeric_limits
#include <vector>

#include "LZMA/Alloc.h" // g_Alloc
#include "LZMA/LzmaDec.h"
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
		SizeT full_size = f_size - sizeof(ReplayHeader);
		if((header.flags & REPLAY_COMPRESSED) == 0)
		{
			std::vector<uint8_t> full(full_size);
			f.read(reinterpret_cast<char*>(full.data()), full_size);
			return full;
		}
		// Decompress.
		std::vector<uint8_t> uncomp(header.size);
		SizeT dest_len = uncomp.size();
		ELzmaStatus status;
		std::vector<uint8_t> comp_buf(full_size);
		f.read(reinterpret_cast<char*>(comp_buf.data()), full_size);
		if(LzmaDecode(uncomp.data(), &dest_len, comp_buf.data(), &full_size,
		              header.props, LZMA_PROPS_SIZE, LZMA_FINISH_ANY, &status,
		              &g_Alloc) != SZ_OK)
		{
			std::cerr << "yrp: Error uncompressing replay.\n";
			uncomp.clear();
		}
		return uncomp;
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
