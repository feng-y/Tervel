#ifndef TERVEL_MEMORY_HP_POOL_MANAGER_H_
#define TERVEL_MEMORY_HP_POOL_MANAGER_H_

#include <atomic>
#include <memory>
#include <utility>

#include <assert.h>
#include <stdint.h>

#include "tervel/memory/system.h"
#include "tervel/util.h"

namespace tervel {
namespace memory {

class Descriptor;

namespace hp {

class DescriptorPool;
class HPElement;

/**
 * Encapsulates a shared central pool between several thread-local pools. Idea
 * is that each thread gets a local pool to store descriptors until it is safe to
 * free them. These pools are managed by a single instance of this class. 
 * The thread local pools can periodically send their unsafe elements into the 
 * shared pool in this manager.
 */
class PoolManager {
 public:
  friend class DescriptorPool;

  PoolManager();
      : pool_(new ManagedPool) {}



 private:
  struct ManagedPool {
    std::atomic<HPElement *> unsafe_pool {nullptr}

    char padding[CACHE_LINE_SIZE - sizeof(pool) - sizeof(unsafe_pool)];
  };
  static_assert(sizeof(ManagedPool) == CACHE_LINE_SIZE,
      "Managed pools have to be cache aligned to prevent false sharing.");

  std::unique_ptr<ManagedPool> pool_;

  DISALLOW_COPY_AND_ASSIGN(PoolManager);
};

}  // namespace hp
}  // namespace memory
}  // namespace tervel

#endif  // TERVEL_MEMORY_HP_POOL_MANAGER_H_