#pragma once
#include "sgn/retail_vehicle_storage.hpp"
#include "sgn/vehicle_trail_render.hpp"
#include "sgn/world_model_draw.hpp"

namespace sgn::retail
{
// Borrowed invocation of418180. All persistent fields remain in the existing
// Session/vehicle/model owners. quality is61297C, never simulation tick612CFC.
struct VehiclePresentationFrame
{
    VehicleStorage &vehicles;
    ModelResources &models;
    std::array<std::uint32_t, 6> &selected_lod;
    std::uint32_t &current_slot;
    const std::uint32_t &quality;
    std::uint32_t camera_vehicle_slot; // current camera+AC identity; unmatched sentinel is allowed
    const std::uint32_t &split_setting;
    LightCameraView camera;
    VehicleStripGeometry &strip;
    WorldModelDrawState &draws;
    ViewMatrixArena &matrices;
    VehicleMatrix &model_basis;
    const TrailProfileBank &profiles;
    SourceTrailPolygon &staging;
    SourceSpritePacketStorage &sprites;
    SourceScreenPolygonQueue screen; // borrowed live reader; no eager base lookup
    const ViewportState &viewport;
};

// Derived host observations only. Indices identify the actual appended queue
// records and remain valid until the original consumer/reseed changes them.
struct VehiclePresentationDraws
{
    std::optional<std::uint32_t> strip;
    std::array<std::optional<std::uint32_t>, 6> cars{}, shadows{};
};

// Full418180 composition, currently a candidate pending whole-caller oracle
// qualification. It publishes source queues; it does not consume or reset them.
// A consumer must handle both generated identities, including stripFFFFFFFE.
VehiclePresentationDraws prepare_vehicle_presentation_frame(VehiclePresentationFrame);
} // namespace sgn::retail
