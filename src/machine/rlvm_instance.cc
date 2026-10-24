// -*- Mode: C++; tab-width:2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi:tw=80:et:ts=2:sts=2
//
// -----------------------------------------------------------------------
//
// This file is part of RLVM, a RealLive virtual machine clone.
//
// -----------------------------------------------------------------------
//
// Copyright (C) 2011 Elliot Glaysher
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
// -----------------------------------------------------------------------

#include "machine/rlvm_instance.h"

#include <fstream>
#include <map>
#include <sstream>

#include <iostream>
#include <string>

#include "libreallive/gameexe.h"
#include "libreallive/reallive.h"
#include "machine/dump_scenario.h"
#include "machine/game_hacks.h"
#include "machine/memory.h"
#include "machine/rlmachine.h"
#include "machine/serialization.h"
#include "modules/module_sys_save.h"
#include "modules/modules.h"
// Temporarily disable guichan for the SDL2 porting.
//#include "platforms/gcn/gcn_platform.h"
#include "systems/base/event_system.h"
#include "systems/base/graphics_system.h"
#include "systems/base/system_error.h"
#include "systems/sdl/sdl_system.h"
#include "utf8cpp/utf8.h"
#include "utilities/exception.h"
#include "utilities/file.h"
#include "utilities/find_font_file.h"
#include "utilities/gettext.h"
#include "utilities/string_utilities.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

namespace fs = boost::filesystem;

// AVG32 file checks. We can't run AVG32 games.
const char* avg32_exes[] = {"avg3216m.exe", "avg3217m.exe", NULL};

// Siglus engine filenames. We can't run VisualArts' newer engine.
const char* siglus_exes[] = {"siglus.exe",       "siglusengine-ch.exe",
                             "siglusengine.exe", "siglusenginechs.exe",
                             NULL};

RLVMInstance::RLVMInstance()
    : seen_start_(-1),
      memory_(false),
      undefined_opcodes_(false),
      count_undefined_copcodes_(false),
      tracing_(false),
      load_save_(-1),
      dump_seen_(-1) {
  srand(time(NULL));
}

RLVMInstance::~RLVMInstance() {}

void RLVMInstance::Run(const boost::filesystem::path& gamerootPath) {
  if (!Init(gamerootPath))
    return;
  while (RunFrame()) {
  }
}

bool RLVMInstance::Init(const boost::filesystem::path& gamerootPath) {
  try {
    fs::path gameexePath = FindGameFile(gamerootPath, "Gameexe.ini");
    fs::path seenPath = FindGameFile(gamerootPath, "Seen.txt");

    // Check for VisualArt's older and newer engines, which we can't emulate:
    CheckBadEngine(gamerootPath, avg32_exes, _("Can't run AVG32 games"));
    CheckBadEngine(gamerootPath, siglus_exes, _("Can't run Siglus games"));

    gameexe_.reset(new Gameexe(gameexePath));
    Gameexe& gameexe = *gameexe_;
    gameexe("__GAMEPATH") = gamerootPath.string();

    // Possibly force starting at a different seen
    if (seen_start_ != -1)
      gameexe("SEEN_START") = seen_start_;

    if (memory_)
      gameexe("MEMORY") = 1;

    if (!custom_font_.empty()) {
      if (!fs::exists(custom_font_)) {
        throw rlvm::UserPresentableError(
            _("Could not open font file."),
            _("Please make sure the font file specified with --font exists and "
              "is a TrueType font."));
      }

      gameexe("__GAMEFONT") = custom_font_;
    }

    arc_.reset(new libreallive::Archive(seenPath.string(), gameexe("REGNAME")));
    sdl_system_.reset(new SDLSystem(gameexe));
    rlmachine_.reset(new RLMachine(*sdl_system_, *arc_));
    AddAllModules(*rlmachine_);
    AddGameHacks(*rlmachine_);

    if (dump_seen_ == 0) {
      // Every scene, each headed "#SEEN n", into /tmp/scenes.txt (for tools
      // that study a game's script, like noberu's walkthrough builder).
      std::ofstream out("/tmp/scenes.txt");
      std::vector<int> numbers;
      for (auto it = arc_->begin(); it != arc_->end(); ++it)
        numbers.push_back(it->first);
      for (int number : numbers) {
        libreallive::Scenario* scenario = arc_->GetScenario(number);
        if (!scenario)
          continue;
        out << "#SEEN " << number << "\n";
        // Each instruction numbered ("@n "), and jumps followed by their
        // targets (" => @a @b"), so the control flow can be rebuilt.
        std::map<const libreallive::BytecodeElement*, int> index;
        int n = 0;
        for (auto const& instruction : *scenario)
          index[instruction.get()] = n++;
        n = 0;
        for (auto const& instruction : *scenario) {
          std::ostringstream line;
          instruction->PrintSourceRepresentation(rlmachine_.get(), line);
          std::string text = line.str();
          while (!text.empty() && text.back() == '\n')
            text.pop_back();
          out << "@" << n++ << " " << text;
          auto* command =
              dynamic_cast<const libreallive::CommandElement*>(instruction.get());
          size_t count = command ? command->GetPointersCount() : 0;
          if (command && command->GetCaseCount()) {
            out << " cases";
            for (size_t i = 0; i < command->GetCaseCount(); ++i)
              out << " [" << libreallive::ParsableToPrintableString(
                                 command->GetCase(i)) << "]";
          }
          if (count) {
            out << " =>";
            for (size_t i = 0; i < count; ++i) {
              libreallive::pointer_t target = command->GetPointer(i);
              if (target == libreallive::pointer_t()) {
                out << " @-1";
                continue;
              }
              auto it = index.find(target->get());
              out << " @" << (it == index.end() ? -1 : it->second);
            }
          }
          out << "\n";
        }
      }
      out.close();
      rlmachine_.reset();
      sdl_system_.reset();
      arc_.reset();
      gameexe_.reset();
      return false;
    }

    if (dump_seen_ != -1) {
      libreallive::Scenario* scenario = arc_->GetScenario(dump_seen_);
      DumpScenario(rlmachine_.get(), scenario);
      rlmachine_.reset();
      sdl_system_.reset();
      arc_.reset();
      gameexe_.reset();
      return false;
    }

    // Validate our font file
    // TODO(erg): Remove this when we switch to native font selection dialogs.
    fs::path fontFile = FindFontFile(*sdl_system_);
    if (fontFile.empty() || !fs::exists(fontFile)) {
      throw rlvm::UserPresentableError(
          _("Could not find msgothic.ttc or a suitable fallback font."),
          _("Please place a copy of msgothic.ttc in either your home directory "
            "or in the game path."));
    }

    // Initialize our platform dialogs (we have to do this after
    // looking for a font because we use that font internally).
    // std::shared_ptr<GCNPlatform> platform(
    //     new GCNPlatform(sdlSystem, sdlSystem.graphics().screen_rect()));
    // sdlSystem.SetPlatform(platform);

    if (undefined_opcodes_)
      rlmachine_->SetPrintUndefinedOpcodes(true);

    if (count_undefined_copcodes_)
      rlmachine_->RecordUndefinedOpcodeCounts();

    if (tracing_)
      rlmachine_->set_tracing_on();

    Serialization::loadGlobalMemory(*rlmachine_);

    // Now to preform a quick integrity check. If the user opened the Japanese
    // version of CLANNAD (or any other game), and then installed a patch, our
    // user data is going to be screwed!
    DoUserNameCheck(*rlmachine_);

    rlmachine_->SetHaltOnException(false);

    if (load_save_ != -1)
      Sys_load()(*rlmachine_, load_save_);

    return true;
  }
  catch (rlvm::UserPresentableError& e) {
    ReportFatalError(e.message_text(), e.informative_text());
  }
  catch (rlvm::Exception& e) {
    ReportFatalError(_("Fatal RLVM error"), e.what());
  }
  catch (libreallive::Error& e) {
    ReportFatalError(_("Fatal libreallive error"), e.what());
  }
  catch (SystemError& e) {
    ReportFatalError(_("Fatal local system error"), e.what());
  }
  catch (std::exception& e) {
    ReportFatalError(_("Uncaught exception"), e.what());
  }
  catch (const char* e) {
    ReportFatalError(_("Uncaught exception"), e);
  }

  rlmachine_.reset();
  sdl_system_.reset();
  arc_.reset();
  gameexe_.reset();
  return false;
}

bool RLVMInstance::RunFrame() {
  if (!rlmachine_)
    return false;

  try {
    if (rlmachine_->halted()) {
      Shutdown();
      return false;
    }

#if defined(__EMSCRIPTEN__)
    static int debug_frame_count = 0;
    static int debug_instr_count = 0;
    if (debug_frame_count < 20) {
      EM_ASM({ console.log('[dbg] RunFrame #' + $0 + ' begin, calling sdl_system_->Run'); },
              debug_frame_count);
    }
#endif

    // Give SDL a chance to respond to events, redraw the screen, etc.
    sdl_system_->Run(*rlmachine_);

#if defined(__EMSCRIPTEN__)
    if (debug_frame_count < 20) {
      EM_ASM({ console.log('[dbg] RunFrame #' + $0 + ' sdl_system_->Run returned, starting instruction loop'); },
              debug_frame_count);
    }
#endif

    // Run the rlmachine through as many instructions as we can in a 10ms
    // time slice. Bail out if we switch to long operation mode, or if the
    // screen is marked as dirty.
    unsigned int start_ticks = sdl_system_->event().GetTicks();
    unsigned int end_ticks = start_ticks;
#if defined(__EMSCRIPTEN__)
    // Belt-and-suspenders bound on top of the elapsed-time check below: this
    // function runs synchronously inside a single browser animation-frame
    // callback (emscripten_set_main_loop), so if anything ever causes
    // end_ticks - start_ticks to stop advancing (a tick-source bug, clock
    // throttling in a backgrounded tab, etc), the time-based condition alone
    // would spin forever and hang the tab with no way to recover -- unlike
    // the native build, where the equivalent loop hanging just stalls one
    // thread the OS can still preempt.
    int instructions_this_slice = 0;
    const int kMaxInstructionsPerSlice = 200000;
#endif
    do {
#if defined(__EMSCRIPTEN__)
      if (debug_instr_count < 500) {
        EM_ASM({ console.log('[dbg] instr ' + $0 + ' scene=' + $1 + ' line=' + $2); },
                debug_instr_count, rlmachine_->SceneNumber(), rlmachine_->line_number());
      }
      ++debug_instr_count;
#endif
      rlmachine_->ExecuteNextInstruction();
#if defined(__EMSCRIPTEN__)
      if (debug_instr_count <= 500) {
        EM_ASM({ console.log('[dbg] instr ' + $0 + ' returned ok'); }, debug_instr_count - 1);
      }
#endif
      end_ticks = sdl_system_->event().GetTicks();
    } while (!rlmachine_->CurrentLongOperation() &&
             !sdl_system_->force_wait() &&
             (end_ticks - start_ticks < 10)
#if defined(__EMSCRIPTEN__)
             && ++instructions_this_slice < kMaxInstructionsPerSlice
#endif
             );  // NOLINT

    // Sleep to be nice to the processor and to give the GPU a chance to
    // catch up. On Emscripten, the browser's own frame cadence already
    // paces us, so callers there should treat this as advisory only; the
    // native (non-Emscripten) loop in Run() actually sleeps.
    if (!sdl_system_->ShouldFastForward()) {
      int real_sleep_time = 10 - (end_ticks - start_ticks);
      if (real_sleep_time < 1)
        real_sleep_time = 1;
#if !defined(__EMSCRIPTEN__)
      sdl_system_->event().Wait(real_sleep_time);
#endif
    }

    sdl_system_->set_force_wait(false);

#if defined(__EMSCRIPTEN__)
    ++debug_frame_count;
#endif

    if (rlmachine_->halted()) {
      Shutdown();
      return false;
    }
    return true;
  }
  catch (rlvm::UserPresentableError& e) {
    ReportFatalError(e.message_text(), e.informative_text());
  }
  catch (rlvm::Exception& e) {
    ReportFatalError(_("Fatal RLVM error"), e.what());
  }
  catch (libreallive::Error& e) {
    ReportFatalError(_("Fatal libreallive error"), e.what());
  }
  catch (SystemError& e) {
    ReportFatalError(_("Fatal local system error"), e.what());
  }
  catch (std::exception& e) {
    ReportFatalError(_("Uncaught exception"), e.what());
  }
  catch (const char* e) {
    ReportFatalError(_("Uncaught exception"), e);
  }

  rlmachine_.reset();
  sdl_system_.reset();
  arc_.reset();
  gameexe_.reset();
  return false;
}

void RLVMInstance::Shutdown() {
  if (rlmachine_)
    Serialization::saveGlobalMemory(*rlmachine_);
#if defined(__EMSCRIPTEN__)
  // The game chose to quit (a title screen's Exit). In a browser there is no
  // process to end and the page loads a fresh engine for the next game, so
  // nothing is torn down: SDL has already let go of the WebGL context by the
  // time the graphics system would free its textures, and that threw. The
  // page is told instead, to save and show that the game ended.
  EM_ASM({ if (Module.onGameExit) Module.onGameExit(); });
  return;
#endif
  rlmachine_.reset();
  sdl_system_.reset();
  arc_.reset();
  gameexe_.reset();
}

boost::filesystem::path RLVMInstance::SelectGameDirectory() {
  return boost::filesystem::path();
}

void RLVMInstance::ReportFatalError(const std::string& message_text,
                                    const std::string& informative_text) {
  std::cerr << message_text << ": " << informative_text << std::endl;
}

void RLVMInstance::DoUserNameCheck(RLMachine& machine) {
  try {
    int encoding = machine.GetProbableEncodingType();

    // Iterate over all the names in both global and local memory banks.
    GlobalMemory& g = machine.memory().global();
    for (int i = 0; i < SIZE_OF_NAME_BANK; ++i)
      cp932toUTF8(g.global_names[i], encoding);

    LocalMemory& l = machine.memory().local();
    for (int i = 0; i < SIZE_OF_NAME_BANK; ++i)
      cp932toUTF8(l.local_names[i], encoding);
  }
  catch (...) {
    // We've failed to interpret one of the name strings as a string in the
    // text encoding of the current native encoding. We're going to fail to
    // display any line that refers to the player's name.
    //
    // That's obviously bad and there's no real way to recover from this so
    // just reset all of global memory.
    if (AskUserPrompt(
            _("Corrupted global memory"),
            _("You appear to have run this game without a translation patch "
              "previously. This can cause lines of text to not print."),
            _("Reset"),
            _("Continue with broken names"))) {
      machine.HardResetMemory();
    }
  }
}

boost::filesystem::path RLVMInstance::FindGameFile(
    const boost::filesystem::path& gamerootPath,
    const std::string& filename) {
  fs::path search_for = gamerootPath / filename;
  fs::path corrected_path = CorrectPathCase(search_for);
  if (corrected_path.empty()) {
    throw rlvm::UserPresentableError(
        _("Could not load game"),
        str(format(_("Could not open %1%. Please make sure it exists.")) %
            search_for));
  }

  return corrected_path;
}

void RLVMInstance::CheckBadEngine(const boost::filesystem::path& gamerootPath,
                                  const char** filenames,
                                  const std::string& message_text) {
  for (const char** cur_file = filenames; *cur_file; cur_file++) {
    if (fs::exists(CorrectPathCase(gamerootPath / *cur_file))) {
      throw rlvm::UserPresentableError(message_text,
                                       _("rlvm can only play RealLive games."));
    }
  }
}
