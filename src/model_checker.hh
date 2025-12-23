#pragma once

#include <trieste/trieste.h>
#include "sync_protocol.hh"

namespace gitmem {
  using namespace trieste;

  int model_check(const Node ast, const std::filesystem::path &output_path, SyncKind sync_kind);
}