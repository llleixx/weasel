#pragma once
#include <WeaselUI.h>
#include <CandidateUIState.h>
#include "ctffunc.h"

class WeaselTSF;

class CCandidateList : public ITfIntegratableCandidateListUIElement,
                       public ITfCandidateListUIElementBehavior {
 public:
  friend class WeaselTSF;
  CCandidateList(com_ptr<WeaselTSF> pTextService);
  ~CCandidateList();

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, _Outptr_ void** ppvObj);
  STDMETHODIMP_(ULONG) AddRef(void);
  STDMETHODIMP_(ULONG) Release(void);

  // ITfUIElement
  STDMETHODIMP GetDescription(BSTR* pbstr);
  STDMETHODIMP GetGUID(GUID* pguid);
  STDMETHODIMP Show(BOOL showCandidateWindow);
  STDMETHODIMP IsShown(BOOL* pIsShow);

  // ITfCandidateListUIElement
  STDMETHODIMP GetUpdatedFlags(DWORD* pdwFlags);
  STDMETHODIMP GetDocumentMgr(ITfDocumentMgr** ppdim);
  STDMETHODIMP GetCount(UINT* pCandidateCount);
  STDMETHODIMP GetSelection(UINT* pSelectedCandidateIndex);
  STDMETHODIMP GetString(UINT uIndex, BSTR* pbstr);
  STDMETHODIMP GetPageIndex(UINT* pIndex, UINT uSize, UINT* puPageCnt);
  STDMETHODIMP SetPageIndex(UINT* pIndex, UINT uPageCnt);
  STDMETHODIMP GetCurrentPage(UINT* puPage);

  // ITfCandidateListUIElementBehavior methods
  STDMETHODIMP SetSelection(UINT nIndex);
  STDMETHODIMP Finalize(void);
  STDMETHODIMP Abort(void);

  // ITfIntegratableCandidateListUIElement methods
  STDMETHODIMP SetIntegrationStyle(GUID guidIntegrationStyle);
  STDMETHODIMP GetSelectionStyle(
      _Out_ TfIntegratableCandidateListSelectionStyle* ptfSelectionStyle);
  STDMETHODIMP OnKeyDown(_In_ WPARAM wParam,
                         _In_ LPARAM lParam,
                         _Out_ BOOL* pIsEaten);
  STDMETHODIMP ShowCandidateNumbers(_Out_ BOOL* pIsShow);
  STDMETHODIMP FinalizeExactCompositionString();

  /* Update */
  void InvalidateServerActions() { _InvalidateUIActions(); }
  void UpdateUI(const weasel::Context& ctx,
                const weasel::Status& status,
                DWORD revision);
  void UpdateStyle(const weasel::UIStyle& sty);
  void UpdateInputPosition(RECT const& rc);
  void Destroy();
  void DestroyAll();
  void StartUI();
  void EndUI();
  void ConnectServerUI();
  void SetFocused(bool focused);

  com_ptr<ITfContext> GetContextDocument();
  bool GetIsReposition() {
    if (_serverUI)
      return _remoteReposition;
    if (_ui)
      return _ui->GetIsReposition();
    else
      return false;
  }

  weasel::UIStyle& style();

 private:
  static LRESULT CALLBACK CallbackProc(HWND, UINT, WPARAM, LPARAM);
  HWND _callbackWindow = nullptr;
  bool _serverUI = false;
  bool _shown = false;
  DWORD _uiToken = 0;
  bool _remoteReposition = false;
  bool _remoteShown = false;
  DWORD _sentToken = 0;
  DWORD _revision = 0;
  DWORD _sentRevision = 0;
  HRESULT _UpdateVisibility();
  void _InvalidateUIActions();
  void _DestroyCallbackWindow();
  // void _UpdateOwner();
  HWND _GetActiveWnd();
  HRESULT _UpdateUIElement();

  // for CCandidateList::EndUI(), after ending composition ||
  // WeaselTSF::_EndUI()
  void _DisposeUIWindow();
  // for CCandidateList::Destroy(), when inputing app exit
  void _DisposeUIWindowAll();
  void _MakeUIWindow();

  std::unique_ptr<weasel::UI> _ui;
  DWORD _cRef;
  com_ptr<WeaselTSF> _tsf;
  DWORD uiid;
  TfIntegratableCandidateListSelectionStyle _selectionStyle =
      STYLE_ACTIVE_SELECTION;

  weasel::CandidateUIState _visibility;
  weasel::UIStyle _style;
  bool _ui_warmed_up = false;

  com_ptr<ITfContext> _pContextDocument;
};
