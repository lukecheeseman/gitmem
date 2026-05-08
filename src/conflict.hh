#pragma once

#include <iostream>
#include <optional>
#include <string>

namespace gitmem {

struct FileLocation {
  std::string file;
  size_t line = 0;
  size_t column = 0;

  std::string linecol() const {
    return std::to_string(line) + ":" + std::to_string(column);
  }

  std::string str() const {
    if (file.empty()) {
      return linecol();
    } else {
      return file + ":" + linecol();
    }
  }
};

inline std::ostream& operator<<(std::ostream& os, const FileLocation& loc) {
  os << loc.str();
  return os;
}

struct ConflictBase {
  virtual ~ConflictBase() = default;
  virtual std::ostream &print(std::ostream &os) const = 0;
  virtual std::string object_name() const = 0;
  virtual std::pair<FileLocation, FileLocation> source_locations() const = 0;
  friend std::ostream &operator<<(std::ostream &os,
                                  const ConflictBase &conflict) {
    return conflict.print(os);
  }
  virtual bool operator==(const ConflictBase &other) const = 0;
};

template <typename VersionID>
struct Conflict : ConflictBase {
  std::string var;
  VersionID version_a;
  VersionID version_b;
  FileLocation location_a;
  FileLocation location_b;

  Conflict(std::string var,
           VersionID version_a,
           VersionID version_b,
           FileLocation location_a,
           FileLocation location_b)
      : var(std::move(var)), version_a(std::move(version_a)), version_b(std::move(version_b)),
        location_a(std::move(location_a)), location_b(std::move(location_b)) {}

  std::ostream &print(std::ostream &os) const override;

  std::string object_name() const override {
    return var;
  }

  std::pair<FileLocation, FileLocation> source_locations() const override {
    return std::make_pair(location_a, location_b);
  }

  bool operator==(const Conflict &other) const {
    // Ignore the FileLocation information for equality, as it is only for reporting purposes
    return var == other.var && version_a == other.version_a && version_b == other.version_b;
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
  os << "conflict on " << var << " { " << version_a << ", "
     << version_b << " }";
  return os;
}


}