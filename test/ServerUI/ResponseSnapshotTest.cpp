#include <ResponseParser.h>
#include <iostream>
#include <ServerUIRevision.h>

int main() {
  std::wstring pipe_buffer =
      L"action=commit,ctx,status,config\ncommit=first\nctx.preedit=ni\n"
      L"ui_revision=1\nstatus.composing=1\nconfig.inline_preedit=1\n.\n";
  weasel::ResponseSnapshot first;
  if (!first.Parse(pipe_buffer.data(), static_cast<UINT>(pipe_buffer.size())))
    return 1;
  // A second transaction overwrites the backing storage before TSF grants
  // the first edit session. Each queued edit must retain its own payload.
  pipe_buffer.assign(
      L"action=commit,ctx,status,config\ncommit=second\n"
      L"ctx.preedit=hao\nstatus.composing=0\n"
      L"ui_revision=2\nconfig.inline_preedit=0\n.\n");
  weasel::ResponseSnapshot second;
  if (!second.Parse(pipe_buffer.data(), static_cast<UINT>(pipe_buffer.size())))
    return 2;
  pipe_buffer.clear();
  if (first.ui_revision != 1 || second.ui_revision != 2)
    return 4;
  weasel::ServerUIRevision revision;
  const auto old_revision = revision.Advance();
  const auto new_revision = revision.Advance();
  if (!revision.pending() || revision.Acknowledge(old_revision) ||
      !revision.Acknowledge(new_revision) || revision.pending() ||
      revision.Acknowledge(0))
    return 5;
  DWORD parsed = 0;
  if (!weasel::ParseServerUIRevision(L"4294967295", parsed) ||
      parsed != MAXDWORD ||
      weasel::ParseServerUIRevision(L"4294967296", parsed) ||
      weasel::ParseServerUIRevision(L"-1", parsed) ||
      weasel::ParseServerUIRevision(L"1x", parsed))
    return 6;
  if (first.commit != L"first" || first.context.preedit.str != L"ni" ||
      !first.status.composing || !first.config.inline_preedit ||
      second.commit != L"second" || second.context.preedit.str != L"hao" ||
      second.status.composing || second.config.inline_preedit)
    return 3;
  std::cout << "PASS: deferred responses own independent text and state\n";
}
