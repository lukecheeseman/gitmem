#pragma once

#include <variant>
#include <memory>
#include "conflict.hh"

namespace gitmem {

struct Event;

using Value = size_t;

struct ValueWithSource {
  Value value;
  std::shared_ptr<Event> source_event;

  auto operator<=>(const ValueWithSource&) const = default;
};

using ReadResult = std::variant<std::monostate, ValueWithSource, std::shared_ptr<ConflictBase>>;

}