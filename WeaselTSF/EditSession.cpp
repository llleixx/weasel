#include "stdafx.h"
#include "WeaselTSF.h"
#include "CandidateList.h"
#include "ResponseParser.h"

HRESULT WeaselTSF::ApplyResponse(TfEditCookie ec,
                                 com_ptr<ITfContext> pContext,
                                 const weasel::ResponseSnapshot& response,
                                 DWORD activation) {
  if (!_pThreadMgr || activation != _activation)
    return S_FALSE;
  const auto& commit = response.commit;
  const auto& config = response.config;
  auto context = std::make_shared<weasel::Context>(response.context);
  _status = response.status;
  _cand->style() = response.style;

  _UpdateLanguageBar(_status);

  bool compositionEnded = false;
  if (_status.composing) {
    _pendingUIContext = context;
    _pendingUIStatus = _status;
    _pendingUIRevision = response.ui_revision;
    _hasPendingUI = true;
  } else {
    _pendingUIContext.reset();
    _hasPendingUI = false;
  }
  if (!commit.empty()) {
    // For auto-selecting, commit and preedit can both exist.
    // Commit the old TSF composition. If Rime immediately has a new
    // preedit (top-word input), compositionEnded ensures the following
    // state check starts a replacement TSF composition.
    if (!_IsComposing()) {
      _StartComposition(pContext,
                        _fCUASWorkaroundEnabled && !config.inline_preedit);
    }
    _InsertText(pContext, commit);
    // Keep the candidate UI alive while the replacement composition is
    // being created; otherwise the key-down path destroys the old window
    // and the new one cannot be positioned until key-up.
    _EndComposition(pContext, false, !_status.composing);
    compositionEnded = true;
    _committed = TRUE;
  } else {
    _committed = FALSE;
  }
  if (_status.composing && (compositionEnded || !_IsComposing())) {
    _StartComposition(pContext,
                      _fCUASWorkaroundEnabled && !config.inline_preedit);
  } else if (!_status.composing && _IsComposing()) {
    _EndComposition(pContext, true);
  }
  if (_IsComposing() && config.inline_preedit) {
    _ShowInlinePreedit(pContext, context);
  }

  // Publish candidates only after the current composition's text extent is
  // available; replacement compositions flush in _SetCompositionPosition().
  if (_IsComposing() && !compositionEnded)
    _UpdateCompositionWindow(pContext);
  if (!(_status.composing && _IsComposing()))
    _UpdateUI(*context, _status, response.ui_revision);

  return S_OK;
}
