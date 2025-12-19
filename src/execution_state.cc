#include "execution_state.hh"
#include "sync_protocol.hh"

namespace gitmem {

bool Thread::operator==(const Thread &other) const {
  return false;
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
  // return ctx.locals == other.ctx.locals &&
  //         block == other.block &&
  //         pc == other.pc &&
  //         terminated == other.terminated;
}

GlobalContext::GlobalContext(const trieste::Node &ast,
                             std::unique_ptr<SyncProtocol> protocol)
    : protocol(std::move(protocol)) {
  trieste::Node starting_block = ast / lang::File / lang::Block;
  ThreadContext starting_ctx = {.locals = {},
                                .tail = std::make_shared<graph::Start>(0)};
  auto main_thread = std::make_shared<Thread>(starting_ctx, starting_block);

  this->threads = {main_thread};
  this->locks = {};
  this->cache = {};
}

GlobalContext::~GlobalContext() = default;

void GlobalContext::print_execution_graph(
    const std::filesystem::path &output_path) const {
  return; // FIXME
  // Loop over the threads and add pending nodes to running threads
  // to indicate a threads next step
  for (const auto &t : threads) {
    assert(t->ctx.tail);
    if (t->terminated ||
        dynamic_pointer_cast<const graph::Pending>(t->ctx.tail->next))
      continue;

    trieste::Node block = t->block;
    size_t &pc = t->pc;
    trieste::Node stmt = block->at(pc);
    thread_append_node<graph::Pending>(t->ctx,
                                       std::string(stmt->location().view()));
  }

  graph::GraphvizPrinter gv(output_path);
  gv.visit(entry_node.get());
}

bool GlobalContext::operator==(const GlobalContext &other) const {
  return false;
  // if (threads.size() != other.threads.size() || locks.size() !=
  // other.locks.size())
  //     return false;

  // // Threads may have been spawned in a different order, so we
  // // find the thread with the same block in the other context
  // for (auto &thread : threads)
  // {
  //     auto it = std::find_if(other.threads.begin(), other.threads.end(),
  //                             [&thread](auto &t)
  //                             { return t->block == thread->block; });
  //     if (it == other.threads.end() || !(*thread == **it))
  //         return false;
  // }

  // for (auto &[name, lock] : locks)
  // {
  //     if (!other.locks.contains(name))
  //         return false;
  //     auto &other_lock = other.locks.at(name);
  //     if (lock.owner != other_lock.owner)
  //         return false;
  // }
  // return true;
}

} // namespace gitmem