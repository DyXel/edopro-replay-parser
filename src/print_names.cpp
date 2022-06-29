/*
 * Copyright (c) 2022, Dylam De La Torre <dyxel04@gmail.com>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "print_names.hpp"

#include <codecvt>
#include <cstring> // std::memcpy
#include <iostream>
#include <locale>

#include "replay_data.hpp" // REPLAY_SINGLE_MODE

namespace
{

#include "read.inl"

// NOTE: From src/Multirole/YGOPro/StringUtils.cpp @ DyXel/Multirole

std::u16string buffer_to_utf16(const void* data,
                               std::size_t max_byte_count) noexcept
{
	std::u16string str{};
	if(max_byte_count == 0U)
		return str;
	const auto* p = reinterpret_cast<const uint8_t*>(data);
	str.reserve((max_byte_count / 2U) + 1U);
	for(const auto* tg = p + max_byte_count; p <= tg; p += sizeof(char16_t))
	{
		char16_t to_append{};
		std::memcpy(&to_append, p, sizeof(to_append));
		if(!to_append || to_append == L'\n' || to_append == L'\r')
			break;
		str.append(1U, to_append);
	}
	return str;
}

constexpr const char* ERROR_STR = "Invalid String";

#if defined(_MSC_VER) && _MSC_VER >= 1900 && _MSC_VER < 1920
using Wsc = std::wstring_convert<std::codecvt_utf8_utf16<int16_t>, int16_t>;
#else
using Wsc = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>;
#endif // defined(_MSC_VER) && _MSC_VER >= 1900 && _MSC_VER < 1920

using WscElem = Wsc::wide_string::value_type;

inline Wsc make_wsc() noexcept
{
	return Wsc{ERROR_STR};
}

std::string utf16_to_utf8(std::u16string_view str) noexcept
{
	const auto* p = reinterpret_cast<const WscElem*>(str.data());
	return make_wsc().to_bytes(p, p + str.size());
}

constexpr auto SEP_STR = ", ";
constexpr auto VS_STR = " vs. ";

} // namespace

auto print_names(uint32_t flags, uint8_t const* ptr) noexcept -> void
{
	auto print_one = [&]()
	{
		std::cout << utf16_to_utf8(buffer_to_utf16(ptr, 40U));
		ptr += 40U;
	};
	if((flags & REPLAY_SINGLE_MODE) != 0U)
	{
		print_one();
		std::cout << VS_STR;
		print_one();
		std::cout << '\n';
		return;
	}
	for(int i = 2; i != 0; --i)
	{
		for(uint32_t j = read<uint32_t>(ptr); j != 0; --j)
		{
			print_one();
			if(j != 1)
				std::cout << SEP_STR;
		}
		if(i == 2)
			std::cout << VS_STR;
	}
	std::cout << '\n';
}
