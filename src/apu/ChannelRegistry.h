#pragma once

// Channel registry: the single, data-driven source of truth for every NES
// channel the synth can allocate. Phase C.1 lists the current 2A03 + VRC6
// channels; future expansion chips (MMC5, FME7/5B, FDS, N163, VRC7) are added
// as rows here when their audio lands (C.2+). GPL-3.0.

#include <array>
#include <cstddef>

namespace nessy {

// One bit per expansion chip; Core2A03 is always enabled.
enum class ChipGroup { Core2A03, VRC6, MMC5, FME7, FDS, N163, VRC7, Count };

enum class ChannelKind { Square, Triangle, Saw, Noise, Dpcm, Wavetable, FM };

enum class ChannelRole { Melodic, Percussion };

// Which side of the pitch-split a melodic channel serves (None for percussion).
enum class SplitTier { Lead, Bass, None };

struct ChannelDesc {
  int         id;        // NES channel id understood by NessyAPU (0-based)
  ChipGroup   group;
  ChannelKind kind;
  ChannelRole role;
  SplitTier   splitTier;
};

// Production channel set (C.1 = today's 2A03 + VRC6 layout). Row order matters:
// the allocator iterates these in order, so this order reproduces the legacy
// channel priority {0,1,2,5,6,7}.
inline constexpr std::array<ChannelDesc, 8> kChannels = {{
    {0, ChipGroup::Core2A03, ChannelKind::Square,   ChannelRole::Melodic,    SplitTier::Lead},
    {1, ChipGroup::Core2A03, ChannelKind::Square,   ChannelRole::Melodic,    SplitTier::Lead},
    {2, ChipGroup::Core2A03, ChannelKind::Triangle, ChannelRole::Melodic,    SplitTier::Bass},
    {3, ChipGroup::Core2A03, ChannelKind::Noise,    ChannelRole::Percussion, SplitTier::None},
    {4, ChipGroup::Core2A03, ChannelKind::Dpcm,     ChannelRole::Percussion, SplitTier::None},
    {5, ChipGroup::VRC6,     ChannelKind::Square,   ChannelRole::Melodic,    SplitTier::Lead},
    {6, ChipGroup::VRC6,     ChannelKind::Square,   ChannelRole::Melodic,    SplitTier::Lead},
    {7, ChipGroup::VRC6,     ChannelKind::Saw,      ChannelRole::Melodic,    SplitTier::Bass},
}};

} // namespace nessy
