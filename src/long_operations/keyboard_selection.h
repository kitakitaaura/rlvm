// Arrow keys and Enter for rlvm's selections (text-window choices, SELBTN
// choice buttons, object-button menus such as Clannad's title screen).
//
// rlvm only ever handled these with the mouse, while RealLive itself also
// takes the keyboard. That left a phone's on-screen d-pad (noberu's
// assets/touch.js), and any keyboard, unable to pick anything. This moves the
// cursor the way a mouse would: arrows inject a mouse move to the centre of
// the next option, Enter injects a click on the highlighted one. Each
// selection keeps doing its own hit-testing, highlighting and sound, so
// nothing else about them changes.

#ifndef SRC_LONG_OPERATIONS_KEYBOARD_SELECTION_H_
#define SRC_LONG_OPERATIONS_KEYBOARD_SELECTION_H_

#include <algorithm>
#include <memory>
#include <vector>

#include "long_operations/wait_long_operation.h"
#include "machine/long_operation.h"
#include "machine/rlmachine.h"
#include "systems/base/graphics_object.h"
#include "systems/base/graphics_object_data.h"
#include "systems/base/graphics_system.h"
#include "systems/base/parent_graphics_object_data.h"
#include "systems/base/event_listener.h"
#include "systems/base/event_system.h"
#include "systems/base/rect.h"
#include "systems/base/system.h"

// What the keyboard has done so far, across calls.
struct KeyboardSelectionState {
  bool clicking = false;         // Enter's press was turned into a mouse press
  bool arrow_held = false;       // an arrow's press was ours; so is its release
  bool navigated = false;        // the cursor sits where an arrow put it...
  Point nav_point;               // ...which is here,
  std::vector<Rect> nav_options; // ...among these options.
};

inline KeyboardSelectionState& KeyboardSelection() {
  static KeyboardSelectionState state;
  return state;
}

// Releases need no look at the screen: they belong to us exactly when the
// matching press did. Returns true if |key|'s release was ours.
inline bool HandleSelectionKeyRelease(RLMachine& machine, KeyCode key) {
  KeyboardSelectionState& state = KeyboardSelection();
  if (key == RLKEY_RETURN || key == RLKEY_KP_ENTER) {
    if (!state.clicking)
      return false;
    state.clicking = false;
    machine.system().event().InjectMouseUp(machine);
    return true;
  }
  if (key == RLKEY_UP || key == RLKEY_DOWN || key == RLKEY_LEFT ||
      key == RLKEY_RIGHT) {
    bool ours = state.arrow_held;
    state.arrow_held = false;
    return ours;
  }
  return false;
}

// |options| are the on-screen rectangles of the options that can be picked,
// in any order. Returns true if the key was one of ours.
inline bool HandleSelectionKey(RLMachine& machine,
                               std::vector<Rect> options,
                               KeyCode key,
                               bool pressed) {
  if (!pressed)
    return HandleSelectionKeyRelease(machine, key);

  const bool move_back = key == RLKEY_UP || key == RLKEY_LEFT;
  const bool move_on = key == RLKEY_DOWN || key == RLKEY_RIGHT;
  const bool pick = key == RLKEY_RETURN || key == RLKEY_KP_ENTER;
  if (!move_back && !move_on && !pick)
    return false;

  options.erase(std::remove_if(options.begin(), options.end(),
                               [](const Rect& r) {
                                 return r.width() <= 0 || r.height() <= 0;
                               }),
                options.end());
  if (options.empty())
    return false;

  // Reading order: top to bottom, then left to right.
  std::sort(options.begin(), options.end(), [](const Rect& a, const Rect& b) {
    return a.y() != b.y() ? a.y() < b.y() : a.x() < b.x();
  });

  KeyboardSelectionState& state = KeyboardSelection();
  EventSystem& events = machine.system().event();
  const Point cursor = events.GetCursorPos();

  // Enter clicks only an option the arrows moved to, with the cursor still
  // there and the same options on screen. Anywhere else it keeps its usual
  // meaning: a tap leaves the cursor wherever it landed, and clicking
  // whatever happens to sit under it on a later screen is not what Enter
  // means there.
  if (pick) {
    if (!state.navigated || !(cursor == state.nav_point) ||
        options != state.nav_options)
      return false;
    // A real click has a press and, later, a release (on Enter's key up):
    // scripts that poll the mouse between frames miss a same-instant pair.
    state.clicking = true;
    events.InjectMouseDown(machine);
    return true;
  }

  int current = -1;
  for (size_t i = 0; i < options.size(); ++i) {
    if (options[i].Contains(cursor)) {
      current = static_cast<int>(i);
      break;
    }
  }

  const int count = static_cast<int>(options.size());
  int next;
  if (current < 0)
    next = move_on ? 0 : count - 1;
  else
    next = (current + (move_on ? 1 : count - 1)) % count;

  const Rect& target = options[next];
  const Point point(target.x() + target.width() / 2,
                    target.y() + target.height() / 2);
  state.arrow_held = true;
  state.navigated = true;
  state.nav_point = point;
  state.nav_options = options;
  events.InjectMouseMovement(machine, point);
  return true;
}

// On-screen rectangles of what a script-driven menu offers: the visible
// button objects, or failing those, the largest set of identically sized
// images lined up in one column or row.
inline std::vector<Rect> MenuOptionRects(RLMachine& machine) {
  GraphicsSystem& graphics = machine.system().graphics();
  std::vector<Rect> buttons;  // objects marked as buttons (objBtn)
  std::vector<Rect> shown;    // every visible object
  auto add = [&](GraphicsObject& obj, GraphicsObject* parent) {
    if (!obj.visible() || !obj.has_object_data() ||
        obj.GetObjectData().IsParentLayer())
      return;
    Rect rect;
    try {
      rect = obj.GetObjectData().DstRect(obj, parent);
    } catch (...) {
      return;  // an object mid-change (an animation, say): not an option
    }
    if (rect.width() <= 0 || rect.height() <= 0)
      return;  // nothing drawn (an empty text object, a GAN between frames)
    shown.push_back(rect);
    if (obj.IsButton())
      buttons.push_back(rect);
  };
  for (GraphicsObject& obj : graphics.GetForegroundObjects()) {
    add(obj, NULL);
    if (obj.has_object_data()) {
      ParentGraphicsObjectData* children =
          dynamic_cast<ParentGraphicsObjectData*>(&obj.GetObjectData());
      if (children) {
        for (GraphicsObject& child : children->objects())
          add(child, &obj);
      }
    }
  }
  if (!buttons.empty())
    return buttons;

  // No buttons: the script hit-tests plain images itself (Clannad's title
  // screen is seven 342x46 images in a column). Take the largest set of two
  // or more identically sized images lined up in one column or row, leaving
  // out anything big enough to be a background or a panel.
  const Size& screen = graphics.screen_size();
  std::vector<Rect> best;

  // Or a staircase: Little Busters' title is four 84x14 images, each 30px
  // right of and 24px below the one before. Every same-sized set whose
  // positions step by the same amount counts as one line.
  for (const Rect& candidate : shown) {
    if (candidate.width() * 2 > screen.width() ||
        candidate.height() * 4 > screen.height())
      continue;
    std::vector<Rect> same;
    for (const Rect& other : shown) {
      if (other.size() == candidate.size() &&
          std::none_of(same.begin(), same.end(), [&](const Rect& r) {
            return r.origin() == other.origin();
          }))
        same.push_back(other);
    }
    if (same.size() < 3 || same.size() <= best.size())
      continue;
    std::sort(same.begin(), same.end(), [](const Rect& a, const Rect& b) {
      return a.y() != b.y() ? a.y() < b.y() : a.x() < b.x();
    });
    const int dx = same[1].x() - same[0].x(), dy = same[1].y() - same[0].y();
    bool even = true;
    for (size_t i = 2; i < same.size(); ++i) {
      if (same[i].x() - same[i - 1].x() != dx ||
          same[i].y() - same[i - 1].y() != dy)
        even = false;
    }
    if (even)
      best = same;
  }

  for (const Rect& candidate : shown) {
    if (candidate.width() * 2 > screen.width() ||
        candidate.height() * 4 > screen.height())
      continue;
    std::vector<Rect> column, row;
    for (const Rect& other : shown) {
      if (other.width() != candidate.width() ||
          other.height() != candidate.height())
        continue;
      if (other.x() == candidate.x() &&
          std::none_of(column.begin(), column.end(),
                       [&](const Rect& r) { return r.y() == other.y(); }))
        column.push_back(other);
      if (other.y() == candidate.y() &&
          std::none_of(row.begin(), row.end(),
                       [&](const Rect& r) { return r.x() == other.x(); }))
        row.push_back(other);
    }
    std::vector<Rect>& line = column.size() >= row.size() ? column : row;
    if (line.size() >= 2 && line.size() > best.size())
      best = line;
  }
  return best;
}

// For keys that arrive while no selection op is showing options: a script
// waiting on the mouse (Clannad's title screen polls the cursor between
// short waits, so a key can land between two waits with no op at all).
// Never while text is being read, where arrows and Enter already mean
// something. Called by the event system before it dispatches the key.
inline bool HandleMenuKey(RLMachine& machine, KeyCode key, bool pressed) {
  if (!pressed)
    return HandleSelectionKeyRelease(machine, key);
  // Only the keys a menu uses look for one. Ctrl (skip) and the rest are the
  // game's, and are pressed through busy scenes - Little Busters' fights and
  // opening - where scanning every object is wasted at best.
  if (key != RLKEY_UP && key != RLKEY_DOWN && key != RLKEY_LEFT &&
      key != RLKEY_RIGHT && key != RLKEY_RETURN && key != RLKEY_KP_ENTER)
    return false;
  std::shared_ptr<LongOperation> op = machine.CurrentLongOperation();
  if (op && !dynamic_cast<WaitLongOperation*>(op.get()))
    return false;
  // A keyboard convenience must never be what stops a game.
  try {
    return HandleSelectionKey(machine, MenuOptionRects(machine), key, pressed);
  } catch (...) {
    return false;
  }
}

#endif  // SRC_LONG_OPERATIONS_KEYBOARD_SELECTION_H_
