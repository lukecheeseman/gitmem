#pragma once

#include "thread_id.hh"
#include "graph.hh"

namespace gitmem {

struct ThreadTrace {
  ThreadID tid;
  std::shared_ptr<graph::Node> head;
  std::shared_ptr<graph::Node> tail;

private:
  template<class T, class... Args>
  void append(Args&&... args) {
    assert(tail);
    auto node = std::make_shared<T>(std::forward<Args>(args)...);
    tail->next = node;
    tail = node;
  }

public:
  explicit ThreadTrace(ThreadID tid): tid(tid), head(nullptr), tail(nullptr) {}

  void on_start(ThreadID tid) {
    assert(head == tail && head == nullptr);
    head = std::make_shared<graph::Start>(tid);
    tail = head;
  }

  void on_stmt(std::string text) {
    append<graph::Pending>(std::move(text));
  }

  void on_lock(std::string lock, std::shared_ptr<graph::Node> last) {
    append<graph::Lock>(std::move(lock), last);
  }

  void on_unlock(std::string lock) {
    append<graph::Unlock>(std::move(lock));
  }

  void on_join(ThreadID tid, std::shared_ptr<graph::Node> target) {
    append<graph::Join>(tid, target);
  }

  void on_assert_fail(std::string expr) {
    append<graph::AssertionFailure>(std::move(expr));
  }

  void on_end() {
    append<graph::End>();
  }
};

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