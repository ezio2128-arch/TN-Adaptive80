#include "../overlay/src/Features/SeasonalSnowPolicy.h"
#include <cassert>

using namespace LLSeasonalSnow;
int main()
{
    Policy p{};
    assert(Allows(Season::Winter, p));
    assert(!Allows(Season::Spring, p));
    assert(!Allows(Season::Summer, p));
    assert(!Allows(Season::Autumn, p));
    for (int m = 0; m < 12; ++m) {
        auto s = FromCalendarMonth(m);
        assert(s != Season::None);
        assert(Allows(s,p) == (m == 11 || m <= 1));
    }
    assert(FromCalendarMonth(-1) == Season::None);
    assert(FromGameSeasonCode(4) == Season::Autumn);
    assert(FromGameSeasonCode(5) == Season::None);
    p.autumn = true;
    assert(Allows(Season::Autumn, p));
    p.enabled = false;
    assert(Allows(Season::Spring, p));
    return 0;
}
