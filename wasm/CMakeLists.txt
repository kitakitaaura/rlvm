cmake_minimum_required(VERSION 3.13)
project(rlvm_wasm CXX C)

if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "Configure this with emcmake cmake ...")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(RLVM_ASAN "Build with -fsanitize=address to debug memory corruption" OFF)

# Two layouts: the Noberu workspace (../engine, ../third_party/...) or the
# rlvm fork's noberu branch (../src, ../vendor, boost built into ./boost-wasm
# by build_boost_wasm.sh).
if(EXISTS ${CMAKE_SOURCE_DIR}/../engine)
  set(ENGINE_DIR ${CMAKE_SOURCE_DIR}/../engine)
  set(VENDOR_DIR ${CMAKE_SOURCE_DIR}/../third_party/vendor)
  set(BOOST_PARENT ${CMAKE_SOURCE_DIR}/../third_party)
else()
  set(ENGINE_DIR ${CMAKE_SOURCE_DIR}/../src)
  set(VENDOR_DIR ${CMAKE_SOURCE_DIR}/../vendor)
  set(BOOST_PARENT ${CMAKE_SOURCE_DIR})
endif()
if(RLVM_ASAN)
  set(BOOST_WASM_DIR ${BOOST_PARENT}/boost-wasm-asan)
else()
  set(BOOST_WASM_DIR ${BOOST_PARENT}/boost-wasm)
endif()

include_directories(
  ${ENGINE_DIR}
  ${VENDOR_DIR}
  ${BOOST_WASM_DIR}/include
)

add_compile_options(
  -funsigned-char
  -Wno-narrowing
  -Wno-c++11-narrowing
  -Wno-write-strings
  -Wno-deprecated-declarations
  -Wno-tautological-constant-out-of-range-compare
  -include ${CMAKE_SOURCE_DIR}/gl_compat.h
)

# rlvm's own headers assume config.h defines are absent -> ENABLE_NLS stays
# undefined, which utilities/gettext.h treats as "NLS disabled" (identity
# macros), so we don't need to generate a build/config.h at all.
add_compile_definitions(
  CASE_SENSITIVE_FILESYSTEM
  _THREAD_SAFE
  BOOST_DATE_TIME_NO_LIB
  BOOST_ALL_NO_LIB
)

set(LIBRLVM_SOURCES
  ${ENGINE_DIR}/base/notification_details.cc
  ${ENGINE_DIR}/base/notification_registrar.cc
  ${ENGINE_DIR}/base/notification_service.cc
  ${ENGINE_DIR}/base/notification_source.cc
  ${ENGINE_DIR}/effects/blind_effect.cc
  ${ENGINE_DIR}/effects/effect.cc
  ${ENGINE_DIR}/effects/effect_factory.cc
  ${ENGINE_DIR}/effects/fade_effect.cc
  ${ENGINE_DIR}/effects/scroll_on_scroll_off.cc
  ${ENGINE_DIR}/effects/wipe_effect.cc
  ${ENGINE_DIR}/encodings/codepage.cc
  ${ENGINE_DIR}/encodings/cp932.cc
  ${ENGINE_DIR}/encodings/cp936.cc
  ${ENGINE_DIR}/encodings/cp949.cc
  ${ENGINE_DIR}/encodings/han2zen.cc
  ${ENGINE_DIR}/encodings/western.cc
  ${ENGINE_DIR}/libreallive/archive.cc
  ${ENGINE_DIR}/libreallive/bytecode.cc
  ${ENGINE_DIR}/libreallive/compression.cc
  ${ENGINE_DIR}/libreallive/expression.cc
  ${ENGINE_DIR}/libreallive/filemap.cc
  ${ENGINE_DIR}/libreallive/gameexe.cc
  ${ENGINE_DIR}/libreallive/intmemref.cc
  ${ENGINE_DIR}/libreallive/scenario.cc
  ${ENGINE_DIR}/long_operations/button_object_select_long_operation.cc
  ${ENGINE_DIR}/long_operations/load_game_long_operation.cc
  ${ENGINE_DIR}/long_operations/pause_long_operation.cc
  ${ENGINE_DIR}/long_operations/select_long_operation.cc
  ${ENGINE_DIR}/long_operations/textout_long_operation.cc
  ${ENGINE_DIR}/long_operations/wait_long_operation.cc
  ${ENGINE_DIR}/long_operations/zoom_long_operation.cc
  ${ENGINE_DIR}/machine/dump_scenario.cc
  ${ENGINE_DIR}/machine/game_hacks.cc
  ${ENGINE_DIR}/machine/general_operations.cc
  ${ENGINE_DIR}/machine/long_operation.cc
  ${ENGINE_DIR}/machine/mapped_rlmodule.cc
  ${ENGINE_DIR}/machine/memory.cc
  ${ENGINE_DIR}/machine/memory_intmem.cc
  ${ENGINE_DIR}/machine/opcode_log.cc
  ${ENGINE_DIR}/machine/reallive_dll.cc
  ${ENGINE_DIR}/machine/reference.cc
  ${ENGINE_DIR}/machine/rlmachine.cc
  ${ENGINE_DIR}/machine/rlmodule.cc
  ${ENGINE_DIR}/machine/rloperation.cc
  ${ENGINE_DIR}/machine/rloperation/argc_t.cc
  ${ENGINE_DIR}/machine/rloperation/complex_t.cc
  ${ENGINE_DIR}/machine/rloperation/rlop_store.cc
  ${ENGINE_DIR}/machine/save_game_header.cc
  ${ENGINE_DIR}/machine/serialization_global.cc
  ${ENGINE_DIR}/machine/serialization_local.cc
  ${ENGINE_DIR}/machine/stack_frame.cc
  ${ENGINE_DIR}/machine/rlvm_instance.cc
  ${ENGINE_DIR}/modules/module_bgm.cc
  ${ENGINE_DIR}/modules/object_mutator_operations.cc
  ${ENGINE_DIR}/modules/module_bgr.cc
  ${ENGINE_DIR}/modules/module_dll.cc
  ${ENGINE_DIR}/modules/module_debug.cc
  ${ENGINE_DIR}/modules/module_event_loop.cc
  ${ENGINE_DIR}/modules/module_g00.cc
  ${ENGINE_DIR}/modules/module_gan.cc
  ${ENGINE_DIR}/modules/module_grp.cc
  ${ENGINE_DIR}/modules/module_jmp.cc
  ${ENGINE_DIR}/modules/module_koe.cc
  ${ENGINE_DIR}/modules/module_mem.cc
  ${ENGINE_DIR}/modules/module_mov.cc
  ${ENGINE_DIR}/modules/module_msg.cc
  ${ENGINE_DIR}/modules/module_obj.cc
  ${ENGINE_DIR}/modules/module_obj_creation.cc
  ${ENGINE_DIR}/modules/module_obj_fg_bg.cc
  ${ENGINE_DIR}/modules/module_obj_management.cc
  ${ENGINE_DIR}/modules/module_obj_getters.cc
  ${ENGINE_DIR}/modules/module_os.cc
  ${ENGINE_DIR}/modules/module_pcm.cc
  ${ENGINE_DIR}/modules/module_refresh.cc
  ${ENGINE_DIR}/modules/module_scr.cc
  ${ENGINE_DIR}/modules/module_se.cc
  ${ENGINE_DIR}/modules/module_sel.cc
  ${ENGINE_DIR}/modules/module_shk.cc
  ${ENGINE_DIR}/modules/module_shl.cc
  ${ENGINE_DIR}/modules/module_str.cc
  ${ENGINE_DIR}/modules/module_sys.cc
  ${ENGINE_DIR}/modules/module_sys_date.cc
  ${ENGINE_DIR}/modules/module_sys_frame.cc
  ${ENGINE_DIR}/modules/module_sys_name.cc
  ${ENGINE_DIR}/modules/module_sys_save.cc
  ${ENGINE_DIR}/modules/module_sys_syscom.cc
  ${ENGINE_DIR}/modules/module_sys_timer.cc
  ${ENGINE_DIR}/modules/module_sys_wait.cc
  ${ENGINE_DIR}/modules/module_sys_index_series.cc
  ${ENGINE_DIR}/modules/module_sys_timetable2.cc
  ${ENGINE_DIR}/modules/modules.cc
  ${ENGINE_DIR}/modules/object_module.cc
  ${ENGINE_DIR}/systems/base/anm_graphics_object_data.cc
  ${ENGINE_DIR}/systems/base/cgm_table.cc
  ${ENGINE_DIR}/systems/base/colour.cc
  ${ENGINE_DIR}/systems/base/colour_filter_object_data.cc
  ${ENGINE_DIR}/systems/base/digits_graphics_object.cc
  ${ENGINE_DIR}/systems/base/drift_graphics_object.cc
  ${ENGINE_DIR}/systems/base/event_listener.cc
  ${ENGINE_DIR}/systems/base/event_system.cc
  ${ENGINE_DIR}/systems/base/frame_counter.cc
  ${ENGINE_DIR}/systems/base/gan_graphics_object_data.cc
  ${ENGINE_DIR}/systems/base/graphics_object.cc
  ${ENGINE_DIR}/systems/base/graphics_object_data.cc
  ${ENGINE_DIR}/systems/base/graphics_object_of_file.cc
  ${ENGINE_DIR}/systems/base/graphics_stack_frame.cc
  ${ENGINE_DIR}/systems/base/graphics_system.cc
  ${ENGINE_DIR}/systems/base/graphics_text_object.cc
  ${ENGINE_DIR}/systems/base/hik_renderer.cc
  ${ENGINE_DIR}/systems/base/hik_script.cc
  ${ENGINE_DIR}/systems/base/koepac_voice_archive.cc
  ${ENGINE_DIR}/systems/base/little_busters_ef00dll.cc
  ${ENGINE_DIR}/systems/base/little_busters_pt00dll.cc
  ${ENGINE_DIR}/systems/base/mouse_cursor.cc
  ${ENGINE_DIR}/systems/base/nwk_voice_archive.cc
  ${ENGINE_DIR}/systems/base/object_mutator.cc
  ${ENGINE_DIR}/systems/base/object_settings.cc
  ${ENGINE_DIR}/systems/base/ovk_voice_archive.cc
  ${ENGINE_DIR}/systems/base/ovk_voice_sample.cc
  ${ENGINE_DIR}/systems/base/parent_graphics_object_data.cc
  ${ENGINE_DIR}/systems/base/platform.cc
  ${ENGINE_DIR}/systems/base/rltimer.cc
  ${ENGINE_DIR}/systems/base/rlbabel_dll.cc
  ${ENGINE_DIR}/systems/base/rect.cc
  ${ENGINE_DIR}/systems/base/selection_element.cc
  ${ENGINE_DIR}/systems/base/sound_system.cc
  ${ENGINE_DIR}/systems/base/surface.cc
  ${ENGINE_DIR}/systems/base/system.cc
  ${ENGINE_DIR}/systems/base/system_error.cc
  ${ENGINE_DIR}/systems/base/text_key_cursor.cc
  ${ENGINE_DIR}/systems/base/text_page.cc
  ${ENGINE_DIR}/systems/base/text_system.cc
  ${ENGINE_DIR}/systems/base/text_waku.cc
  ${ENGINE_DIR}/systems/base/text_waku_normal.cc
  ${ENGINE_DIR}/systems/base/text_waku_type4.cc
  ${ENGINE_DIR}/systems/base/text_window.cc
  ${ENGINE_DIR}/systems/base/text_window_button.cc
  ${ENGINE_DIR}/systems/base/tomoyo_after_dt00dll.cc
  ${ENGINE_DIR}/systems/base/tone_curve.cc
  ${ENGINE_DIR}/systems/base/voice_archive.cc
  ${ENGINE_DIR}/systems/base/voice_cache.cc
  ${ENGINE_DIR}/utilities/exception.cc
  ${ENGINE_DIR}/utilities/file.cc
  ${ENGINE_DIR}/utilities/graphics.cc
  ${ENGINE_DIR}/utilities/string_utilities.cc
  ${ENGINE_DIR}/utilities/date_util.cc
  ${ENGINE_DIR}/utilities/find_font_file.cc
  ${ENGINE_DIR}/utilities/math_util.cc
  ${VENDOR_DIR}/xclannad/endian.cpp
  ${VENDOR_DIR}/xclannad/file.cc
  ${VENDOR_DIR}/xclannad/koedec_ogg.cc
  ${VENDOR_DIR}/xclannad/nwatowav.cc
  ${VENDOR_DIR}/xclannad/wavfile.cc
)

set(SYSTEM_SDL_SOURCES
  ${ENGINE_DIR}/systems/sdl/sdl_audio_locker.cc
  ${ENGINE_DIR}/systems/sdl/sdl_colour_filter.cc
  ${ENGINE_DIR}/systems/sdl/sdl_event_system.cc
  ${ENGINE_DIR}/systems/sdl/sdl_graphics_system.cc
  ${ENGINE_DIR}/systems/sdl/sdl_music.cc
  ${ENGINE_DIR}/systems/sdl/sdl_render_to_texture_surface.cc
  ${ENGINE_DIR}/systems/sdl/sdl_sound_chunk.cc
  ${ENGINE_DIR}/systems/sdl/sdl_sound_system.cc
  ${ENGINE_DIR}/systems/sdl/sdl_surface.cc
  ${ENGINE_DIR}/systems/sdl/sdl_system.cc
  ${ENGINE_DIR}/systems/sdl/sdl_text_system.cc
  ${ENGINE_DIR}/systems/sdl/sdl_text_window.cc
  ${ENGINE_DIR}/systems/sdl/sdl_utils.cc
  ${ENGINE_DIR}/systems/sdl/shaders.cc
  ${ENGINE_DIR}/systems/sdl/texture.cc
  ${VENDOR_DIR}/pygame/alphablit.cc

  # NOTE: resample.cc/audiofile.cc/dither.cc/zresample.cc (NWA resampling via
  # libsndfile + zita-resampler) are intentionally excluded for v1 -- neither
  # dependency has an Emscripten port. See rlvm_instance / sound system for
  # the fallback path. Revisit if a tested game needs NWA audio.
)

set(WASM_SOURCES
  ${CMAKE_SOURCE_DIR}/main_wasm.cc
  ${CMAKE_SOURCE_DIR}/wasm_stubs.cc
  ${CMAKE_SOURCE_DIR}/gl_upload_compat.cc
  ${ENGINE_DIR}/platforms/sdl/sdl_rlvm_instance.cc
)

add_executable(rlvm ${LIBRLVM_SOURCES} ${SYSTEM_SDL_SOURCES} ${WASM_SOURCES})

target_compile_definitions(rlvm PRIVATE __EMSCRIPTEN__)

target_link_libraries(rlvm PRIVATE
  ${BOOST_WASM_DIR}/lib/libboost_filesystem.a
  ${BOOST_WASM_DIR}/lib/libboost_serialization.a
  ${BOOST_WASM_DIR}/lib/libboost_iostreams.a
  ${BOOST_WASM_DIR}/lib/libboost_date_time.a
)

set(EMS_FLAGS
  "-sUSE_SDL=2"
  "-sUSE_SDL_TTF=2"
  "-sUSE_SDL_MIXER=2"
  "-sUSE_FREETYPE=1"
  "-sUSE_VORBIS=1"
  "-sUSE_OGG=1"
  "-sUSE_ZLIB=1"
  "-sUSE_SDL_IMAGE=2"
  "-sSDL2_IMAGE_FORMATS=[png]"
  "-sLEGACY_GL_EMULATION=1"
  "-fexceptions"
  # main() returns immediately (see wasm/main_wasm.cc) and JS keeps calling
  # into the module afterward (rlvm_start(), FS writes during upload, etc).
  # Without this, Emscripten tears the runtime down when main() returns;
  # Release builds happened to tolerate calling in anyway, but ASan/debug
  # builds assert on it.
  "-sEXIT_RUNTIME=0"
)

if(RLVM_ASAN)
  list(APPEND EMS_FLAGS "-fsanitize=address" "-sINITIAL_MEMORY=1024MB")
endif()

target_compile_options(rlvm PRIVATE ${EMS_FLAGS})
target_link_options(rlvm PRIVATE
  ${EMS_FLAGS}
  "-sALLOW_MEMORY_GROWTH=1"
  # Start big enough that the heap never has to grow in normal play (Clannad
  # boots in ~43 MB). Safari 26 on iOS has WebAssembly bugs around
  # memory.grow that showed up as "Out of bounds memory access (evaluating
  # 'getWasmTableEntry(index)(a1, a2, a3, a4)')" on boot, while Chrome and
  # older Safari were fine (emscripten-core/emscripten#25905, WebKit bugs
  # 303387 and 301634). Untouched pages cost no real memory.
  "-sINITIAL_MEMORY=256MB"
  "-sEXPORTED_RUNTIME_METHODS=[FS,ccall,cwrap]"
  "-sEXPORTED_FUNCTIONS=[_main,_rlvm_start,_rlvm_start_at,_rlvm_set_int,_rlvm_select,_rlvm_dump_scene]"
  "-sFORCE_FILESYSTEM=1"
  "-lidbfs.js"
  # On-demand game files: lazyfs.js replaces the openat syscall so opening a
  # file that is not loaded yet has the page read it in first (see there for
  # why that read is synchronous rather than ASYNCIFY).
  "--js-library=${CMAKE_SOURCE_DIR}/lazyfs.js"
  "--shell-file=${CMAKE_SOURCE_DIR}/shell.html"
)

set_target_properties(rlvm PROPERTIES
  OUTPUT_NAME "rlvm"
  SUFFIX ".html"
)
