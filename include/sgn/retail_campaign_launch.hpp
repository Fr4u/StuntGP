#pragma once

namespace sgn::retail
{
struct FrontendLaunchWorld;
// Full 0044BF10..0044C09B. Rebuild the actual 61BBE0 roster and 61BED0
// result owners, then call the complete recovered 43AF20 selection producer.
// No campaign phase, scene lifetime or display page is advanced here.
void launch_campaign_race_44bf10(FrontendLaunchWorld &);
} // namespace sgn::retail
