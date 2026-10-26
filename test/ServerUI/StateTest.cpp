#include <CandidateUIState.h>
#include <iostream>

int main() {
  weasel::CandidateUIState state;
  state.Begin(false);
  state.SetComposing(true);
  if (state.visible())
    return 1;  // Host renders the candidates itself.
  state.SetHostAllowed(true);
  if (!state.visible())
    return 2;
  state.SetHostAllowed(false);
  state.SetComposing(false);
  state.SetComposing(true);
  if (state.visible())
    return 3;  // A key cannot undo Show(FALSE).
  state.SetHostAllowed(true);
  state.SetFocused(false);
  state.SetComposing(true);
  if (state.visible())
    return 4;  // Late response from unfocused context.
  state.SetFocused(true);
  if (!state.visible())
    return 5;
  if (state.AcceptsAction(0, 0) || state.AcceptsAction(71, 72) ||
      !state.AcceptsAction(72, 72))
    return 8;
  state.End();
  if (state.visible())
    return 6;
  if (state.AcceptsAction(72, 72))
    return 9;
  state.Begin(false);
  if (state.visible())
    return 7;  // New negotiation replaces old policy.
  std::cout << "PASS: host policy, focus and composition are independent\n";
}
