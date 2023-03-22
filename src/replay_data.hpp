/*
 * Copyright (c) 2022, Dylam De La Torre <dyxel04@gmail.com>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef ERP_REPLAY_DATA_HPP
#define ERP_REPLAY_DATA_HPP
#include <cstdint>

// NOTE: From src/Multirole/YGOPro/Replay.cpp @ DyXel/Multirole

enum ReplayTypes
{
	REPLAY_YRP1 = 0x31707279,
	REPLAY_YRPX = 0x58707279
};

enum ReplayFlags
{
	REPLAY_COMPRESSED = 0x1,
	REPLAY_TAG = 0x2,
	REPLAY_DECODED = 0x4,
	REPLAY_SINGLE_MODE = 0x8,
	REPLAY_LUA64 = 0x10,
	REPLAY_NEWREPLAY = 0x20,
	REPLAY_HAND_TEST = 0x40,
	REPLAY_DIRECT_SEED = 0x80,
	REPLAY_64BIT_DUELFLAG = 0x100,
	REPLAY_EXTENDED_HEADER = 0x200,
};

struct ReplayHeader
{
	uint32_t type;    // See ReplayTypes
	uint32_t version; // Unused atm, should be set to YGOPro::ClientVersion
	uint32_t flags;   // See ReplayFlags
	uint32_t seed; // Unix timestamp for YRPX and YRP with extended header. Core
	               // duel seed otherwise
	uint32_t size; // Uncompressed size of whatever is after this header
	uint32_t hash; // Unused
	uint8_t props[8]; // Used for LZMA compression (check their apis)
};

struct ExtendedReplayHeader
{
	static constexpr uint64_t latest_header_version = 1;
	ReplayHeader base;
	uint64_t header_version;
	uint64_t seed[4]; // 128 bit seed used for the Core duel
};

#endif // ERP_REPLAY_DATA_HPP
