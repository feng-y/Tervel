#ifndef TERVEL_UTIL_MEMORY_RC_POOL_ELEMENT_H_
#define TERVEL_UTIL_MEMORY_RC_POOL_ELEMENT_H_

#include <atomic>
#include <utility>

#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#include "tervel/util/descriptor.h"
#include "tervel/util/system.h"
#include "tervel/util/util.h"
#include "tervel/util/info.h"

namespace tervel {
namespace util {
namespace memory {
namespace rc {

const int DEBUG_EXPECTED_STAMP = 0xDEADBEEF;
/** 
 * This class is used to hold the memory management information (Header) and
 * a descriptor object. It is important to sepearte them to prevent the case
 * where a thread attempts to dereference an object while its type id is being
 * changed.
 */
class PoolElement {
 public:
  /**
   * All the member variables of PoolElement are stored in a struct so that the
   * left over memory for cache padding can be easily calculated.
   */
  struct Header {
    PoolElement *next;
    std::atomic<uint64_t> ref_count {0};

#ifdef DEBUG_POOL
    std::atomic<bool> descriptor_in_use_ {false};

    std::atomic<uint64_t> allocation_count {1};
    std::atomic<uint64_t> free_count {0};

    // This stamp is checked when doing memory pool shenanigans to make sure
    // that a given descriptor actually belongs to a memory pool.
    int debug_pool_stamp_ {DEBUG_EXPECTED_STAMP};
#endif
  };

  explicit PoolElement(PoolElement *next=nullptr) {
    this->header().next = next;
  }

  // TODO(carlos) add const versions of these accessors

  /**
   * Returns a pointer to the associated descriptor of this element. This
   * pointer may or may not refrence a constructed object.
   */
  tervel::util::Descriptor * descriptor();

  Header & header() { return *reinterpret_cast<Header*>(padding_); }

  /**
   * Helper method for getting the next pointer.
   */
  PoolElement * next() { return header().next; }

  /**
   * Helper method for setting the next pointer.
   */
  void next(PoolElement *next) { header().next = next; }

  /**
   * Constructs a descriptor of the given type within this pool element. Caller
   * must be careful that there's not another descriptor already in use in this
   * element, or memory will be stomped and resources might leak.
   *
   * Call cleanup_descriptor() to call the descriptor's destructor when done
   * with it.
   */
  template<typename DescrType, typename... Args>
  void init_descriptor(Args&&... args);

  /**
   * Should be called by the owner of this element when the descriptor in this
   * element is no longer needed, and it is safe to destroy it. Simply calls the
   * destructor on the internal descriptor.
   */
  void cleanup_descriptor();

 private:
  /**
   * This object should have 2 members: a header and a descriptor, but the
   * memory layout of classes is system-dependent, so we explicitly construct
   * everything in-place inside this padding.
   *
   * This is possibly a horrible idea, and it might be safer (but slower) to use
   * back-pointers.
   */
  char padding_[CACHE_LINE_SIZE];

  DISALLOW_COPY_AND_ASSIGN(PoolElement);
};
static_assert(sizeof(PoolElement) == CACHE_LINE_SIZE,
    "Pool elements should be cache-aligned. Padding calculation is probably"
    " wrong.");



// IMPLEMENTATIONS
// ===============
inline tervel::util::Descriptor * PoolElement::descriptor() {
  // Extra padding is added to ensure that the descriptor pointer returned is
  // cache-aligned.
  //
  // TODO(carlos) This assumes that cache is aligned to 4 bytes. Should grab
  // the cache alignment size of the target system and use that instead.
  constexpr size_t header_pad = 4 - sizeof(Header) % 4;
  tervel::util::Descriptor *descr = reinterpret_cast<Descriptor*>(padding_ +
    sizeof(Header) + header_pad);

  // algorithms expect descriptors to be mem-aligned, so the LSB's should not be
  // taken up.
  assert((reinterpret_cast<uintptr_t>(descr) & 0x03) == 0);

  return descr;
}


template<typename DescrType, typename... Args>
void PoolElement::init_descriptor(Args&&... args) {
  static_assert(sizeof(DescrType) <= sizeof(padding_),
      "Descriptor is too large to use in a pool element");
  // TODO(Steven) Carlos make sure the 4byte alligment is factored in to the above
#ifdef DEBUG_POOL
  assert(!this->header().descriptor_in_use_.load());
  this->header().descriptor_in_use_.store(true);
#endif
  new(descriptor()) DescrType(std::forward<Args>(args)...);
}


inline void PoolElement::cleanup_descriptor() {
#ifdef DEBUG_POOL
  assert(this->header().descriptor_in_use_.load());
  this->header().descriptor_in_use_.store(false);
#endif
  this->descriptor()->~Descriptor();
}


/**
 * If the given descriptor was allocated through a DescriptorPool, then it has
 * an associated PoolElement header. This methods returns that PoolElement.
 *
 * Use with caution as Descriptors not allocated from a pool will not have an
 * associated header, and, thus, the returned value will be to some random
 * place in memory.
 */
inline PoolElement * get_elem_from_descriptor(tervel::util::Descriptor *descr) {
  PoolElement::Header *tmp = reinterpret_cast<PoolElement::Header *>(descr) - 1;
#ifdef DEBUG_POOL
  // If this fails, then the given descriptor is not part of a PoolElement. This
  // probably means the user passed in a descriptor that wasn't allocated
  // through a memory pool.
  assert(tmp->debug_pool_stamp_ == DEBUG_EXPECTED_STAMP);
#endif
  return reinterpret_cast<PoolElement *>(tmp);
}


}  // namespace rc
}  // namespace memory
}  // namespace util
}  // namespace tervel

#endif  // TERVEL_UTIL_MEMORY_RC_POOL_ELEMENT_H_
