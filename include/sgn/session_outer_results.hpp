#pragma once

namespace sgn::retail
{
struct SessionOuterFrameContext;

// Actual Session wiring for the existing parent declaration. The Session,
// material owner, scratch, textures and sink must outlive this synchronous call.
// Borrows current owners; no source rows, strings or drawing storage are owned
// here. Material/font/scratch validation occurs only when a child borrows them.
void submit_session_results_449fb0(SessionOuterFrameContext &);
} // namespace sgn::retail
