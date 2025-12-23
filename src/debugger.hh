#pragma once

#include <trieste/trieste.h>

namespace gitmem {
  int interpret_interactive(const trieste::Node,
                            const std::filesystem::path &output_file);
}