#pragma once
#include "sgn/source_renderer_reset.hpp"
#include "sgn/retail_device_setup.hpp"
#include "sgn/source_application_literals.hpp"
namespace sgn::retail
{
// These are mandatory device-production boundaries, not readiness flags. The
// port of431360 below does not implement native enumeration by itself.
struct SourceRendererBootstrapServices
{
    virtual ~SourceRendererBootstrapServices() = default;
    virtual void enumerate_adapters_42f6a0(FrontendState &) = 0;
    virtual void create_surfaces_42fd80(FrontendState &, std::uint32_t adapter) = 0;
    virtual void create_depth_42ff20(FrontendState &, std::uint32_t adapter, std::uint32_t device) = 0;
    virtual void attach_depth_42fff0(FrontendState &) = 0;
    virtual std::uint32_t create_device_430020(FrontendState &, std::uint32_t adapter, std::uint32_t device) = 0;
    // Must copy actual selected descriptor+164 to the sole global device owner.
    virtual void publish_selected_device_4313d5(FrontendState &) = 0;
    virtual std::uint32_t create_viewport_42f840(FrontendState &) = 0;
    virtual void create_white_material_430ff0(FrontendState &) = 0;
    virtual DeviceSetupWorld device_setup_world(FrontendState &) = 0;
    virtual void query_gamma_432070(FrontendState &) = 0;
    virtual void driver_restrictions_42f220(FrontendState &) = 0;
    // Actual44E4A0 +nonreturning462DFA(1). Return is a binding error.
    virtual void fatal_431330(FrontendState &) = 0;
    // Side-effect-free host borrow at431417 and cold caller42E726/42E72B,
    // including later material entry. Never reset, construct or publish resources
    // here. Return the existing backend and same frontend/scratch/material owners;
    // actual construction belongs to preceding device-production children.
    virtual SourceRendererResetContext reset_context(FrontendState &) = 0;
};
struct SourceRendererBootstrapContext
{
    FrontendState &frontend;
    SessionWorldMaterials &materials;
    SourceApplicationLiteralData &application;
    PolygonSubmissionState &submission;
    SourceRendererBootstrapServices &calls;
};
void initialize_source_renderer_431360(SourceRendererBootstrapContext);
} // namespace sgn::retail
