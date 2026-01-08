#pragma once

#include <iostream>

namespace gitmem {

struct ConflictBase {
  virtual ~ConflictBase() = default;
  virtual std::ostream &print(std::ostream &os) const = 0;
  friend std::ostream &operator<<(std::ostream &os,
                                  const ConflictBase &conflict) {
    return conflict.print(os);
  }
  virtual bool operator==(const ConflictBase &other) const = 0;
};

template <typename VersionID>
struct Conflict : ConflictBase {
  std::string var;
  std::pair<VersionID, VersionID> versions;

  Conflict(std::string var, std::pair<VersionID, VersionID> versions)
      : var(std::move(var)), versions(std::move(versions)) {}

  std::ostream &print(std::ostream &os) const override;

  bool operator==(const Conflict &other) const {
    return var == other.var && versions == other.versions;
  }

  bool operator==(const ConflictBase& other) const override {
    auto* o = dynamic_cast<const Conflict*>(&other);
    if (!o)
      return false;
    return *this == *o;
  }
};

template <typename T>
std::ostream &Conflict<T>::print(std::ostream &os) const {
  os << "conflict on " << var << " { " << versions.first << ", "
     << versions.second << " }";
  return os;
}


}