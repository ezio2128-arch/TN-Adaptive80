#pragma once

// Living Lands experimental Snow Cover policy.
// Portable C++20 logic. SoS exports SEASON_TYPE values: 0 none, 1 winter,
// 2 spring, 3 summer, 4 autumn.
namespace LLSeasonalSnow
{
    enum class Season : unsigned int { None = 0, Winter = 1, Spring = 2, Summer = 3, Autumn = 4 };

    struct Policy
    {
        bool enabled = true;
        bool followSeasonsOfSkyrim = true;
        bool winter = true;
        bool spring = false;
        bool summer = false;
        bool autumn = false;  // vanilla regional snow remains; no added shader snow
    };

    constexpr Season FromGameSeasonCode(unsigned int code)
    {
        return code >= 1 && code <= 4 ? static_cast<Season>(code) : Season::None;
    }

    // Calendar fallback applies only when the Seasons of Skyrim API is absent
    // or returns kNone. Does not honor custom calendar month mappings.
    constexpr Season FromCalendarMonth(int zeroBasedMonth)
    {
        if (zeroBasedMonth < 0 || zeroBasedMonth > 11) return Season::None;
        if (zeroBasedMonth == 11 || zeroBasedMonth <= 1) return Season::Winter;
        if (zeroBasedMonth <= 4) return Season::Spring;
        if (zeroBasedMonth <= 7) return Season::Summer;
        return Season::Autumn;
    }

    constexpr bool Allows(Season season, const Policy& policy)
    {
        if (!policy.enabled) return true;
        switch (season) {
        case Season::Winter: return policy.winter;
        case Season::Spring: return policy.spring;
        case Season::Summer: return policy.summer;
        case Season::Autumn: return policy.autumn;
        default: return false; // fail closed while season is unknown
        }
    }

    constexpr const char* Name(Season season)
    {
        switch (season) {
        case Season::Winter: return "Winter";
        case Season::Spring: return "Spring";
        case Season::Summer: return "Summer";
        case Season::Autumn: return "Autumn";
        default: return "Unknown";
        }
    }
}
