/*
 * Copyright (c) 2022, Dylam De La Torre <dyxel04@gmail.com>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef ERP_PARSER_HPP
#define ERP_PARSER_HPP
#include <cstdint>
#include <string>
#include <string_view>

auto analyze(std::string_view exe, uint8_t* buffer, size_t size) noexcept
	-> std::string;

#endif // ERP_PARSER_HPP
