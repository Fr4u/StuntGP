#pragma once
#include "sgn/retail_world_model_producer.hpp"
#include "sgn/retail_world_model_lighting.hpp"

namespace sgn::retail
{
// Synchronous borrowed owners; prepared is the existing624530 allocation.
// Source part lighting changes coordinates after packet emission. The packets
// retain their borrowed attributes and see these same writes at submission.
struct WorldModelLightingBinding
{
    PmdLightBank &lights;
    const LightViewSelection &selection;
    LightVertexCursor &prepared;
    const RetailAmbientLight &ambient;
    const ColorTables &colors;
    std::uint32_t quality, frame_stamp;
    std::uint32_t &stride; // sole62497C, retained when no shaded part is visited
};
enum class WorldModelLightingRoute
{
    diffuse,
    material_specular
};
// Full42A8C0 (diffuse) /42A990 (material_specular). Selected source LOD and
// previous descriptor64 stay in their existing owners. Only flags==1 render;
// flags02 bit4 enables shading and low2bits choose specular in42A990.
void produce_world_model_lit(ModelResource &, unsigned source_lod, const ModelTransform &outer, const CameraState &,
                             const ViewMatrixArena &, const CameraMatrix &shared_identity, const ProjectionParameters &,
                             bool clipped, WorldModelViewScratch &, WorldTransformedVertexCursor &,
                             WorldPolygonProducerList &opaque, WorldPolygonProducerList &translucent,
                             WorldModelLightingRoute, WorldModelLightingBinding);
} // namespace sgn::retail
