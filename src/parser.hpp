/*
 * Copyright (c) 2024, Dylam De La Torre <dyxel04@gmail.com>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef ERP_PARSER_HPP
#define ERP_PARSER_HPP
#include <cstdint>
#include <string>
#include <string_view>

struct AnalyzeResult
{
	bool success;
	std::string duel_messages;
	uint8_t* old_replay_mode_buffer;
	size_t old_replay_mode_size;
};

auto analyze(std::string_view exe, uint8_t* buffer,
             size_t size) noexcept -> AnalyzeResult;

#endif // ERP_PARSER_HPP
