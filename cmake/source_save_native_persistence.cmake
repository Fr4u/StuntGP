include_guard(GLOBAL)
target_sources(source_menu_reconstruction PRIVATE src/source_save_card_storage.cpp)
# The concrete Native/Core provider belongs to application runtime. Keep its
# single translation unit there without a reverse menu-to-runtime dependency.
target_sources(source_application_runtime PRIVATE src/source_save_native_persistence.cpp)
