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
auto read_replay_contents(std::string_view exe,
                          ExtendedReplayHeader const& header, std::istream& f,
                          size_t filesize) noexcept -> std::vector<uint8_t>
{
	auto header_size = (header.base.flags & REPLAY_EXTENDED_HEADER) != 0
	                       ? sizeof(ExtendedReplayHeader)
	                       : sizeof(ReplayHeader);
	f.seekg(0, std::ios_base::beg);
	f.ignore(header_size);
	const auto filesize_without_header = filesize - header_size;
	std::vector<uint8_t> pth_buf(filesize);
	f.read(reinterpret_cast<char*>(pth_buf.data()), filesize_without_header);
	if(static_cast<size_t>(f.gcount()) != filesize_without_header)
	{
		std::cerr << exe << ": Read error\n";
		return {};
	}
	if(header.base.flags & REPLAY_COMPRESSED)
	{
		pth_buf = decompress(exe, header, pth_buf.data(), pth_buf.size(),
		                     header.base.size);
		if(pth_buf.size() == 0U)
			return {}; // NOTE: Error printed by `decompress`.
	}
	else if(header.base.size != filesize)
	{
		std::cerr << exe << ": File size doesn't match header\n";
		return {};
	}
	return pth_buf;
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

constexpr auto read_until_decks(uint32_t flags, uint8_t*& ptr) noexcept
	-> unsigned
{
	auto const num_duelists = skip_duelists(flags, ptr);
	ptr += sizeof(uint32_t) * 3; // starting_lp, etc...
	read_duel_flags(flags, ptr);
	return num_duelists;
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
	const auto filesize = static_cast<size_t>(f.gcount());
	if(filesize < sizeof(ExtendedReplayHeader))
	{
		std::cerr << exe << ": File too small.\n";
		return EXIT_FAILURE;
	}
	f.clear();
	auto [read_yrpx_success, yrpx_header] = [&]() -> ReadHeaderResult
	{
		std::array<uint8_t, sizeof(ExtendedReplayHeader)> header_buffer{};
		f.seekg(0, std::ios_base::beg);
		f.read(reinterpret_cast<char*>(header_buffer.data()),
		       sizeof(ExtendedReplayHeader));
		return read_header(exe, header_buffer.data(), REPLAY_YRPX);
	}();
	if(!read_yrpx_success)
		return EXIT_FAILURE; // NOTE: Error printed by `read_header`.
	if((yrpx_header.base.flags & REPLAY_HAND_TEST) != 0)
	{
		std::cerr << exe << ": Replay is from hand test mode\n";
		return EXIT_FAILURE;
	}
	auto pth_buf = read_replay_contents(exe, yrpx_header, f, filesize);
	if(pth_buf.empty())
		return EXIT_FAILURE;
	if(print_names_opt)
		print_names(yrpx_header.base.flags, pth_buf.data());
	if(print_date_opt)
		print_date(yrpx_header.base.seed);
	if(!print_decks_opt && !print_duel_seed_opt && !print_duel_options_opt &&
	   !print_duel_msgs_opt && !print_duel_resps_opt)
		return EXIT_SUCCESS;
	uint64_t duel_flags{};
	auto ptr_to_msgs = [&, &yrpx_header=yrpx_header]() -> uint8_t*
	{
		auto* ptr = pth_buf.data();
		skip_duelists(yrpx_header.base.flags, ptr);
		duel_flags = read_duel_flags(yrpx_header.base.flags, ptr);
		return ptr;
	}();
	std::optional<AnalyzeResult> analysis;
	bool const needs_yrp = print_decks_opt || print_duel_seed_opt ||
	                       print_duel_options_opt || print_duel_resps_opt;
	bool const needs_analysis = print_duel_msgs_opt || needs_yrp;
	if(auto core_version_major = (yrpx_header.base.version >> 16) & 0xff;
	   (needs_analysis || needs_yrp) && core_version_major < 10)
	{
		// with core version 10, the query for card race was changed from 32 bit
		// to 64 bit, breaking any message using it, drop such replays for now
		std::cerr << exe << ": Version of core used in this replay is too old.\n";
		return EXIT_FAILURE;
	}
	if(needs_analysis)
	{
		size_t buffer_size = pth_buf.size() - (ptr_to_msgs - pth_buf.data());
		analysis = analyze(exe, ptr_to_msgs, buffer_size);
		if(!analysis->success)
			return EXIT_FAILURE; // NOTE: Error printed by `analyze`.
	}
	std::optional<ExtendedReplayHeader> yrp_header;
	std::optional<std::vector<uint8_t>> decompressed_yrp_buffer;
	if(needs_yrp)
	{
		assert(analysis.has_value());
		if(analysis->old_replay_mode_buffer == nullptr)
		{
			std::cerr << exe << ": Replay doesn't have OLD_REPLAY_MODE.\n";
			return EXIT_FAILURE;
		}
		if(analysis->old_replay_mode_size < sizeof(ExtendedReplayHeader))
		{
			std::cerr << exe << ": Yrp buffer too small.\n";
			return EXIT_FAILURE;
		}
		auto [read_yrp_success, header] =
			read_header(exe, analysis->old_replay_mode_buffer, REPLAY_YRP1);
		if(!read_yrp_success)
			return EXIT_FAILURE; // NOTE: Error printed by `read_header`.
		yrp_header = header;
		auto header_size = (header.base.flags & REPLAY_EXTENDED_HEADER) != 0
		               ? sizeof(ExtendedReplayHeader)
		               : sizeof(ReplayHeader);
		analysis->old_replay_mode_buffer += header_size;
		analysis->old_replay_mode_size -= header_size;
		if((header.base.flags & REPLAY_COMPRESSED) != 0)
		{
			decompressed_yrp_buffer =
				decompress(exe, header, analysis->old_replay_mode_buffer,
			               analysis->old_replay_mode_size, header.base.size);
			analysis->old_replay_mode_buffer = decompressed_yrp_buffer->data();
			analysis->old_replay_mode_size = decompressed_yrp_buffer->size();
		}
		else if(analysis->old_replay_mode_size != header.base.size)
		{
			std::cerr << exe << ": Yrp buffer size doesn't match header\n";
			return EXIT_FAILURE;
		}
	}
	if(print_decks_opt)
	{
		assert(yrp_header.has_value());
		auto* ptr_to_decks = analysis->old_replay_mode_buffer;
		auto const num_duelists =
			read_until_decks(yrp_header->base.flags, ptr_to_decks);
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
		auto* ptr_to_opts = analysis->old_replay_mode_buffer;
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
		auto* ptr_to_resps = analysis->old_replay_mode_buffer;
		auto const num_duelists =
			read_until_decks(yrp_header->base.flags, ptr_to_resps);
		for(auto i = num_duelists; i != 0; i--)
		{
			ptr_to_resps += read<uint32_t>(ptr_to_resps) * sizeof(uint32_t);
			ptr_to_resps += read<uint32_t>(ptr_to_resps) * sizeof(uint32_t);
		}
		ptr_to_resps += read<uint32_t>(ptr_to_resps) * sizeof(uint32_t);
		// Read responses
		using Response = std::vector<uint8_t>;
		std::vector<Response> resps;
		decltype(ptr_to_resps) const sentry =
			analysis->old_replay_mode_buffer + analysis->old_replay_mode_size;
		while(sentry != ptr_to_resps)
		{
			assert(ptr_to_resps < sentry);
			auto const size = size_t{read<uint8_t>(ptr_to_resps)};
			assert(size != 0);
			auto& resp = resps.emplace_back(size, 0);
			assert(resp.data() != nullptr);
			std::memcpy(resp.data(), ptr_to_resps, size);
			ptr_to_resps += size;
		}
		// Print responses
		std::cout << "{\"responses\":[";
		auto* pad1 = "";
		for(auto const& resp : resps)
		{
			std::cout << pad1 << "[";
			pad1 = ",";
			auto* pad2 = "";
			for(auto const byte : resp)
			{
				std::cout << pad2 << uint32_t{byte};
				pad2 = ",";
			}
			std::cout << "]";
		}
		std::cout << "]}\n";
	}
	return EXIT_SUCCESS;
}
