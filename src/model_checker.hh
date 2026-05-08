#pragma once

#include <trieste/trieste.h>
#include "memory_model.hh"
#include <functional>

namespace gitmem {
  using namespace trieste;

  using MemoryModelFactory = std::function<std::unique_ptr<MemoryModel>()>;

  int model_check(const Node ast, const std::filesystem::path &output_path,
                  const MemoryModelFactory& make_model);
}