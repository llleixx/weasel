#pragma once
#include <cstdint>

namespace weasel {

// TSF's host policy is independent of composition and document focus. An
// internal hide must not change that policy; a later key must not undo a
// host's ITfUIElement::Show(FALSE), either.
class CandidateUIState {
 public:
  void Begin(bool host_allowed) {
    started_ = true;
    host_allowed_ = host_allowed;
    focused_ = true;
  }
  void End() { started_ = false; }
  void SetHostAllowed(bool allowed) { host_allowed_ = allowed; }
  void SetFocused(bool focused) { focused_ = focused; }
  void SetComposing(bool composing) { composing_ = composing; }
  bool started() const { return started_; }
  bool host_allowed() const { return host_allowed_; }
  bool visible() const {
    return started_ && host_allowed_ && focused_ && composing_;
  }
  bool AcceptsAction(uint32_t token, uint32_t current_token) const {
    return visible() && token && token == current_token;
  }

 private:
  bool started_ = false;
  bool host_allowed_ = true;
  bool focused_ = true;
  bool composing_ = false;
};

}  // namespace weasel
