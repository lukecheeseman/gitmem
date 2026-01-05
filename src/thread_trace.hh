#pragma once

#include "thread_id.hh"
#include "conflict.hh"

namespace gitmem {

struct Event;

struct StartEvent {};
struct SpawnEvent { const ThreadID child_tid; };
struct ReadEvent { const std::string var; const size_t value; };
struct WriteEvent { const std::string var; const size_t value; };
struct LockEvent { std::string lock_name; std::unique_ptr<ConflictBase> maybe_conflict; std::shared_ptr<Event> last_unlock_event; };
struct UnlockEvent { const std::string lock_name; std::unique_ptr<ConflictBase> maybe_conflict; };
struct JoinEvent { const ThreadID joinee_tid; std::unique_ptr<ConflictBase> maybe_conflict; };
struct AssertEvent { const std::string condition; };

struct EndEvent {};

using EventID = size_t;

struct Event {
  ThreadID tid;
  EventID eid;
  std::variant<
    StartEvent,
    SpawnEvent,
    ReadEvent,
    WriteEvent,
    LockEvent,
    UnlockEvent,
    JoinEvent,
    AssertEvent,
    EndEvent
  > data;
};

inline std::string event_header(const Event& e) {
  std::ostringstream oss;
  oss << "[tid=" << e.tid << ", eid=" << e.eid << "]";
  return oss.str();
}

// --- operator<< overloads for individual event types ---
inline std::ostream& operator<<(std::ostream& os, const StartEvent&) {
  return os << "StartEvent";
}

inline std::ostream& operator<<(std::ostream& os, const SpawnEvent& e) {
  return os << "SpawnEvent(child_tid=" << e.child_tid << ")";
}

inline std::ostream& operator<<(std::ostream& os, const ReadEvent& e) {
  return os << "ReadEvent(var=\"" << e.var << "\", value=" << e.value << ")";
}

inline std::ostream& operator<<(std::ostream& os, const WriteEvent& e) {
  return os << "WriteEvent(var=\"" << e.var << "\", value=" << e.value << ")";
}

inline std::ostream& operator<<(std::ostream& os, const LockEvent& e) {
  os << "LockEvent(lock_name=\"" << e.lock_name << "\"";
  if (e.last_unlock_event)
    os << ", last unlock " << event_header(*e.last_unlock_event);
  if (e.maybe_conflict)
    os << ", conflict)";
  else
    os << ")";
  return os;
}

inline std::ostream& operator<<(std::ostream& os, const UnlockEvent& e) {
  os << "UnlockEvent(lock_name=\"" << e.lock_name << "\"";
  if (e.maybe_conflict)
    os << ", conflict)";
  else
    os << ")";
  return os;
}

inline std::ostream& operator<<(std::ostream& os, const JoinEvent& e) {
  os << "JoinEvent(joinee_tid=" << e.joinee_tid;
  if (e.maybe_conflict)
    os << ", conflict)";
  else
    os << ")";
  return os;
}

inline std::ostream& operator<<(std::ostream& os, const AssertEvent& e) {
  return os << "AssertEvent(condition=\"" << e.condition << "\")";
}

inline std::ostream& operator<<(std::ostream& os, const EndEvent&) {
  return os << "EndEvent";
}

// --- operator<< for the wrapper Event ---
inline std::ostream& operator<<(std::ostream& os, const Event& e) {
  os << event_header(e) << " ";
  std::visit([&os](auto&& arg) { os << arg; }, e.data);
  return os;
}

static EventID next_eid = 0;

struct ThreadTrace {
  std::vector<std::shared_ptr<Event>> trace;
  ThreadID tid;

  auto begin() { return trace.begin(); }
  auto end()   { return trace.end(); }

  auto begin() const { return trace.begin(); }
  auto end()   const { return trace.end(); }

  explicit ThreadTrace(ThreadID tid) : tid(tid) {}

private:
  template<class T, class... Args>
  std::shared_ptr<Event> append(Args&&... args) {
    auto event = std::make_shared<Event>(tid, next_eid++, T(std::forward<Args>(args)...));
    trace.push_back(event);
    return event;
  }

  public:
  std::shared_ptr<Event> on_start() {
    return append<StartEvent>();
  }

  std::shared_ptr<Event> on_spawn(ThreadID child_tid) {
    return append<SpawnEvent>(child_tid);
  }

  std::shared_ptr<Event> on_read(const std::string text, const size_t value) {
    return append<ReadEvent>(std::move(text), value);
  }

  std::shared_ptr<Event> on_write(const std::string text, const size_t value) {
    return append<WriteEvent>(std::move(text), value);
  }

  std::shared_ptr<Event> on_lock(const std::string lock_name,
                                 std::shared_ptr<Event> last_unlock_event,
                                 std::unique_ptr<ConflictBase> conflict = nullptr) {
    return append<LockEvent>(std::move(lock_name), std::move(conflict), last_unlock_event);
  }

  std::shared_ptr<Event> on_unlock(const std::string lock_name, std::unique_ptr<ConflictBase> conflict = nullptr) {
    return append<UnlockEvent>(std::move(lock_name), std::move(conflict));
  }

  std::shared_ptr<Event> on_join(ThreadID tid, std::unique_ptr<ConflictBase> conflict = nullptr) {
    return append<JoinEvent>(tid, std::move(conflict));
  }

  std::shared_ptr<Event> on_assert_fail(std::string expr) {
    return append<AssertEvent>(std::move(expr));
  }

  std::shared_ptr<Event> on_end() {
    return append<EndEvent>();
  }
};


// --- operator<< for ThreadTrace ---
inline std::ostream& operator<<(std::ostream& os, const ThreadTrace& tt) {
  os << "ThreadTrace[" << tt.trace.size() << " events]:\n";
  for (size_t i = 0; i < tt.trace.size(); ++i) {
    os << "  " << i << ": " << *(tt.trace[i]) << "\n";
  }
  return os;
}

} // namespace gitmem

// template <typename T, typename... Args>
// std::shared_ptr<T> thread_append_node(ThreadContext &ctx, Args &&...args) {
//   assert(ctx.tail);
//   auto node = std::make_shared<T>(std::forward<Args>(args)...);
//   ctx.tail->next = node;
//   ctx.tail = node;
//   return node;
// }

// template <>
// std::shared_ptr<graph::Pending>
// thread_append_node<graph::Pending>(ThreadContext &ctx, std::string &&stmt) {
//   // pending nodes don't update the tail position as we will destroy them
//   // once we execute the node
//   auto s = std::regex_replace(stmt, std::regex("\n"), "\\l   ");
//   auto node = make_shared<graph::Pending>(std::move(s));
//   ctx.tail->next = node;
//   return node;
// }