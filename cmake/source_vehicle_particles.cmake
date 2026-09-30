include_guard(GLOBAL)
# R17 source-owner particle children of 418830/418970 (416A80,415880,417CA0,416F30,417800,4173E0).
# Same target as the R17 vehicle receiver (cmake/source_vehicle_contact_update.cmake).
target_sources(source_music_runtime PRIVATE src/source_vehicle_particles.cpp)
# Reused verified leaves: 45E110 allocator, 417B50 emit_load_particle/4169E0 staging, 42F1F0 RNG integer.
target_link_libraries(source_music_runtime PUBLIC contact_vehicle_particles contact_particle_allocator retail_vehicle)
