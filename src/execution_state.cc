#include <regex>

#include "execution_state.hh"
#include "sync_protocol.hh"

namespace gitmem {

ThreadContext::ThreadContext(ThreadID tid, SyncKind sync_kind) {
  switch (sync_kind) {
    case SyncKind::Linear:
      sync.emplace<LinearData>();
      break;
    case SyncKind::Branching:
      sync.emplace<BranchingData>(tid);
      break;
  }
}

bool ThreadContext::operator==(const ThreadContext &other) const {
  if (locals != other.locals)
    return false;

  // ignore the graph node, we're not interested in that

  if (sync.index() != other.sync.index())
    return false;

  return std::visit([&](const auto& a, const auto& b) -> bool {
    using A = std::decay_t<decltype(a)>;
    using B = std::decay_t<decltype(b)>;

    if constexpr (std::is_same_v<A, std::monostate> &&
                  std::is_same_v<B, std::monostate>) {
      return true;
    } else if constexpr (std::is_same_v<A, B>) {
      return a.store == b.store;
    } else {
      return false; // unreachable due to index check
    }
  }, sync, other.sync);
}

// An old comment on equals
// Globals have a history that we don't care about, so we only
// compare values
// if (ctx.globals.size() != other.ctx.globals.size())
//     return false;
// for (const auto &[var, global] : ctx.globals)
// {
//     if (!other.ctx.globals.contains(var) ||
//         ctx.globals.at(var).val != other.ctx.globals.at(var).val)
//     {
//         return false;
//     }
// }

bool Thread::operator==(const Thread &other) const {
  return ctx == other.ctx &&
          block == other.block &&
          pc == other.pc &&
          terminated == other.terminated;
}

GlobalContext::GlobalContext(const trieste::Node &ast,
                             std::unique_ptr<SyncProtocol> protocol)
    : protocol(std::move(protocol)) {
  trieste::Node starting_block = ast / lang::File / lang::Block;

  ThreadID main_tid = 0;

  ThreadContext starting_ctx(main_tid, this->protocol->kind());

  this->threads.emplace_back(main_tid, std::move(starting_ctx), starting_block);
  this->locks = {};
  this->cache = {};
}

GlobalContext::~GlobalContext() = default;

// void GlobalContext::print_execution_graph(
//     const std::filesystem::path &output_path) const {
//   return; // FIXME
//   // Loop over the threads and add pending nodes to running threads
//   // to indicate a threads next step
//   for (const auto &t : threads) {
//     assert(t->ctx.tail);
//     if (t->terminated ||
//         dynamic_pointer_cast<const graph::Pending>(t->ctx.tail->next))
//       continue;

//     trieste::Node block = t->block;
//     size_t &pc = t->pc;
//     trieste::Node stmt = block->at(pc);
//     thread_append_node<graph::Pending>(t->ctx,
//                                        std::string(stmt->location().view()));
//   }

//   graph::GraphvizPrinter gv(output_path);
//   gv.visit(entry_node.get());
// }

bool GlobalContext::operator==(const GlobalContext &other) const {
  if (threads.size() != other.threads.size() ||
      locks.size() != other.locks.size())
    return false;

  // Threads may have been spawned in a different order, so we
  // find the thread with the same block in the other context
  for (auto &thread : threads) {
    auto it = std::find_if(other.threads.begin(), other.threads.end(),
                            [&thread](auto &t)
                            { return t.block == thread.block; });
    if (it == other.threads.end() || !(thread == *it))
      return false;
  }

  for (auto &[name, lock] : locks) {
    if (!other.locks.contains(name))
      return false;
    auto &other_lock = other.locks.at(name);
    if (lock.owner != other_lock.owner)
      return false;
  }
  return true;
}

/** Print the state of a thread, including its local and global variables,
 * and the current position in the program. */
std::ostream& operator<<(std::ostream& os, const Thread& thread) {
  os << thread.ctx << std::endl;

  size_t idx = 0;
  for (const auto &stmt : *(thread.block)) {
    if (idx == thread.pc) {
      os << "-> ";
    } else {
      os << "   ";
    }

    // This should be somewhere else
    // Fix indentation of nested blocks
    auto s = std::string(stmt->location().view());
    s = std::regex_replace(s, std::regex("\n"), "\n   ");
    os << s << ";" << std::endl;

    idx++;
  }
  if (thread.pc == thread.block->size()) {
    os << "-> " << std::endl;
  }

  return os;
}

std::ostream& operator<<(std::ostream& os, const ThreadContext& ctx) {
  os << "ThreadContext{locals={";

  bool first = true;
  for (const auto& [k, v] : ctx.locals) {
    if (!first) os << ", ";
    first = false;
    os << k << "=" << v;
  }

  os << "}"; //, tail=" << ctx.tail;

  std::visit([&](const auto& data) {
    using T = std::decay_t<decltype(data)>;

    if constexpr (std::is_same_v<T, ThreadContext::LinearData>) {
      os << ", sync=linear{" << data.store << "}";
    } else if constexpr (std::is_same_v<T, ThreadContext::BranchingData>) {
      os << ", sync=branching{" << data.store << "}";
    }
  }, ctx.sync);

  os << "}";
  return os;
}

void show_lock(const std::string &lock_name, const struct Lock &lock) {
  std::cout << lock_name << ": ";
  if (lock.owner) {
    std::cout << "held by thread " << *lock.owner;
  } else {
    std::cout << "<free>";
  }
  std::cout << std::endl;
  // for (auto &[var, global] : lock.globals) {
  //   show_global(var, global);
  // }
}

void GlobalContext::print(std::ostream& os, bool show_all) const {
  os << *protocol << std::endl;

  bool showed_any = false;
  for (size_t i = 0; i < threads.size(); i++) {
    auto& thread = threads[i];
    if (show_all || !thread.terminated ||
        *threads[i].terminated != TerminationStatus::completed) {
      os << "---- Thread " << i << std::endl;
      os << threads[i] << std::endl;
      os << std::endl;
      showed_any = true;
    }
  }

  if (showed_any && locks.size() > 0) {
    os << "---- Locks" << std::endl;

    for (const auto &[lock_name, lock] : locks) {
      show_lock(lock_name, lock);
    }

    if (locks.size() > 0)
      os << "--" << std::endl;
  }
}

std::ostream& operator<<(std::ostream& os, const GlobalContext& gctx) {
  gctx.print(os);
  return os;
}


} // namespace gitmem