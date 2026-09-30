#pragma once
#include "sgn/retail_campaign_score_navigation.hpp"
#include <memory>
#include <thread>

namespace sgn::retail
{
class RaceSessionPlatform;
// Application-owned adapter. Construction precedes runtime installation;
// each call resolves the one binding on the same platform. No playback owner.
class SourceMenuMusic final : public CampaignScoreMusic
{
    std::weak_ptr<RaceSessionPlatform> platform_;
    const std::thread::id thread_;

  public:
    explicit SourceMenuMusic(const std::shared_ptr<RaceSessionPlatform> &);
    void start(std::uint32_t requested, std::uint32_t loop) override;
};
} // namespace sgn::retail
