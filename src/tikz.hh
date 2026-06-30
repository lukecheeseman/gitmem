#pragma once
#include "graph.hh"
#include <string>
#include <filesystem>

namespace gitmem {
namespace graph {

struct TikzPrinter {
  void print(const ExecutionGraph& g, const std::filesystem::path& path);
};

} // namespace graph
} // namespace gitmem
