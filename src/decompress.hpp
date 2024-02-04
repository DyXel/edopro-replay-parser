/*
 * Copyright (c) 2022, Dylam De La Torre <dyxel04@gmail.com>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef ERP_DECOMPRESS_HPP
#define ERP_DECOMPRESS_HPP
#include <cstdint>
#include <string_view>
#include <vector>

#include "replay_data.hpp"

auto decompress(std::string_view exe, ExtendedReplayHeader const& header,
                uint8_t const* replay_buffer, size_t replay_buffer_size,
                size_t max_size) noexcept
	-> std::vector<uint8_t>;

#endif // ERP_DECOMPRESS_HPP
