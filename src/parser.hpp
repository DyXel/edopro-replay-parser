/*
 * Copyright (c) 2021, Dylam De La Torre <dyxel04@gmail.com>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef YRP_PARSER_HPP
#define YRP_PARSER_HPP
#include <cstdint>
#include <string>

auto analyze(uint8_t* buffer, size_t size) noexcept -> std::string;

#endif // YRP_PARSER_HPP
