/*
 * Copyright (c) 2022, Dylam De La Torre <dyxel04@gmail.com>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include <array>
#include <cstring> // std::memcpy
#include <fstream>
#include <google/protobuf/stubs/common.h>
#include <iostream>
#include <limits> // std::numeric_limits
#include <vector>

#include "decompress.hpp"
#include "parser.hpp"
#include "print_names.hpp"

namespace
{

#include "read.inl"

constexpr auto IOS_IN = std::ios_base::binary | std::ios_base::in;
constexpr auto IOS_OUT = std::ios_base::binary | std::ios_base::out;

} // namespace

auto main(int argc, char* argv[]) -> int
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	auto const exe = std::string_view(argv[0]);
	if(argc < 2)
	{
		std::cerr << exe << ": No input file, yrpX file expected.\n";
		return 1;
	}
	std::fstream f(argv[argc - 1], IOS_IN);
	bool print_names_opt = true;           // argv == "--names"sv
	if(!f.is_open())
	{
		std::cerr << exe << ": Could not open file.\n";
		return 2;
	}
	f.ignore(std::numeric_limits<std::streamsize>::max());
	auto const f_size = static_cast<size_t>(f.gcount());
	if(f_size < sizeof(ReplayHeader))
	{
		std::cerr << exe << ": File too small.\n";
		return 3;
	}
	f.clear();
	f.seekg(0, std::ios_base::beg);
	ReplayHeader header;
	f.read(reinterpret_cast<char*>(&header), sizeof(ReplayHeader));
	if(header.type != REPLAY_YRPX)
	{
		std::cerr << exe << ": Not a yrpX file.\n";
		return 4;
	}
	auto pth_buf = decompress(exe, header, f, header.size);
	if(pth_buf.size() == 0U)
		return 5;
	if(print_names_opt)
		print_names(header.flags, pth_buf.data());
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
	auto const replay_bin = analyze(exe, ptr_to_msgs, msg_buffer_size);
	std::fstream{std::string{argv[argc - 1]} + ".pb", IOS_OUT} << replay_bin;
	google::protobuf::ShutdownProtobufLibrary();
	return 0;
}
