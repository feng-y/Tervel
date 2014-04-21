#include "thread/hp/descriptor_pool.h"

namespace ucf {
namespace thread {
namespace hp {

void HPPool::free_descriptor(HPElement *descr, bool dont_check) {
  if (!dont_check && is_watched(descr)) {
    this->add_to_unsafe(descr);
  } else {
    delete descr;
  }
}


void HPPool::send_to_manager() {
  this->try_clear_unsafe_pool(false);
  clear_pool(&unsafe_pool_, &this->manager_unsafe_pool());
}


namespace {

/**
 * Generic logic for clearing a pool and sending its elements to another,
 * shared pool. Parameters are passed by pointer because this function changes
 * their values.
 */
void clear_pool(HPElement **local_pool,
    std::atomic<HPElement *> *manager_pool) {
  if (local_pool != nullptr) {
    HPElement *p1 = *local_pool;
    HPElement *p2 = p1->next();
    while (p2 != nullptr) {
      p1 = p2;
      p2 = p1->next();
    }

    // p1->pool_next's value is updated to the current value
    // after a failed cas. (pass by reference fun)
    p1->next(manager_pool->load());
    while (!manager_pool->
        compare_exchange_strong(p1->header().next, *local_pool)) {}
  }
  *local_pool = nullptr;
}

}  // namespace
// REVIEW(carlos) excess whitespace





void HPPool::add_to_unsafe(HPElement* descr) {
  descr->next(unsafe_pool_);
  unsafe_pool_ = p;

#ifdef DEBUG_POOL
    unsafe_pool_count_++;
#endif
}


void HPPool::try_clear_unsafe_pool(bool dont_check) {
  while (unsafe_pool_) {
    HPElement *temp = unsafe_pool_->next();

    // REVIEW(carlos) 2 large blocks of code here, should add a short comment
    // for each
    bool watched = is_watched(temp);
    if (!dont_check && watched) {
      break;
    } else {
#ifdef DEBUG_POOL
      unsafe_pool_count_--;
#endif
      // REVIEW(carlos) That's not how the destructor is called. It's just
      //   another function:
      //     temp->~HPElement()
      // REVIEW(carlos) It'd be nice if the public interface to HPElement was
      //   the same as rc::PoolElement, so that this would be
      //   temp->cleanup_descriptor()
      ~temp();
      unsafe_pool_ = temp;
    }
  }

  if (unsafe_pool_ != nullptr) {
    HPElement *prev = unsafe_pool_;
    HPElement *temp = unsafe_pool_->next();

    while (temp) {
      PoolElement *temp_next = temp->next();

      bool watched = is_watched(temp);
      if (!dont_check && watched) {
        prev = temp;
        temp = temp_next;
      } else {
        // REVIEW(carlos) see above comments
        ~temp();
        prev->next(temp_next);
        temp = temp_next;
#ifdef DEBUG_POOL
        unsafe_pool_count_--;
#endif
      }
    }
  }
}

}  // namespace hp
}  // namespace thread
}  // namespace ucf
