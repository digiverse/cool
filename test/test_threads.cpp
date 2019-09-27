/* Copyright (c) 2015 Digiverse d.o.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. The
 * license should be included in the source distribution of the Software;
 * if not, you may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * The above copyright notice and licensing terms shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <atomic>
#include <cmath>
#include <string>
#include <unistd.h>
#include <gtest/gtest.h>
#include "cool/gcd_task.h"
#include "cool/gcd_async.h"

const int NO_QUEUES = 200;
const int NO_TASKS = 100;
const int BLOCK_QUEUES = 10;
const int NUM_SQRTS_PER_SUM = 1000000;
const int BLOCK_SECONDS = 90;
int max_threads = 0;

using namespace cool::basis;
std::atomic<double> result;

std::mutex m;
std::condition_variable v;

std::atomic<int> tasks_count(NO_TASKS * (NO_QUEUES - BLOCK_QUEUES) + BLOCK_QUEUES);
int runners_count(NO_QUEUES);

int num_sqrts_per_sum = NUM_SQRTS_PER_SUM;

void blocking()
{
  std::this_thread::sleep_for(std::chrono::seconds(BLOCK_SECONDS));
  --tasks_count;
}

void long_running()
{
  double sum = 0;
  for (int i = 0; i < num_sqrts_per_sum; ++i)
    sum += std::sqrt(i);
  result = sum;
  --tasks_count;
}

void send_complete()
{
  std::unique_lock<std::mutex> l(m);
  --runners_count;
  v.notify_one();
}

std::shared_ptr<cool::gcd::task::runner> queues[NO_QUEUES];

void dispatch()
{
  for (int i = 0; i < NO_QUEUES; ++i)
    queues[i] = std::make_shared<cool::gcd::task::runner>();

   for (int i = 0; i < BLOCK_QUEUES; ++i)
    cool::gcd::task::factory::create(queues[i], blocking).run();

  for (int i = 0; i < NO_TASKS; ++i)
    for (int j = BLOCK_QUEUES; j < NO_QUEUES; ++j)
      cool::gcd::task::factory::create(queues[j], long_running).run();

  for (int i = 0; i < NO_QUEUES; ++i)
      cool::gcd::task::factory::create(queues[i], send_complete).run();

}

int get_num_threads()
{
#if defined(APPLE_TARGET) || defined(LINUX_TARGET)
#if defined(APPLE_TARGET)
  static std::string cmd = "ps -fM " + std::to_string(getpid()) + " | wc -l";
#elif defined(LINUX_TARGET)
  static std::string cmd = "ls -l /proc/" + std::to_string(getpid()) + "/task | wc -l";
#endif
  char rd[1000];

  {
    auto fp = popen(cmd.c_str(), "r");
    fgets(rd, sizeof(rd), fp);
    pclose(fp);
  }

  int no_threads;
  {
    std::istringstream ss(rd);
    ss >> no_threads;
  }
  return no_threads - 1;
#else
  return 999999;
#endif
}
#if 1
TEST(threads, max_threads)
{
  num_sqrts_per_sum = NUM_SQRTS_PER_SUM;
  runners_count = NO_QUEUES;
  tasks_count = NO_TASKS * (NO_QUEUES - BLOCK_QUEUES) + BLOCK_QUEUES;

  dispatch();
  while (true)
  {
    std::unique_lock<std::mutex> l(m);
    v.wait_for(l, std::chrono::seconds(BLOCK_SECONDS/6), []() { return runners_count == 0; });
    std::cout << "waiting on " << runners_count << " runners, remaining tasks " << tasks_count << "]" << std::endl;
    auto cnt = get_num_threads();
    std::cout << "Num threads: " << cnt << std::endl;
    if (cnt > max_threads)
      max_threads = cnt;
    EXPECT_LT(cnt, NO_QUEUES/4);
    if (runners_count == 0)
      break;
  }
  std::cout << "Max threads: " << max_threads << ", test limit: << " << NO_QUEUES/4 << std::endl;
}
#endif

TEST(threads, runner_race)
{
  runners_count = NO_QUEUES;
  num_sqrts_per_sum = 100;

  for (int i = 0; i < NO_QUEUES; ++i)
    queues[i] = std::make_shared<cool::gcd::task::runner>();

  for (int i = 0; i < NO_TASKS; ++i)
    for (int j = 0; j < NO_QUEUES; ++j)
      cool::gcd::task::factory::create(queues[j], long_running).run();

//  sleep(1);
  for (int i = 0; i < NO_QUEUES; ++i)
    queues[i].reset();
}

TEST(threads, timer)
{
  std::mutex mutex;
  std::condition_variable cv;
  bool done = false;
  cool::gcd::task::runner runner;
  cool::gcd::async::timer timer(
      [&mutex, &cv, &done](unsigned long count)
      {
        std::unique_lock<std::mutex> l(mutex);
        done = true;
        cv.notify_one();
      }
    , runner
    , std::chrono::milliseconds(100)
  );

  auto start = std::chrono::steady_clock::now();
  {
    std::unique_lock<std::mutex> l(mutex);

    timer.start();

    cv.wait_for(l, std::chrono::milliseconds(1000), [&done]() { return done; });
  }

  auto interval = std::chrono::steady_clock::now() - start;
  EXPECT_EQ(done, true);
  EXPECT_LT(std::chrono::milliseconds(90), interval);
  EXPECT_LT(interval, std::chrono::milliseconds(120));
}

TEST(threads, runner)
{
  std::mutex mutex;
  std::condition_variable cv;
  bool done = false;
  cool::gcd::task::runner runner;

  std::unique_lock<std::mutex> l(mutex);
  auto lambda = [&mutex, &cv, &done]()
      {
        std::unique_lock<std::mutex> l(mutex);
        done = true;
        cv.notify_one();
      };
  runner.run(lambda);

  cv.wait_for(l, std::chrono::milliseconds(1000), [&done]() { return done; });

  EXPECT_EQ(done, true);
}
