/*
 * Copyright (c) 2024, Dylam De La Torre <dyxel04@gmail.com>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "print_date.hpp"

#include <ctime>
#include <iomanip>
#include <iostream>

auto print_date(uint32_t timestamp) noexcept -> void
{
	auto const t = std::time_t{timestamp};
	std::cout << "Date: "
			  << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S\n");
}
