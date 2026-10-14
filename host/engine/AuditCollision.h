#pragma once
// Collision audit (dev tool, --audit-collision).
//
// For every model known to the game (every "objs"/"tobj" entry across the
// gta.dat IDEs) it compares the model's COLLISION bounds against its VISUAL
// (DFF) bounds, both in model space, and classifies the result:
//   OK            - collision box concentric with and similar size to the mesh
//   OFFSET        - collision centre is far from the mesh centre (pivot/offset
//                   bug -> collision sits off the rendered model)
//   SIZE_MISMATCH - collision much bigger/smaller than the mesh
//   MISSING_COL   - a big solid-looking model (building) with a DFF but no COL
//   EXTRA_COL     - a COL entry whose model has no DFF (collision for nothing)
//   NO_DFF_NO_COL - neither present (dangling IDE id); reported only in summary
//
// Because the engine applies the IDENTICAL placement transform to both the
// DFF and the COL (QuatToMatrix == QuatXform), per-instance orientation is
// correct by construction; the only way collision can be misoriented/offset
// relative to the rendered model is a model-space mismatch between the two,
// which is exactly what this audit measures.
//
// Pure CPU: no window or D3D device required. Writes a TSV and a summary.

#include <string>

class ImgLoader;

// Runs the audit over the already-opened IMG. gameDir is the game root (for
// data\gta.dat + the IDE files). Writes the per-model TSV to outTsvPath.
// Returns the number of flagged (non-OK) models.
int RunCollisionAudit(ImgLoader& img, const std::string& gameDir,
                      const std::string& outTsvPath);

// Audits every billboard/sign model: for each, checks that its DFF loads, each
// mesh texture resolves (in the model's own txd, a txdp parent, or a boot txd),
// and its collision matches the mesh. Writes a per-model TSV. Returns the number
// of models with a missing texture or a collision problem.
int RunSignAudit(ImgLoader& img, const std::string& gameDir,
                 const std::string& outTsvPath);
