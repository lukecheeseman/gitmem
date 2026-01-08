#pragma once

#include "branching/base_version_store.hh"

namespace gitmem {

namespace branching {

struct ReadConflict : ConflictBase {
  ObjectNumber obj;
  std::pair<Timestamp, Timestamp> versions;

  ReadConflict(ObjectNumber obj, std::pair<Timestamp, Timestamp> versions):
    obj(obj), versions(std::move(versions)) {}

  ~ReadConflict() = default;
  std::ostream &print(std::ostream &os) const override {
    return os;
  };

  bool operator==(const ReadConflict &other) const {
    return obj == other.obj && versions == other.versions;
  }

  bool operator==(const ConflictBase& other) const override {
    auto* o = dynamic_cast<const ReadConflict*>(&other);
    if (!o)
      return false;
    return *this == *o;
  }
};

class LazyLocalVersionStore : public LocalVersionStore {
public:
  ~LazyLocalVersionStore() = default;

  LazyLocalVersionStore(ThreadID tid) : LocalVersionStore(tid) {}

  std::optional<Conflict> merge_with_commit(const std::shared_ptr<const Commit>&) override;
  ReadResult get_committed(ObjectNumber number) const override;

};

}

}