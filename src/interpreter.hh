#pragma once

#include <trieste/trieste.h>
#include "lang.hh"
#include "graph.hh"
#include "graphviz.hh"
#include "execution_state.hh"

namespace gitmem {

  /* For debug printing */
    inline struct Verbose
    {
        bool enabled = false;

        template <typename T>
        const Verbose &operator<<(const T &msg) const
        {
            if (enabled)
            {
                std::cout << msg;
            }
            return *this;
        }

        const Verbose &operator<<(std::ostream &(*manip)(std::ostream &)) const
        {
            if (enabled)
            {
                std::cout << manip;
            }
            return *this;
        }
    } verbose;

    // Entry functions
    int interpret(const trieste::Node, const std::filesystem::path &output_file);

    // int interpret_interactive(const trieste::Node, const std::filesystem::path &output_file);
    // int model_check(const trieste::Node, const std::filesystem::path &output_file);

    // Internal functions
    int run_threads(GlobalContext &);

    std::variant<ProgressStatus, TerminationStatus>
    progress_thread(GlobalContext &, const ThreadID, std::shared_ptr<Thread>);

} // namespace gitmem