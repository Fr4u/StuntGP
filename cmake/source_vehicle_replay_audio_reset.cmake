include_guard(GLOBAL)
# R26: the R17 replay_audio_reset sources (41AC30/41AB70/41BEA0/4187A0/418320) were removed with
# the dead R17 418A90 chain (never linked into StuntGP_NEW). The link dependencies stay unchanged
# so that the StuntGP_NEW link line (library order) is identical.
target_link_libraries(source_music_runtime PUBLIC
  retail_audio retail_replay retail_replay_playback retail_vehicle_air_tick platform_audio)
