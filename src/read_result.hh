#pragma once

#include <variant>
#include "conflict.hh"

namespace gitmem {

using Value = size_t;
using ReadResult = std::variant<std::monostate, Value, std::unique_ptr<ConflictBase>>;

}