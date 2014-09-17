//
//  main_tester.c
//  mCAS
//
//  Created by Steven Feldman on 11/1/12.

//  Copyright (c) 2012 Steven FELDMAN. All rights reserved.
//

// #define KILL_THREAD 1

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include <sys/time.h>
#include <time.h>
#include <vector>
#include <thread>

#include <iostream>
#include <atomic>

#include <boost/random.hpp>
#include <boost/generator_iterator.hpp>
#include <gflags/gflags.h>

#include "container_api.h"

DEFINE_int32(capacity, 64, "The initial capacity of the hash map");
DEFINE_int32(num_threads, 1, "The number of executing threads.");
DEFINE_int32(execution_time, 1, "The amount of time to run the tests");

class TestObject {
 public:
  TestObject(int num_threads, int capacity, int execution_time)
      : test_class_(num_threads, capacity)
      , num_threads_(num_threads)
      , execution_time_(execution_time) {}

  void print_test() {
    printf("Not Implemented...\n");
  };

  void print_results() {
#ifdef DEBUG
    printf("Debug Print Results Not Implemented...\n");
#else
    printf("Print Results Not Implemented...\n");
#endif
  }

  std::atomic<int> ready_count_{0};
  std::atomic<bool> running_{true};
  std::atomic<bool> wait_flag_{true};

  TestClass<int64_t, int64_t> test_class_;
  const int num_threads_;
  const int execution_time_;
};


void run(int thread_id, TestObject * test_object);

int main(int argc, char** argv) {
#ifdef USE_CDS
  cds::Initialize() ;
{
  cds::gc::HP hpGC;
  cds::gc::HP::thread_gc myThreadGC (true) ;
#endif

  gflags::ParseCommandLineFlags(&argc, &argv, true);

  TestObject test_data(FLAGS_num_threads+1, FLAGS_capacity,
      FLAGS_execution_time);

#ifdef DEBUG
  test_data.print_test();
#endif


  std::vector<std::thread> thread_list;
  for (int64_t i = 0; i < FLAGS_num_threads; i++) {
    std::thread temp_thread(run, i, &test_data);
    thread_list.push_back(std::move(temp_thread));
  }

  while (test_data.ready_count_.load() < FLAGS_num_threads) {}

#ifdef DEBUG
  printf("Beginning Test.\n");
#endif
  test_data.wait_flag_.store(false);
  std::this_thread::sleep_for(std::chrono::seconds(test_data.execution_time_));
  test_data.running_.store(false);
  std::this_thread::sleep_for(std::chrono::seconds(1));
#ifdef DEBUG
  printf("Signaled Stop!\n");
#endif

  std::for_each(thread_list.begin(), thread_list.end(), [](std::thread &t)
    { t.join(); });

  test_data.print_results();

#ifdef USE_CDS
  }
#endif

  return 0;
}

void run(int thread_id, TestObject * test_data) {

#ifdef USE_CDS
  cds::gc::HP::thread_gc myThreadGC (true) ;
#endif


  const int64_t num_threads = test_data->num_threads_;
  test_data->test_class_.attach_thread();

  test_data->ready_count_.fetch_add(1);

  bool res;

  int64_t i;
  for (i = thread_id; test_data->running_.load(); i += num_threads) {
    res = test_data->test_class_.insert(i, i);
    assert(res);
  }


  const int64_t max_value = i;

  for (i = thread_id; i < max_value; i += num_threads) {
    int64_t temp = -1;
    res = test_data->test_class_.find(i, temp);
    assert(res && temp == i);
  }

  for (i = thread_id; i < max_value; i += num_threads) {
    int64_t temp = i;
    res = test_data->test_class_.update(i, temp, i+2);
    assert(res && temp == i);
  }

  for (i = thread_id; i < max_value; i += num_threads) {
    int64_t temp = -1;
    res = test_data->test_class_.find(i, temp);
    assert(res && temp == (i+2));
  }

  for (i = thread_id; i < max_value; i += num_threads) {
    res = test_data->test_class_.remove(i);
    assert(res);
  }

  for (i = thread_id; i < max_value; i += num_threads) {
    int64_t temp = -1;
    res = test_data->test_class_.find(i, temp);
    assert(!res && temp == -1);
  }




  test_data->test_class_.detach_thread();
}

