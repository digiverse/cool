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
#include <iomanip>
#include <sstream>

#if defined(WIN32_TARGET)
#define WIN32_COOL_BUILD
#endif

#include "cool/gcd_task.h"
#include "cool/gcd_async.h"

namespace cool { namespace gcd { namespace task {

namespace entrails
{

// ---- --------------------------------------------------------------------
// ----
// ---- Task queue
// ----
// ---- ---------------------------------------------------------------------
namespace {

#if HAS_QUEUE_WITH_TARGET != 1
void delete_taskinfo(void *ti)
{
  delete static_cast<entrails::taskinfo*>(ti);
}
#endif
}  // anonymous namespace

dispatch_queue_t& queue::get_global()
{
  static dispatch_queue_t global_ = ::dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);

  return global_;
}

queue::queue(dispatch_queue_t q)
  : m_is_system(true)
  , m_enabled(true)
  , m_queue(q)
{ /* noop */ }

queue::queue(const std::string& name)
  : m_is_system(false)
  , m_enabled(true)
{
#if HAS_QUEUE_WITH_TARGET == 1
  m_queue = dispatch_queue_create_with_target(name.c_str(), NULL, get_global());
#else
  m_running = false;
#endif
}

queue::~queue()
{
#if HAS_QUEUE_WITH_TARGET != 1
  for (auto&& item : m_fifo)
    item.clear();
#endif
}

void queue::enqueue(entrails::taskinfo* task)
{
  if (is_suspended())
    return;
#if HAS_QUEUE_WITH_TARGET == 1
  ::dispatch_async_f(m_queue, task, entrails::task_executor);
#else
  {
    std::unique_lock<std::mutex> l(m_mutex);
    m_fifo.push_back(queue_context(task, entrails::task_executor, delete_taskinfo));
  }
  {
    bool expect = false;
    if (m_running.compare_exchange_strong(expect, true))
      submit_next();
  }
#endif
}

void queue::enqueue(void* task)
{
  if (is_suspended())
    return;
#if HAS_QUEUE_WITH_TARGET == 1
  ::dispatch_async_f(m_queue, task, entrails::executor);
#else
  {
    std::unique_lock<std::mutex> l(m_mutex);
    m_fifo.push_back(queue_context(task, entrails::executor, nullptr));
  }
  {
    bool expect = false;
    if (m_running.compare_exchange_strong(expect, true))
      submit_next();
  }
#endif
}

#if HAS_QUEUE_WITH_TARGET != 1
void queue::enqueue(void(*task)(void *), void* context)
{
  if (is_suspended())
    return;

  {
    std::unique_lock<std::mutex> l(m_mutex);
    m_fifo.push_back(queue_context(context, task, nullptr));
  }
  {
    bool expect = false;
    if (m_running.compare_exchange_strong(expect, true))
      submit_next();
  }
}
#endif


void queue::start()
{
  if (!is_system())
  {
    bool expected = false;
    if (m_enabled.compare_exchange_strong(expected, true))
      ::dispatch_resume(m_queue);
  }
}

void queue::stop()
{
  if (!is_system())
  {
    bool expected = true;
    if (m_enabled.compare_exchange_strong(expected, false))
      ::dispatch_suspend(m_queue);
  }
}

#if HAS_QUEUE_WITH_TARGET != 1
void queue::submit_next()
{
  ::dispatch_async_f(get_global(), this, run_next);
}

void queue::run_next(void *self_)
{
  auto me = static_cast<queue*>(self_);
  queue_context task;
  {
    std::unique_lock<std::mutex> l(me->m_mutex);
    if (me->m_fifo.empty())
    {
      me->m_running = false;
      return;
    }

    task = me->m_fifo.front();
    me->m_fifo.pop_front();
  }
  (*task.m_executor)(task.m_context);
  me->submit_next();
}

#endif
// ---- --------------------------------------------------------------------
// ----
// ---- Taskinfo
// ----
// ---- ---------------------------------------------------------------------

void cleanup_reverse(taskinfo* info_)
{
  for ( ; info_->m_prev != nullptr; info_ = info_->m_prev)
  ;
  cleanup(info_);
}

void cleanup(taskinfo* info_)
{
  entrails::taskinfo* aux;

  while (info_ != nullptr)
  {
    aux = info_->m_next;
    delete info_;
    info_ = aux;
  }
}

taskinfo* start(taskinfo* p)
{
  for ( ; p->m_prev != nullptr; p = p->m_prev)
  ;
  return p;
}


void kickstart(taskinfo* info_)
{
  if (info_ == nullptr)
    throw cool::exception::illegal_state("this task object is in undefined state");

  auto&& aux = info_->m_runner.lock();
  if (!aux)
    throw runner_not_available();

  aux->task_run(info_);
}

void kickstart(taskinfo* info_, const std::exception_ptr& e_)
{
  auto aux = info_->m_runner.lock();
  if (aux)
  {
    aux->task_run(new task_t(std::bind(info_->m_eh, e_)));
  }
}

taskinfo::taskinfo(const std::weak_ptr<runner>& r)
  : m_runner(r)
  , m_next(nullptr)
  , m_prev(nullptr)
  , m_is_on_exception(false)
{ /* noop */ }

taskinfo::taskinfo(entrails::task_t* t, const std::weak_ptr<runner>& r)
  : m_runner(r)
  , m_next(nullptr)
  , m_prev(nullptr)
  , m_is_on_exception(false)
{
  m_callable.task(t);
}

taskinfo::~taskinfo()
{
  if (m_callable)
  {
    if (m_deleter)
      m_deleter();
    else
      delete m_callable.task();
  }
}

// executor for task::run()
void task_executor(void* ctx)
{
  taskinfo* aux = static_cast<taskinfo*>(ctx);
  (*aux->m_callable.task())();
}

// executor for runner::run()
void executor(void* ctx)
{
  std::unique_ptr<task_t> task(static_cast<task_t*>(ctx));
  try { (*task)(); } catch (...) { }
}

} // namespace

#if defined(APPLE_TARGET)
runner::runner(dispatch_queue_attr_t queue_type)
    : named("si.digiverse.cool.runner")
    , m_active(true)
    , m_data(std::make_shared<entrails::queue>(name().c_str()))
{ /* noop */ }
#else
runner::runner()
    : named("si.digiverse.cool.runner")
    , m_active(true)
    , m_data(std::make_shared<entrails::queue>(name().c_str()))
{ /* noop */ }
#endif

runner::runner(const std::string& name, dispatch_queue_priority_t priority)
    : named(name)
    , m_active(true)
    , m_data(std::make_shared<entrails::queue>(name.c_str()))
{ /* noop */ }

runner::~runner()
{
  start();
}

void runner::stop()
{
  m_data->stop();
}

void runner::task_run(entrails::taskinfo* info_)
{
  m_data->enqueue(info_);
}

void runner::task_run(entrails::task_t* task)
{
// TODO  ::dispatch_async_f(m_data->m_queue, task, entrails::executor);
}

void runner::start()
{
  m_data->start();
}

// --------------- global variables ---------------------------
class system_runner : public runner
{
 public:
  system_runner(const std::string& name, dispatch_queue_priority_t priority)
      : runner(name, priority)
  { /* noop */ }
};

std::shared_ptr<runner> runner::sys_high_ptr()
{
  static const std::shared_ptr<system_runner> sys_runner_high(
      new system_runner("prio-high", DISPATCH_QUEUE_PRIORITY_HIGH));

  return sys_runner_high;
}
const runner& runner::sys_high()
{
  return *sys_high_ptr();
}

std::shared_ptr<runner> runner::sys_default_ptr()
{
  static const std::shared_ptr<system_runner> sys_runner_default(
      new system_runner("prio-def-", DISPATCH_QUEUE_PRIORITY_DEFAULT));

  return sys_runner_default;
}
const runner& runner::sys_default()
{
  return *sys_default_ptr();
}

const runner& runner::sys_low()
{
  static const std::shared_ptr<system_runner> sys_runner_low(
      new system_runner("prio-low-", DISPATCH_QUEUE_PRIORITY_LOW));
  return *sys_runner_low;
}

#if !defined(WIN32_TARGET)
std::shared_ptr<runner> runner::sys_background_ptr()
{
  static const std::shared_ptr<system_runner> sys_runner_background(
      new system_runner("prio-bg-", DISPATCH_QUEUE_PRIORITY_BACKGROUND));
  return sys_runner_background;
}
const runner& runner::sys_background()
{
  return *sys_background_ptr();
}
#endif

std::shared_ptr<runner> runner::cool_default_ptr()
{
  static const std::shared_ptr<runner> cool_serial_runner(new runner);

  return cool_serial_runner;
}
const runner& runner::cool_default()
{
  return *cool_default_ptr();
}

// ---- --------------------------------------------------------------------
// ----
// ---- Group
// ----
// ---- ---------------------------------------------------------------------
#if 0
group::group()
{
  auto g = ::dispatch_group_create();
  if (g == NULL)
    throw exception::create_failure("Failed to create task group");

  m_group = g;
}

group::group(const group& original)
{
  m_group = original.m_group;
  ::dispatch_retain(m_group);
}

group& group::operator=(const cool::gcd::task::group &original)
{
  m_group = original.m_group;
  ::dispatch_retain(m_group);
  return *this;
}

group::~group()
{
  ::dispatch_group_notify_f(m_group, runner::cool_default(), m_group, &group::finalizer);
}

void group::executor(void* ctx)
{
  std::unique_ptr<entrails::task_t> task(static_cast<entrails::task_t*>(ctx));
  try
  {
    (*task)();
  }
  catch (...)
  {
    // TODO: how to handle this?
  }
}

void group::finalizer(void *ctx)
{
  dispatch_group_t g = static_cast<dispatch_group_t>(ctx);
  ::dispatch_release(g);
}

void group::then(const handler_t& handler)
{
  void* ctx = new entrails::task_t(handler);
  ::dispatch_group_notify_f(m_group, runner::cool_default(), ctx, &group::executor);
}

void group::wait()
{
  ::dispatch_group_wait(m_group, ::dispatch_time(DISPATCH_TIME_FOREVER, 0));
}

void group::wait(int64_t interval)
{
  if (interval < 0)
    throw cool::exception::illegal_argument("Cannot use negative wait interval.");

  if (::dispatch_group_wait(m_group, ::dispatch_time(DISPATCH_TIME_NOW, interval)) == 0)
    return;

  throw cool::exception::timeout("Timeout while waiting for tasks to complete");
}
#endif

} } } // namespace
