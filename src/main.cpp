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
#include <optional>
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

auto print_usage(std::string_view exe) noexcept -> void
{
	std::cerr << "\nUsage: " << exe << " [--names]"
			  << " [--date]"
			  << " [--decks]"
			  << " [--duel-seed]"
			  << " [--duel-options]"
			  << "\n       " << std::string(exe.length(), ' ')
			  << " [--duel-msgs]"
			  << " [--duel-responses]"
			  << " REPLAY\n\n";
	std::cerr << "  --names\t\tPrint names of all the duelists.\n";
	std::cerr << "  --date\t\tPrint date of the replay (when the "
				 "duel started).\n";
	std::cerr << "  --decks\t\tPrint the decks of all duelists (same order as "
				 "names).\n";
	std::cerr << "  --duel-seed\t\tPrint the duel seed "
				 "(in hexadecimal).\n";
	std::cerr << "  --duel-options\tPrint the duel flags "
				 "(in hexadecimal).\n";
	std::cerr << "  --duel-msgs\t\tPrint all the parsed messages.\n";
	std::cerr << "  --duel-resps\t\tPrint all responses.\n";
	std::cerr << "  REPLAY\t\tReplay file to parse (required).\n";
}

struct ReadHeaderResult
{
	bool success{};
	ExtendedReplayHeader header{};
};

auto read_header(std::string_view exe, uint8_t const* buffer_data,
                 ReplayTypes magic) noexcept -> ReadHeaderResult
{
	ReadHeaderResult r{};
	auto& h = r.header;
	std::memcpy(&h.base, buffer_data, sizeof(ReplayHeader));
	if(h.base.type != magic)
	{
		std::cerr << exe << ": Not a yrp or yrpX file.\n";
		return r;
	}
	if(h.base.flags & REPLAY_EXTENDED_HEADER)
	{
		std::memcpy(&h, buffer_data, sizeof(ExtendedReplayHeader));
		if(h.header_version > ExtendedReplayHeader::latest_header_version)
		{
			std::cerr << exe << ": Replay version is too new.\n";
			return r;
		}
	}
	r.success = true;
	return r;
}

constexpr auto skip_duelists(uint32_t flags, uint8_t*& ptr) noexcept -> unsigned
{
	unsigned num_duelists = 0;
	if((flags & REPLAY_SINGLE_MODE) != 0U)
	{
		num_duelists += 2;
		ptr += 40U * num_duelists;
	}
	else
	{
		num_duelists += read<uint32_t>(ptr);
		ptr += 40U * num_duelists; // Duelists team 1.
		auto const t2c = read<uint32_t>(ptr);
		num_duelists += t2c;
		ptr += 40U * t2c; // Duelists team 2.
	}
	return num_duelists;
}

constexpr auto read_duel_flags(uint32_t flags, uint8_t*& ptr) noexcept
	-> uint64_t
{
	if((flags & REPLAY_64BIT_DUELFLAG) != 0U)
		return read<uint64_t>(ptr);
	else
		return static_cast<uint64_t>(read<uint32_t>(ptr));
}

} // namespace

auto main(int argc, char* argv[]) -> int
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	struct End
	{
		~End() { google::protobuf::ShutdownProtobufLibrary(); }
	} _;
	auto const exe = std::string_view{argv[0]};
	if(argc < 3)
	{
		std::cerr << exe << ": No input file or flags.\n";
		print_usage(exe);
		return EXIT_FAILURE;
	}
	auto const fn = std::string_view{argv[argc - 1]};
	std::fstream f(fn.data(), IOS_IN);
	if(!f.is_open())
	{
		std::cerr << exe << ": Could not open file '" << fn << "'.\n";
		return EXIT_FAILURE;
	}
	bool print_names_opt = false;
	bool print_date_opt = false;
	bool print_decks_opt = false;
	bool print_duel_seed_opt = false;
	bool print_duel_options_opt = false;
	bool print_duel_msgs_opt = false;
	bool print_duel_resps_opt = false;
	for(int a = 1; a < argc - 1; a++)
	{
		auto const arg = std::string_view{argv[a]};
		if(arg == "--names")
		{
			print_names_opt = true;
			continue;
		}
		if(arg == "--date")
		{
			print_date_opt = true;
			continue;
		}
		if(arg == "--decks")
		{
			print_decks_opt = true;
			continue;
		}
		if(arg == "--duel-seed")
		{
			print_duel_seed_opt = true;
			continue;
		}
		if(arg == "--duel-options")
		{
			print_duel_options_opt = true;
			continue;
		}
		if(arg == "--duel-msgs")
		{
			print_duel_msgs_opt = true;
			continue;
		}
		if(arg == "--duel-resps")
		{
			print_duel_resps_opt = true;
			continue;
		}
		std::cerr << "Unrecognized option '" << arg << "'.\n";
		print_usage(exe);
		return EXIT_FAILURE;
	}
	f.ignore(std::numeric_limits<std::streamsize>::max());
	if(static_cast<size_t>(f.gcount()) < sizeof(ExtendedReplayHeader))
	{
		std::cerr << exe << ": File too small.\n";
		return EXIT_FAILURE;
	}
	f.clear();
	auto [read_yrpx_success, yrpx_header] = [&]() -> ReadHeaderResult
	{
		std::vector<uint8_t> header_buffer(sizeof(ExtendedReplayHeader));
		f.seekg(0, std::ios_base::beg);
		f.read(reinterpret_cast<char*>(header_buffer.data()),
		       sizeof(ExtendedReplayHeader));
		return read_header(exe, header_buffer.data(), REPLAY_YRPX);
	}();
	if(!read_yrpx_success)
		return EXIT_FAILURE; // NOTE: Error printed by `read_header`.
	f.seekg((yrpx_header.base.flags & REPLAY_EXTENDED_HEADER) != 0
	            ? sizeof(ExtendedReplayHeader)
	            : sizeof(ReplayHeader),
	        std::ios_base::beg);
	auto pth_buf = decompress(exe, yrpx_header, f, yrpx_header.base.size);
	if(pth_buf.size() == 0U)
		return EXIT_FAILURE; // NOTE: Error printed by `decompress`.
	if(print_names_opt)
		print_names(yrpx_header.base.flags, pth_buf.data());
	if(print_date_opt)
		print_date(yrpx_header.base.seed);
	if(!print_duel_seed_opt && !print_duel_options_opt &&
	   !print_duel_msgs_opt && !print_duel_resps_opt)
		return EXIT_SUCCESS;
	uint64_t duel_flags{};
	auto ptr_to_msgs = [&]() -> uint8_t*
	{
		auto* ptr = pth_buf.data();
		skip_duelists(yrpx_header.base.flags, ptr);
		duel_flags = read_duel_flags(yrpx_header.base.flags, ptr);
		return ptr;
	}();
	std::optional<AnalyzeResult> analysis;
	if(print_duel_seed_opt || print_duel_msgs_opt || print_duel_resps_opt)
	{
		size_t buffer_size = pth_buf.size() - (ptr_to_msgs - pth_buf.data());
		analysis = analyze(exe, ptr_to_msgs, buffer_size);
		if(!analysis->success)
			return EXIT_FAILURE; // NOTE: Error printed by `analyze`.
	}
	std::optional<ExtendedReplayHeader> yrp_header;
	if(print_decks_opt || print_duel_seed_opt || print_duel_resps_opt)
	{
		assert(analysis.has_value());
		auto [read_yrp_success, header] =
			read_header(exe, analysis->old_replay_mode_buffer, REPLAY_YRP1);
		if(!read_yrp_success)
			return EXIT_FAILURE; // NOTE: Error printed by `read_header`.
		yrp_header = header;
	}
	if(print_decks_opt)
	{
		assert(yrp_header.has_value());
		auto* ptr_to_decks = analysis->old_replay_mode_buffer;
		ptr_to_decks += (yrp_header->base.flags & REPLAY_EXTENDED_HEADER) != 0
		                    ? sizeof(ExtendedReplayHeader)
		                    : sizeof(ReplayHeader);
		auto const num_duelists =
			skip_duelists(yrp_header->base.flags, ptr_to_decks);
		ptr_to_decks += sizeof(uint32_t) * 3; // starting_lp, etc...
		auto duel_flags_yrp =
			read_duel_flags(yrp_header->base.flags, ptr_to_decks);
		assert(duel_flags == duel_flags_yrp);
		using CodeVector = std::vector<uint32_t>;
		auto read_code_vector = [&ptr_to_decks](CodeVector& cv) noexcept
		{
			auto const size = read<uint32_t>(ptr_to_decks);
			for(unsigned i = 0; i < size; i++)
				cv.emplace_back(read<uint32_t>(ptr_to_decks));
		};
		std::vector<std::pair<CodeVector, CodeVector>> decks;
		CodeVector extra_cards;
		decks.reserve(num_duelists);
		for(auto i = num_duelists; i != 0; i--)
		{
			auto& d = decks.emplace_back();
			read_code_vector(d.first);  // Main deck
			read_code_vector(d.second); // Extra deck
		}
		read_code_vector(extra_cards);
		// Print decks + extra cards
		for(auto const& deck_pair : decks)
		{
			std::cout << "#main";
			for(auto code : deck_pair.first)
				std::cout << ' ' << code;
			std::cout << " #extra";
			for(auto code : deck_pair.second)
				std::cout << ' ' << code;
			std::cout << '\n';
		}
		std::cout << "#rules";
		for(auto code : extra_cards)
			std::cout << ' ' << code;
		std::cout << '\n';
	}
	if(print_duel_seed_opt)
	{
		assert(yrp_header.has_value());
		std::cout << std::hex;
		auto const& s = yrp_header->seed;
		std::cout << "Duel seed: 0x" << std::setw(16) << std::setfill('0')
				  << s[0] << '\'' << std::setw(16) << std::setfill('0') << s[1]
				  << '\'' << std::setw(16) << std::setfill('0') << s[2] << '\''
				  << std::setw(16) << std::setfill('0') << s[3] << '\n';
		std::cout << std::dec;
	}
	if(print_duel_options_opt)
	{
		assert(yrp_header.has_value());
		auto* ptr_to_opts =
			analysis->old_replay_mode_buffer + sizeof(ExtendedReplayHeader);
		skip_duelists(yrp_header->base.flags, ptr_to_opts);
		auto const starting_lp = read<uint32_t>(ptr_to_opts);
		auto const starting_draw_count = read<uint32_t>(ptr_to_opts);
		auto const draw_count_per_turn = read<uint32_t>(ptr_to_opts);
		std::cout << "Duel options: " << starting_lp << ' '
				  << starting_draw_count << ' ' << draw_count_per_turn << ' '
				  << duel_flags << '\n';
	}
	if(print_duel_msgs_opt)
	{
		assert(analysis.has_value());
		std::cout << analysis->duel_messages << '\n';
	}
	if(print_duel_resps_opt)
	{
		assert(yrp_header.has_value());
		// TODO
	}
	return EXIT_SUCCESS;
}
