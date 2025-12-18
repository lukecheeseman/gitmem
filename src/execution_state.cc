#include "execution_state.hh"
#include "sync_protocol.hh"

namespace gitmem {

  GlobalContext::GlobalContext(const trieste::Node &ast) {
    trieste::Node starting_block = ast / lang::File / lang::Block;
    entry_node = std::make_shared<graph::Start>(0);
    // ThreadContext starting_ctx = {{}, {}, entry_node};
    // auto main_thread = std::make_shared<Thread>(starting_ctx, starting_block);

    // this->threads = {main_thread};
    // this->locks = {};
    // this->cache = {};
  }


  GlobalContext::~GlobalContext() = default;
}