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
//#include <gtest/gtest.h>
#include "cool/gcd_task.h"

const int NO_QUEUES = 200;
const int NO_TASKS = 100;
const int BLOCK_QUEUES = 10;
const int NUM_SQRTS_PER_SUM = 1000000; //10000000;
const int BLOCK_SECONDS = 90;
int max_threads = 0;

using namespace cool::basis;
std::atomic<double> result;

std::mutex m;
std::condition_variable v;
int count(NO_QUEUES);

std::atomic<int> total(NO_TASKS * (NO_QUEUES - BLOCK_QUEUES) + BLOCK_QUEUES);

void blocking()
{
  std::this_thread::sleep_for(std::chrono::seconds(BLOCK_SECONDS));
  --total;
}

void long_running()
{
  double sum = 0;
  for (int i = 0; i < NUM_SQRTS_PER_SUM; ++i)
    sum += std::sqrt(i);
  result = sum;
  --total;
}

void send_complete()
{
  std::unique_lock<std::mutex> l(m);
  --count;
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


void test1()
{
#if HAS_QUEUE_WITH_TARGET != 1
  {
    auto runner = std::make_shared<cool::gcd::task::runner>();
    cool::gcd::task::factory::create(runner,
      []()
      {
        sleep(1);
      }
    ).run();

    // sleep(3); // comment to crash ;)
  }

  sleep(5);
#endif
}

void test2()
{
  dispatch();
  while (true)
  {
    std::unique_lock<std::mutex> l(m);
    v.wait_for(l, std::chrono::seconds(BLOCK_SECONDS/6), []() { return count == 0; });
    std::cout << "wait on " << count << " [" << total << "]" << std::endl;
    auto cnt = get_num_threads();
    std::cout << "Num threads: " << cnt << std::endl;
    if (cnt > max_threads)
      max_threads = cnt;
//    ASSERT_LT(cnt, NO_QUEUES/4);
    if (count == 0)
      break;
  }
  std::cout << "max threads: " << max_threads << std::endl;

}
//TEST(threads, run)
int main(int argc, char* argv[])
{
  test1();
  test2();
}
