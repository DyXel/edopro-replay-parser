/*
 * Copyright (c) 2022, Dylam De La Torre <dyxel04@gmail.com>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef ERP_DECOMPRESS_HPP
#define ERP_DECOMPRESS_HPP
#include <cstdint>
#include <istream>
#include <string_view>
#include <vector>

#include "replay_data.hpp"

auto decompress(std::string_view exe, ExtendedReplayHeader const& header,
                std::istream& is, size_t max_size) noexcept
	-> std::vector<uint8_t>;

#endif // ERP_DECOMPRESS_HPP
