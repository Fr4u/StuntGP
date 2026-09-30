#include "sgn/scene_resources.hpp"
#include "sgn/session_world_materials.hpp"

namespace sgn::retail
{
SourceSceneResourcePrefixResult reset_source_scene_resource_prefix_42e510(SceneResources &scene,
                                                                          SessionWorldMaterials &materials,
                                                                          SourceResourceRegistryObserver *observer)
{
    const auto flags = materials.registry_flags_for(scene); // identity/lifetime before writes
    scene.require_owner_thread();                           // borrowed scene access follows world lifetime validation
    for (unsigned slot = 0; slot < 16; ++slot)
    {
        auto &row = scene.records_[slot];
        row.descriptor = std::monostate{}; // 42E519: [636418+slot*16]
        if (observer)
            observer->cleared({0x42e519, 0x636418 + slot * 16, slot, 0}, scene, flags);
        row.file = {}; // 42E51C: [636420+slot*16]
        if (observer)
            observer->cleared({0x42e51c, 0x636420 + slot * 16, slot, 8}, scene, flags);
        row.file_size = 0; // 42E51E: [636424+slot*16]
        if (observer)
            observer->cleared({0x42e51e, 0x636424 + slot * 16, slot, 12}, scene, flags);
    }
    // 42E52B ->41C710 repeats slot1's descriptor store. It is an observable
    // source write even though42E519 already made this binding null.
    scene.records_[1].descriptor = std::monostate{};
    if (observer)
        observer->cleared({0x41c710, 0x636428, 1, 0}, scene, flags);
    return {}; // 42E53C is still required; do not admit full42E510.
}
} // namespace sgn::retail
