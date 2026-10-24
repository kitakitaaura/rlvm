// Emscripten entry point for rlvm. Replaces src/platforms/sdl/rlvm.cc's
// blocking CLI main() with a browser-driven main loop: the game root is
// populated into MEMFS by the JS shell (see web/app.js) before this module's
// main() runs, and RunFrame() is called once per animation frame instead of
// blocking on RLVMInstance::Run()'s while loop.

#include <emscripten.h>

#include <boost/filesystem.hpp>

#include "libreallive/intmemref.h"
#include "long_operations/select_long_operation.h"
#include "machine/memory.h"
#include "machine/rlmachine.h"
#include "machine/rlvm_instance.h"
#include "platforms/sdl/sdl_rlvm_instance.h"

namespace {
SDLRLVMInstance* g_instance = nullptr;

void MainLoop() {
  if (!g_instance || !g_instance->RunFrame()) {
    emscripten_cancel_main_loop();
    // Not deleted: the instance has already saved and told the page
    // (RLVMInstance::Shutdown), and destroying it would free GL textures
    // after SDL has dropped the context. The page loads a new engine for the
    // next boot, so the memory goes with it.
    g_instance = nullptr;
  }
}
}  // namespace

// Called from JS (web/app.js) once the uploaded game folder has been fully
// written into MEMFS at /game.
extern "C" EMSCRIPTEN_KEEPALIVE void rlvm_start(const char* game_root) {
  if (g_instance) {
    delete g_instance;
    g_instance = nullptr;
  }

  g_instance = new SDLRLVMInstance(nullptr, nullptr);
  if (g_instance->Init(boost::filesystem::path(game_root))) {
    emscripten_set_main_loop(MainLoop, 0, 0);
  } else {
    delete g_instance;
    g_instance = nullptr;
  }
}

// Debug helper: boots like rlvm_start() but begins at SEEN `seen_num`
// (rlvm's --start-seen), to reach a late scene without playing up to it.
extern "C" EMSCRIPTEN_KEEPALIVE void rlvm_start_at(const char* game_root,
                                                    int seen_num) {
  if (g_instance) {
    delete g_instance;
    g_instance = nullptr;
  }

  g_instance = new SDLRLVMInstance(nullptr, nullptr);
  g_instance->set_seen_start(seen_num);
  if (g_instance->Init(boost::filesystem::path(game_root))) {
    emscripten_set_main_loop(MainLoop, 0, 0);
  } else {
    delete g_instance;
    g_instance = nullptr;
  }
}

// The walkthrough (noberu-web assets/walkthrough.js) picks option `index` of
// the choice on screen. Returns 1 if it was taken.
extern "C" EMSCRIPTEN_KEEPALIVE int rlvm_select(int index) {
  if (!g_instance || !g_instance->machine())
    return 0;
  auto select = std::dynamic_pointer_cast<SelectLongOperation>(
      g_instance->machine()->CurrentLongOperation());
  if (!select || !select->IsSelectable(index))
    return 0;
  select->SelectByIndex(index);
  return 1;
}

// Debug helper: sets an integer variable, e.g. ('F', 1107, 1) for intF[1107],
// to put a scene started with rlvm_start_at() on the branch a playthrough
// would have taken.
extern "C" EMSCRIPTEN_KEEPALIVE void rlvm_set_int(int bank, int location,
                                                   int value) {
  if (!g_instance || !g_instance->machine())
    return;
  g_instance->machine()->memory().SetIntValue(
      libreallive::IntMemRef(static_cast<char>(bank), location), value);
}

// Debug helper: dumps a scene's bytecode source representation to the
// console via DumpScenario() (RLVMInstance::Init() with dump_seen_ set
// returns before starting the main loop) instead of running it. Used to
// find what opcode a hang is stuck on. Not part of the normal app flow.
extern "C" EMSCRIPTEN_KEEPALIVE void rlvm_dump_scene(const char* game_root,
                                                      int seen_num) {
  SDLRLVMInstance instance(nullptr, nullptr);
  instance.set_dump_seen(seen_num);
  instance.Init(boost::filesystem::path(game_root));
}

int main() {
  // Nothing to do until JS calls rlvm_start() once the game folder upload
  // finishes; keep the runtime alive without an active main loop.
  return 0;
}
