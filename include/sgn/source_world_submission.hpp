#pragma once
#include "sgn/session_outer_frame_services.hpp"
#include "sgn/polygon_submission_capacity.hpp"
#include "sgn/retail_world_material_loading.hpp"
#include "sgn/world_polygon_packet_binding.hpp"
#include "sgn/source_application_draw_start_win32.hpp"

namespace sgn::retail
{
// Invocation views of the same Core owners. No native packet/motion mirrors.
// Core admits the completed resource epoch, actual sink, serial and lifetime.
struct SourceWorldSubmissionContext
{
    FrontendState &frontend;
    SessionOuterFrameServices &frames;
    const WorldMaterialLoadingState &materials;
    PolygonSubmissionState &scratch;
    PolygonSubmissionSink &sink;
    SourceDrawStartupFailure &failure; // existing external error/termination receiver
    PolygonSubmissionBudget budget{desktop_polygon_submission_budget};
};
// Full433F40 call order. Every group's count/pointer is captured only at its
// reached source call. Source records are five DWORDs, never native packets.
void submit_source_world_polygons_433f40(SourceWorldSubmissionContext &);
// Individual source routes; zero count returns before reading the packet.
// Same owner/epoch admission as the parent; no geometry or UV advancement.
void submit_source_world_polygon_route(SourceWorldSubmissionContext &, PolygonSubmissionRoute,
                                       WorldPolygonPacketPointer, std::uint32_t count);
} // namespace sgn::retail
