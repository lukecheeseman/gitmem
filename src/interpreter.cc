#include <regex>
#include <trieste/trieste.h>
#include <variant>

#include "debug.hh"
#include "graphviz.hh"
#include "interpreter.hh"
#include "sync_protocol.hh"

namespace gitmem {

using namespace trieste;

/* Interpreter for a gitmem program. Threads can read and write local
 * variables as well as versioned global variables. Globals are not stored
 * in a single memory location but instead in the state of 'synchronising
 * objects' which include threads and locks. Synchronising actions between
 * threads, and between threads and locks, synchronise the versioned memory
 * and if both objects see updates to the same versioned global variable
 * then a data race is detected. These synchronising actions include:
 * - thread t1 joining a thread t2, which waits for t2 to complete before
 *   trying to 'pull' the new data into t1
 * - t locking a lock l, which waits for the lock l to be available before
 *   trying to 'pull' the new data into t
 * - t unlocking a lock l, which updates l to have t's versioned memory
 */

bool is_syncing(Node stmt) {
  auto s = stmt / lang::Stmt;
  return s == lang::Join || s == lang::Lock || s == lang::Unlock;
}

bool is_syncing(Thread &thread) {
  return !thread.terminated && is_syncing(thread.block->at(thread.pc));
}

template <typename T, typename... Args>
std::shared_ptr<T> thread_append_node(ThreadContext &ctx, Args &&...args) {
  assert(ctx.tail);
  auto node = std::make_shared<T>(std::forward<Args>(args)...);
  ctx.tail->next = node;
  ctx.tail = node;
  return node;
}

template <>
std::shared_ptr<graph::Pending>
thread_append_node<graph::Pending>(ThreadContext &ctx, std::string &&stmt) {
  // pending nodes don't update the tail position as we will destroy them
  // once we execute the node
  auto s = std::regex_replace(stmt, std::regex("\n"), "\\l   ");
  auto node = make_shared<graph::Pending>(std::move(s));
  ctx.tail->next = node;
  return node;
}

/* Evaluating an expression either returns the result of the expression or
 * a the exceptional termination status of the thread.
 */
std::variant<size_t, TerminationStatus>
evaluate_expression(Node expr, GlobalContext &gctx, ThreadContext &ctx) {
  auto e = expr / lang::Expr;
  if (e == lang::Reg) {
    // It is invalid to read a previously unwritten value
    auto var = std::string(expr->location().view());
    if (ctx.locals.contains(var)) {
      return ctx.locals[var];
    } else {
      return TerminationStatus::unassigned_variable_read_exception;
    }
  } else if (e == lang::Var) {
    auto var = std::string(expr->location().view());
    if (std::optional<size_t> result = gctx.protocol->read(ctx, var)) {
      return *result;
    } else { // It is invalid to read a previously unwritten value
      return TerminationStatus::unassigned_variable_read_exception;
    }
  } else if (e == lang::Const) {
    return size_t(std::stoi(std::string(e->location().view())));
  } else if (e == lang::Add) {
    size_t sum = 0;
    for (auto &child : *e) {
      auto result = evaluate_expression(child, gctx, ctx);
      if (std::holds_alternative<TerminationStatus>(result))
        return result;
      sum += std::get<size_t>(result);
    }
    return sum;
  } else if (e == lang::Spawn) {
    ThreadID tid = gctx.threads.size();
    auto node = std::make_shared<graph::Start>(tid);
    ThreadContext child_ctx(node);

    if (std::optional<std::unique_ptr<ConflictBase>> conflict =
            gctx.protocol->on_spawn(ctx, child_ctx, gctx)) {
      assert(false); // handle this
    }

    // making the on_spawn and on_start events happen at thread spawn
    if (std::optional<std::unique_ptr<ConflictBase>> conflict =
            gctx.protocol->on_start(child_ctx, gctx)) {
      std::unreachable();
    }

    gctx.threads.push_back(
      std::make_shared<Thread>(std::move(child_ctx), e / lang::Block));
    thread_append_node<graph::Spawn>(ctx, tid, node);

    return tid;
  } else if (e == lang::Eq || e == lang::Neq) {
    auto lhs = e / lang::Lhs;
    auto rhs = e / lang::Rhs;

    auto lhsEval = evaluate_expression(lhs, gctx, ctx);
    if (std::holds_alternative<TerminationStatus>(lhsEval))
      return lhsEval;

    auto rhsEval = evaluate_expression(rhs, gctx, ctx);
    if (std::holds_alternative<TerminationStatus>(rhsEval))
      return rhsEval;

    return e == lang::Eq
               ? (std::get<size_t>(lhsEval)) == (std::get<size_t>(rhsEval))
               : (std::get<size_t>(lhsEval)) != (std::get<size_t>(rhsEval));
  } else {
    throw std::runtime_error("Unknown expression: " +
                             std::string(expr->type().str()));
  }
}

/* Evaluating a statement either returns the resulting change of the program
 * counter (0 if waiting for some other thread) or the exceptional
 * termination status of the thread.
 */
std::variant<int, TerminationStatus> run_statement(Node stmt,
                                                   GlobalContext &gctx,
                                                   ThreadContext &ctx,
                                                   const ThreadID &tid) {
  auto s = stmt / lang::Stmt;
  if (s == lang::Nop) {

    verbose << "Nop" << std::endl;

  } else if (s == lang::Jump) {

    auto cnst = s / lang::Const;
    auto delta = std::stoi(std::string(cnst->location().view()));
    assert(delta > 0);
    return delta;

  } else if (s == lang::Cond) {

    auto expr = s / lang::Expr;
    auto cnst = s / lang::Const;
    auto result = evaluate_expression(expr, gctx, ctx);

    if (auto b = std::get_if<size_t>(&result)) {
      auto delta = std::stoi(std::string(cnst->location().view()));
      assert(delta > 0);
      return *b ? 1 : delta;
    } else {
      return std::get<TerminationStatus>(result);
    }

  } else if (s == lang::Assign) {

    auto lhs = s / lang::LVal;
    auto var = std::string(lhs->location().view());
    auto rhs = s / lang::Expr;
    auto val_or_term = evaluate_expression(rhs, gctx, ctx);

    if (size_t *val = std::get_if<size_t>(&val_or_term)) {
      if (lhs == lang::Reg) {

        // Local variables can be re-assigned whenever
        verbose << "Set register '" << lhs->location().view() << "' to " << *val
                << std::endl;
        ctx.locals[var] = *val;

      } else if (lhs == lang::Var) {

        gctx.protocol->write(ctx, var, *val);

        // // Global variable writes need to create a new commit id
        // // to track the history of updates
        // auto &global = ctx.globals[var];
        // global.val = *val;
        // global.commit = gctx.uuid++;
        // verbose <<  "Set global '" << lhs->location().view() << "' to " <<
        // *val <<  " with id " << *(global.commit) << std::endl;

        // auto node = thread_append_node<graph::Write>(ctx, var, global.val,
        // *global.commit); gctx.commit_map[*(global.commit)] = node;
      } else {
        throw std::runtime_error("Bad left-hand side: " +
                                 std::string(lhs->type().str()));
      }
    } else {
      return std::get<TerminationStatus>(val_or_term);
    }
  } else if (s == lang::Join) {
    // A join must waiting for the terminating thread to continue,
    // we don't want to re-evaluate the expression repeatedly as this
    // may be effecting so store the result in the cache.
    auto expr = s / lang::Expr;

    if (!gctx.cache.contains(expr)) {
      auto val_or_term = evaluate_expression(expr, gctx, ctx);
      if (size_t *val = std::get_if<size_t>(&val_or_term)) {
        gctx.cache[expr] = *val;
      } else {
        return std::get<TerminationStatus>(val_or_term);
      }
    }

    auto result = gctx.cache[expr];
    auto &joinee = gctx.threads[result];
    if (joinee->terminated &&
        (*joinee->terminated == TerminationStatus::completed)) {
      if (auto conflict = gctx.protocol->on_join(ctx, joinee->ctx, gctx)) {
        verbose << (**conflict) << std::endl;
        return TerminationStatus::datarace_exception;
      } else {
        thread_append_node<graph::Join>(ctx, result, joinee->ctx.tail);
      }

    } else {
      verbose << "Waiting on thread " << result << std::endl;
      return 0;
    }
  } else if (s == lang::Lock) {
    // We can only lock unlocked locks, if a lock hasn't been used
    // before it is implicitly created, we then commit the pending
    // updates of this thread and pull the updates from the lock.
    auto v = s / lang::Var;
    auto var = std::string(v->location().view());

    Lock &lock = gctx.locks[var];
    if (lock.owner) {
      verbose << "Waiting for lock " << var << " owned by "
              << lock.owner.value() << std::endl;
      return 0;
    }

    lock.owner = tid;
    if (auto conflict = gctx.protocol->on_lock(ctx, lock, gctx)) {
      verbose << (**conflict) << std::endl;
      //     using graph::Node;
      //     auto [s1, s2] = conflict->commits;
      //     auto sources = std::pair<std::shared_ptr<Node>,
      //     std::shared_ptr<Node>>{gctx.commit_map[s1], gctx.commit_map[s2]};
      //     auto graph_conflict = graph::Conflict(conflict->var, sources);
      //     thread_append_node<graph::Lock>(ctx, var, lock.last,
      //     graph_conflict);
      return TerminationStatus::datarace_exception;
    }

    thread_append_node<graph::Lock>(ctx, var, lock.last);

    verbose << "Locked " << var << std::endl;
  } else if (s == lang::Unlock) {
    // We can only unlock locks we previously locked. We commit any
    // pending updates and then copy the threads versioned globals
    // to the locks versioned globals (nobody could have changed
    // them since we locked the lock).

    // commit(ctx.globals);
    auto v = s / lang::Var;
    auto var = std::string(v->location().view());

    auto &lock = gctx.locks[var];
    if (!lock.owner || (lock.owner && *lock.owner != tid)) {
      return TerminationStatus::unlock_exception;
    }

    if (auto conflict = gctx.protocol->on_unlock(ctx, lock, gctx)) {
      verbose << (**conflict) << std::endl;
      return TerminationStatus::datarace_exception;
    }

    // lock.globals = ctx.globals;
    lock.owner.reset();

    thread_append_node<graph::Unlock>(ctx, var);
    lock.last = ctx.tail;

    verbose << "Unlocked " << var << std::endl;

  } else if (s == lang::Assert) {

    auto expr = s / lang::Expr;
    auto result_or_term = evaluate_expression(expr, gctx, ctx);
    if (size_t *result = std::get_if<size_t>(&result_or_term)) {
      if (*result) {
        verbose << "Assertion passed: " << expr->location().view() << std::endl;
      } else {
        verbose << "Assertion failed: " << expr->location().view() << std::endl;
        thread_append_node<graph::AssertionFailure>(
            ctx, std::string(expr->location().view()));
        return TerminationStatus::assertion_failure_exception;
      }
    } else {
      return std::get<TerminationStatus>(result_or_term);
    }

  } else {
    throw std::runtime_error("Unknown statement: " +
                             std::string(stmt->type().str()));
  }
  return 1;
}

/* Run a particular thread until it reaches a synchronisation point or until
 * it terminates. Report whether the thread was able to progress or not, or
 * whether it terminated.
 */
std::variant<ProgressStatus, TerminationStatus>
run_single_thread_to_sync(GlobalContext &gctx, const ThreadID tid,
                          std::shared_ptr<Thread> thread) {
  if (thread->terminated) {
    return *(thread->terminated);
  }
  Node block = thread->block;
  size_t &pc = thread->pc;
  ThreadContext &ctx = thread->ctx;

  // TODO: one possible interpretation of on_start is to sync when the thread
  // starts execution statements
  // if (pc == 0) {
  //   gctx.protocol->on_start(thread->ctx, gctx);
  // }

  bool first_statement = true;
  while (pc < block->size()) {
    Node stmt = block->at(pc);

    if (!first_statement && is_syncing(stmt)) {
      return ProgressStatus::progress;
    }

    auto delta_or_term = run_statement(stmt, gctx, ctx, tid);
    if (auto term = std::get_if<TerminationStatus>(&delta_or_term)) {
      thread->terminated = *term;
      // thread_append_node<graph::End>(ctx);
      return *term;
    }

    auto delta = std::get<int>(delta_or_term);

    if (delta == 0) {
      return first_statement ? ProgressStatus::no_progress
                             : ProgressStatus::progress;
    }

    pc += delta;
    first_statement = false;
  }

  thread->terminated = TerminationStatus::completed;
  gctx.protocol->on_end(thread->ctx, gctx);

  thread_append_node<graph::End>(ctx);
  return TerminationStatus::completed;
}

/**
 * Run a thread to the next sync point, including any threads spawned by that
 * thread
 */
std::variant<ProgressStatus, TerminationStatus>
progress_thread(GlobalContext &gctx, const ThreadID tid,
                std::shared_ptr<Thread> thread) {
  auto no_threads = gctx.threads.size();
  auto prog_or_term = run_single_thread_to_sync(gctx, tid, thread);

  bool any_progress =
      std::holds_alternative<ProgressStatus>(prog_or_term) &&
      std::get<ProgressStatus>(prog_or_term) == ProgressStatus::progress;

  for (size_t i = no_threads; i < gctx.threads.size(); ++i) {
    // If there are new threads, we can run them to sync as well
    any_progress = true;
    auto new_thread = gctx.threads[i];
    if (!is_syncing(*new_thread)) {
      verbose << "==== Thread " << i << " (spawn) ====" << std::endl;
      progress_thread(gctx, i, new_thread);
    }
  }

  if (std::holds_alternative<TerminationStatus>(prog_or_term))
    return prog_or_term;

  return any_progress ? ProgressStatus::progress : ProgressStatus::no_progress;
}

/* Try to evaluate all threads until a sync point or termination point
 */
std::variant<ProgressStatus, TerminationStatus>
run_threads_to_sync(GlobalContext &gctx) {
  verbose << "-----------------------" << std::endl;
  bool all_completed = true;
  ProgressStatus any_progress = ProgressStatus::no_progress;
  for (size_t i = 0; i < gctx.threads.size(); ++i) {
    verbose << "==== t" << i << " ====" << std::endl;
    auto thread = gctx.threads[i];
    if (!thread->terminated) {
      auto prog_or_term = run_single_thread_to_sync(gctx, i, thread);
      if (ProgressStatus *prog = std::get_if<ProgressStatus>(&prog_or_term)) {
        any_progress |= *prog;
      } else {
        // We could return termination status of any error here and stop
        // at the first error
        thread->terminated = std::get<TerminationStatus>(prog_or_term);
        any_progress |= ProgressStatus::progress;
      }

      all_completed &= thread->terminated.has_value();
      // if a thread spawns a new thread, it will end up at the end so
      // we will always include the new threads in the termination
      // criteria
    }
  }

  if (all_completed)
    return TerminationStatus::completed;

  return any_progress;
}

bool is_finished(
    std::variant<ProgressStatus, TerminationStatus> &prog_or_term) {
  // Either, the system is stuck and made no progress in which case there
  // is a deadlock (or a thread is stuck waiting for a crashed thread?)
  if (ProgressStatus *prog = std::get_if<ProgressStatus>(&prog_or_term))
    return (*prog) == ProgressStatus::no_progress;

  // Or, there was some termination criteria in which case we stop
  return true;
}

/* Try to evaluate all threads until they have all terminated in some way
 * or we have reached a stuck configuration.
 */
int run_threads(GlobalContext &gctx) {
  std::variant<ProgressStatus, TerminationStatus> prog_or_term;
  do {
    prog_or_term = run_threads_to_sync(gctx);
  } while (!is_finished(prog_or_term));

  verbose << "----------- execution complete -----------" << std::endl;

  bool exception_detected = false;
  for (size_t i = 0; i < gctx.threads.size(); ++i) {
    const auto &thread = gctx.threads[i];
    if (thread->terminated) {
      switch (thread->terminated.value()) {
      case TerminationStatus::completed:
        verbose << "Thread " << i << " terminated normally" << std::endl;
        break;

      case TerminationStatus::unlock_exception:
        verbose << "Thread " << i << " unlocked a lock it does not own"
                << std::endl;
        exception_detected = true;
        break;

      case TerminationStatus::datarace_exception:
        verbose << "Thread " << i << " encountered a data-race" << std::endl;
        exception_detected = true;
        break;

      case TerminationStatus::assertion_failure_exception:
        verbose << "Thread " << i << " failed an assertion" << std::endl;
        exception_detected = true;
        break;

      case TerminationStatus::unassigned_variable_read_exception:
        verbose << "Thread " << i << " read an uninitialised value"
                << std::endl;
        exception_detected = true;
        break;

      default:
        verbose << "Thread " << i << " has an unhandled termination state"
                << std::endl;
        break;
      }
    } else {
      exception_detected = true;
      thread_append_node<graph::End>(thread->ctx);
      verbose << "Thread " << i << " is stuck" << std::endl;
    }
  }

  return exception_detected ? 1 : 0;
}

int interpret(const Node ast, const std::filesystem::path &output_path, SyncKind sync_kind) {
  GlobalContext gctx(ast, make_protocol(sync_kind));
  auto result = run_threads(gctx);
  // gctx.print_execution_graph(output_path); FIXME

  return result;
}

} // namespace gitmem