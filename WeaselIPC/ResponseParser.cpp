#include "stdafx.h"
#include <StringAlgorithm.hpp>
#include <WeaselIPC.h>
#include "Deserializer.h"
#include <ServerUIRevision.h>

using namespace weasel;

bool ResponseSnapshot::Parse(LPWSTR buffer, UINT length) {
  ResponseParser parser(&commit, &context, &status, &config, &style,
                        &ui_revision);
  return parser(buffer, length);
}

ResponseParser::ResponseParser(std::wstring* commit,
                               Context* context,
                               Status* status,
                               Config* config,
                               UIStyle* style,
                               DWORD* ui_revision)
    : p_commit(commit),
      p_context(context),
      p_status(status),
      p_config(config),
      p_style(style),
      p_ui_revision(ui_revision) {
  Deserializer::Initialize(this);
}

bool ResponseParser::operator()(LPWSTR buffer, UINT length) {
  wbufferstream bs(buffer, length);
  std::wstring line;
  while (bs.good()) {
    std::getline(bs, line);
    if (!bs.good())
      return false;

    // file ends
    if (line == L".")
      break;

    Feed(line);
  }
  return bs.good();
}

void ResponseParser::Feed(const std::wstring& line) {
  // ignore blank lines and comments
  if (line.empty() || line.find_first_of(L'#') == 0)
    return;

  if (p_ui_revision && line.compare(0, 12, L"ui_revision=") == 0) {
    ParseServerUIRevision(line.substr(12), *p_ui_revision);
    return;
  }

  Deserializer::KeyType key;
  std::wstring value;

  // extract key (split by L'.') and value
  std::wstring::size_type sep_pos = line.find_first_of(L'=');
  if (sep_pos == std::wstring::npos)
    return;
  split(key, line.substr(0, sep_pos), L".");
  if (key.empty())
    return;
  value = line.substr(sep_pos + 1);

  // first part of the key serve as action type
  std::wstring const& action = key[0];

  // get required action deserializer instance
  std::map<std::wstring, Deserializer::Ptr>::iterator i =
      deserializers.find(action);
  if (i == deserializers.end()) {
    // line ignored... since corresponding deserializer is not active
    return;
  }

  // dispatch
  Deserializer::Ptr p = i->second;
  p->Store(key, value);
}
