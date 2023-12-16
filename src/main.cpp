/*
 * Copyright (c) 2024, Dylam De La Torre <dyxel04@gmail.com>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include <array>
#include <cstring> // std::memcpy
#include <fstream>
#include <google/protobuf/stubs/common.h>
#include <iomanip>
#include <iostream>
#include <limits> // std::numeric_limits
#include <vector>

#include "decompress.hpp"
#include "parser.hpp"
#include "print_date.hpp"
#include "print_names.hpp"
#include "replay_data.hpp"

namespace
{

#include "read.inl"

constexpr auto IOS_IN = std::ios_base::binary | std::ios_base::in;
constexpr auto IOS_OUT = std::ios_base::binary | std::ios_base::out;

} // namespace

auto main(int argc, char* argv[]) -> int
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	struct _
	{
		~_() { google::protobuf::ShutdownProtobufLibrary(); }
	} on_exit;
	auto const exe = std::string_view(argv[0]);
	if(argc < 2)
	{
		std::cerr << exe << ": No input file, yrpX file expected.\n";
		return 1;
	}
	auto const fn = std::string_view{argv[argc - 1]};
	std::fstream f(fn.data(), IOS_IN);
	if(!f.is_open())
	{
		std::cerr << exe << ": Could not open file " << fn << ".\n";
		return 2;
	}
	bool print_names_opt = true;         // argv == "--names"sv
	bool print_date_opt = true;          // argv == "--date"sv
	bool print_duel_options_opt = true;  // argv == "--duel-options"sv
	bool print_duel_messages_opt = true; // argv == "--msgs"sv
	f.ignore(std::numeric_limits<std::streamsize>::max());
	auto const f_size = static_cast<size_t>(f.gcount());
	if(f_size < sizeof(ReplayHeader))
	{
		std::cerr << exe << ": File too small.\n";
		return 3;
	}
	f.clear();
	f.seekg(0, std::ios_base::beg);
	ExtendedReplayHeader header{};
	f.read(reinterpret_cast<char*>(&header.base), sizeof(ReplayHeader));
	if(header.base.type != REPLAY_YRPX)
	{
		std::cerr << exe << ": Not a yrpX file.\n";
		return 4;
	}
	if(header.base.flags & REPLAY_EXTENDED_HEADER)
	{
		if(f_size < sizeof(ExtendedReplayHeader))
		{
			std::cerr << exe << ": File too small.\n";
			return 5;
		}
		f.seekg(0, std::ios_base::beg);
		f.read(reinterpret_cast<char*>(&header), sizeof(ExtendedReplayHeader));
		if(header.header_version > ExtendedReplayHeader::latest_header_version)
		{
			std::cerr << exe << ": Replay version is too new.\n";
			return 6;
		}
	}
	auto pth_buf = decompress(exe, header, f, header.base.size);
	if(pth_buf.size() == 0U)
		return 7; // NOTE: Error message printed by `decompress`.
	if(print_names_opt)
		print_names(header.base.flags, pth_buf.data());
	if(print_date_opt)
		print_date(header.base.seed);
	if(!print_duel_options_opt && !print_duel_messages_opt)
		return 0;
	uint64_t duel_flags{};
	auto ptr_to_msgs = [&]() -> uint8_t*
	{
		auto* ptr = pth_buf.data();
		if((header.base.flags & REPLAY_SINGLE_MODE) != 0U)
		{
			ptr += 40U * 2U; // Assume only 2 duelists.
		}
		else
		{
			ptr += read<uint32_t>(ptr) * 40U; // Duelists team 1.
			ptr += read<uint32_t>(ptr) * 40U; // Duelists team 2.
		}
		// Duel flags.
		if((header.base.flags & REPLAY_64BIT_DUELFLAG) != 0U)
			duel_flags = read<uint64_t>(ptr);
		else
			duel_flags = static_cast<uint64_t>(read<uint32_t>(ptr));
		return ptr;
	}();
	if(print_duel_options_opt)
	{
		std::cout << std::hex;
		// FIXME: Need to read YRP's seed instead of YRPX's. As YRPX's seed is
		// not set by the server.
		auto const& s = header.seed;
		std::cout << "Duel seed: 0x" << std::setw(16) << std::setfill('0')
				  << s[0] << '\'' << std::setw(16) << std::setfill('0') << s[1]
				  << '\'' << std::setw(16) << std::setfill('0') << s[2] << '\''
				  << std::setw(16) << std::setfill('0') << s[3] << '\n';
		std::cout << "Duel flags: 0x" << std::setw(16) << std::setfill('0')
				  << duel_flags << '\n';
		std::cout << std::dec;
	}
	size_t msg_buffer_size = pth_buf.size() - (ptr_to_msgs - pth_buf.data());
	auto const replay_bin = analyze(exe, ptr_to_msgs, msg_buffer_size);
	std::fstream{std::string{argv[argc - 1]} + ".pb", IOS_OUT} << replay_bin;
	return 0;
}
