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

#if !defined(GCD_ASYNC_H_HEADER_GUARD)
#define GCD_ASYNC_H_HEADER_GUARD

#include <functional>
#include <exception>
#include <atomic>
#include <dispatch/dispatch.h>
#include <csignal>

#include "entrails/platform.h"
#include "miscellaneous.h"
#include "gcd_task.h"
#include "entrails/gcd_async.h"

namespace cool { namespace gcd {

/**
 * Namespace containing abstractions of GCD's event sources.
 *
 * These abstractions provide facilities for asynchronous programming.
 */
namespace async {

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// ----
// ---- File I/O
// ----
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
/**
 * Asynchronous reader.
 *
 * An asynchronous reader monitors the associated file descriptor and invokes
 * the user supplied callback each time new data arrives and is ready to be read.
 * The callback is executed asynchronously on the task queue of the
 * specified task::runner.
 *
 * @note Upon object creation the reader is in the stopped state and must
 *   be explicitly started.
 *
 * <b>Thread Safety</b><br>
 *
 * Instances of cool::gcd::async::reader class are not thread safe.
 *
 * <b>Platform availability</b><br>
 *
 * This class is available on Mac OS/X and Linux.
 */
#if defined(APPLE_TARGET) || defined(LINUX_TARGET)
class reader : public entrails::fd_io
{
 public:
 /**
  * The user callback type.
  *
  * The user callback must be a Callable object that can be assigned to
  * @c std::function<void(int, std::size_t)> functional type. When called, the
  * handler receives the following parameters:
  *
  * @param fd file descriptor associated with the reader
  * @param count   approximate number of bytes available. Note that the actual
  *   number of bytes may differ when actually read.
  */
  typedef entrails::fd_io::handler_t handler_t;

 public:
  /**
   * Constructs a new instance of the asynchronous reader.
   *
   * @param fd    file descriptor to monitor
   * @param run   task::runner instance to use for callbacks
   * @param cb    user supplied callback to call when data is ready to be read
   * @param owner if true will close the file descriptor upon destruction
   *
   * @note Asynchronous reader takes the ownership of the supplied file descriptor. The
   *   file descriptor will be closed by the reader instance upon its destruction.
   *   Closing the file descriptor outside the reader may lead to undefined
   *   behavior.
   * @exception cool::exception::create_failure thrown if the creation of the reader
   *   instance failed.
   */
  reader(int fd, const task::runner& run, const handler_t& cb, bool owner = true)
      : entrails::fd_io(DISPATCH_SOURCE_TYPE_READ, fd, cb, run, owner)
  {
    if (fd < 0)
      throw exception::out_of_range("reader::fd");
  }

  /**
   * Start the asynchronous reader.
   *
   * The asynchronous reader will start calling the user supplied callback when
   * new data is ready for reading.
   *
   * @note Immediately after creation the asynchronous reader is in the stopped
   *   state. The user must explicitly call start() in order to start the
   *   operations.
   */
  void start() { resume(); }
  /**
   * Stop the asynchronous reader.
   *
   * The asynchronous reader will stop calling the user supplied callback when
   * new data is ready for reading.
   *
   * @note Use start() to resume the operations.
   */
  void stop()  { suspend(); }
};
#endif
/**
 * Asynchronous writer.
 *
 * The asynchronous writer accepts requests to write a sequence of bytes
 * to the associated file descriptor, writes them asynchronously to the
 * requester, and informs the requester upon the completion of the write
 * request, or upon error.
 *
 * <b>Thread Safety</b><br>
 *
 * Instances of cool::gcd::async::writer class are not thread safe.
 *
 * <b>Platform availability</b><br>
 *
 * This class is available on Mac OS/X and Linux.
 */
#if defined(APPLE_TARGET) || defined(LINUX_TARGET)
class writer
{
 public:
  /**
   * The user callback type.
   *
   * The callback type of the user function object to be called upon
   * the write completion. The handler must be assignable to <tt>std::function</tt>
   * object with the following signature:
   * @code
   *   std::function<void(const void* data, std::size_t count)>
   * @endcode
   * When called, the handler receives the following parameters:
   * @param data pointer to the data buffer specified to the read() method
   * @param count number of bytes written
   */
  using handler_t = entrails::write_complete_handler;
  /**
   * The user callback type.
   *
   * The callback type of the user function object to be called upon
   * error during write. The argument is the <i>errno</i>. Note that
   * upon error the current write request is cancelled.
   */
  using err_handler_t = entrails::error_handler;

 public:
  /**
   * Constructs a new instance of the asynchronous writer.
   *
   * @param fd  file descriptor to write data to
   * @param run task::runner instance to use for callbacks
   * @param cb  user supplied callback to call when writer is complete
   * @param ecb user supplied callback to call if error occurs during write
   * @param owner if true will close the file descriptor upon destruction
   *
   * @note Asynchronous writer takes the ownership of the supplied file descriptor. The
   *   file descriptor will be closed by the reader instance upon its destruction.
   *   Closing the file descriptor outside the writer may lead to undefined
   *   behavior.
   * @exception cool::exception::create_failure thrown if the creation of the writer
   *   instance failed.
   */
  writer(int fd,
         const task::runner& run,
         const handler_t& cb = handler_t(),
         const err_handler_t& ecb = err_handler_t(),
         bool owner = true);

  /**
   * Request write.
   *
   * Requests @c size data from the data buffer pointed to by @ data to be
   * written to the associated file descriptor.
   *
   * @param data pointer to data to be written
   * @param size number of bytes to be written
   *
   * @exception exception::illegal_state thrown if the previous write request
   *   is not yet completed.
   *
   * @note The writer can handle only one request at once. Requesting another
   *   write while the previous is still being processed will throw an
   *   exception.
   */
  void write(const void*  data, std::size_t size);

  /**
   * Return true if write operation in progress.
   *
   * This predicate returns true if write operation is in progress and
   * another write request would throw.
   */
  bool is_busy() const;

 private:
  entrails::writer::ptr m_impl;

};
#endif
/**
 * Asynchronous reader and writer.
 *
 * This class is a composition of cool::gcd::async::reader and
 * cool::gcd::async::writer classes.
 *
 * @note Upon creation the reader part of the asynchronous reader/writer is
 *   in the stopped state and must be explicitly started. The writer part,
 *   however, is immediately ready to accept write requests.
 *
 * <b>Thread Safety</b><br>
 * Instances of cool::gcd::async::writer class are not thread safe.
 *
 * <b>Platform availability</b><br>
 *
 * This class is available on Mac OS/X and Linux.
 *
 * @see @ref cool::gcd::async::reader "asynchronous reader"
 * @see @ref cool::gcd::async::writer "asynchronous writer"
 */
#if defined(APPLE_TARGET) || defined(LINUX_TARGET)
class reader_writer
{
 public:
  /**
   * Constructs a new instance of the asynchronous reader/writer.
   *
   * @param fd     file descriptor to associate with
   * @param run    task::runner instance to use for callbacks
   * @param rd_cb  user supplied callback to call when data is ready to be read
   * @param wr_cb  user supplied callback to call when write request is complete
   * @param err_cb user supplied callback to call if error occurs during write
   * @param owner if true will close the file descriptor upon destruction
   *
   * @note Asynchronous reader/writer takes the ownership of the supplied file descriptor. The
   *   file descriptor will be closed by the reader instance upon its destruction.
   *   Closing the file descriptor outside the reader/writer may lead to undefined
   *   behavior.
   * @exception cool::exception::create_failure thrown if the creation of the writer
   *   instance failed.
   */
  reader_writer(int fd,
                const task::runner& run,
                const reader::handler_t& rd_cb,
                const writer::handler_t& wr_cb = writer::handler_t(),
                const writer::err_handler_t& err_cb = writer::err_handler_t(),
                bool owner = true);

  /**
   * Request write.
   *
   * Requests @c size data from the data buffer pointed to by @ data to be
   * written to the associated file descriptor.
   *
   * @param data pointer to data to be written
   * @param size number of bytes to be written
   *
   * @exception exception::illegal_state thrown if the previous write request
   *   is not yet completed.
   *
   * @note The reader/writer can handle only one write request at once. Requesting another
   *   write while the previous is still being processed will throw an
   *   exception.
   */
  void write(void*  data, std::size_t size)
  {
    m_wr.write(data, size);
  }
  /**
   * Start the reader part of asynchronous reader/writer.
   *
   * The reader will start calling the user supplied callback when
   * new data is ready for reading.
   *
   * @note Immediately after creation the reader is in the stopped
   *   state. The user must explicitly call start() in order to start the
   *   operations.
   */
  void start() { m_rd.start(); }
  /**
   * Stop the reader part of asynchronous reader/writer.
   *
   * The reader will stop calling the user supplied callback when
   * new data is ready for reading.
   *
   * @note Use start() to resume the operations.
   */
  void stop()  { m_rd.stop(); }
  /**
   * Return true if write operation in progress.
   *
   * This predicate returns true if write operation is in progress and
   * another write request would throw.
   */
  bool is_write_busy() const { return m_wr.is_busy(); }
 private:
  reader m_rd;
  writer m_wr;
};
#endif
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// ----
// ---- Software signals
// ----
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
/**
 * Action at the software signal.
 *
 * The signal objects enable the user code to specify the action to be
 * called when the program receives a software signal. This class is a
 * C++ abstraction of the old style signal(2) interface.
 *
 * @note Upon creation the signal handler is active.
 * @note Multiple signal objects can be specified for the same software signal
 *   number. All objects will be notified when the signal arrives.
 * @note Signal objects created via copy construction or copy assignment are
 *   clones of the original object. Only one of the clones will be notified
 *   upon arrival of the signal.
 *
 * @warning When the signal object is created it will set the signal(2) handler
 *   to SIG_IGN. This setting remains effective event after the last signal
 *   object for this signal number is destroyed.
 *
 * @warning Manipulating the software signal via signal(2) or sigaction(2)
 *   interfaces after the signal object for this software signal is created
 *   results in undefined behavior.
 *
 * <b>Thread Safety</b><br>
 * Instances of cool::gcd::async::signal class are not thread safe.
 *
 * <b>Platform availability</b><br>
 *
 * This class is available on Mac OS/X and Linux.
 *
 */
#if defined(APPLE_TARGET) || defined(LINUX_TARGET)
class signal : public entrails::async_source<std::function<void(int, int)>, int>
{
 public:
  /**
   * The user signal handler type.
   *
   * The user signal handler must be Callable that can be assigned to this
   * function type. When called, the handler receives the following parameters:
   *
   * @param signo The signal number - this allows use of the same handler for
   *   several different signals.
   * @param count The number of received signals between two successive calls
   *   to the handler for the same signal. Note that count is incremented even
   *   when the signal delivery is stopped.
   */
  typedef std::function<void(int signo, int count)> handler_t;

 private:
  typedef entrails::source_data<handler_t, int> context_t;

 public:
  /**
   * Create a signal object for the specified software signal number.
   *
   * @param signo The software signal number
   * @param handler Handler to be called when the software signal is detected.
   * @exception cool::exception::create_failure Thrown if signal object cannot be created.
   * @exception cool::exception::illegal_argument Thrown if signal number is out of range
   *   or if it equals SIGKILL or SIGSTOP, which cannot be intercepted.
   *
   * Upon arrival of the specified software signal the callback is called from the
   * context of @ref cool::gcd::task::runner::sys_default() "the default system runner".
   */
  signal(int signo, const handler_t& handler);
  /**
   * Create a signal object for the specified software signal number.
   *
   * @param signo The software signal number
   * @param handler Handler to be called when the software signal is detected.
   * @param runner  @ref cool::gcd::task::runner "runner" to use to execute the handler.
   *
   * @exception cool::exception::create_failure Thrown if signal object cannot be created
   * @exception cool::exception::illegal_argument Thrown if signal number is out of range
   *   or if it equals SIGKILL or SIGSTOP, which cannot be intercepted.
   */
  signal(int signo, const handler_t& handler, const task::runner& runner);
  /**
   * Start delivering the signal.
   *
   * This call will restart the signal delivery after it was stopped by a call
   * to stop().
   */
  void start() { resume(); }
  /**
   * Stop delivering the signal.
   *
   * After this call the signal handler will no longer be called although the
   * signal will still be registered internally and the signal counter will be
   * incremented. A call to stop() will effectively ignore the software signal.
   * Use start() to restart the signal delivery.
   */
  void stop()  { suspend(); }

 private:
  static void event_handler(void* ctx);
#if HAS_QUEUE_WITH_TARGET != 1
  static void secondary_event_handler(void* ctx);
#endif
};
#endif
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// ----
// ---- Timers
// ----
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
/**
 * Timer objects.
 *
 * Timer objects enable the user to specify the callback into the user
 * code which is called periodically. Timer objects are @ref cool::basis::named
 * "named" objects. The timer objects use the specified
 * @ref cool::gcd::task::runner "runner" to call the user callback.
 *
 * @note Upon creation the timer object is inactive and must be explicitly
 *   started using start().
 * @note Timer objects created via copy construction or copy assignment
 *   are clones and refer to the same underlying system timer. Any changes
 *   made through one of the clones will affect all clones. The system
 *   timer will be destroyed when the last clone is destroyed. You can use
 *   name() method to check whether two timer objects are clones; all clones
 *   have the same name.
 *
 * @warning The user callback must not throw.
 *
 * <b>Platform availability</b><br>
 *
 * This class is available on Mac OS/X, Linux and Windows.
 *
 */
class timer : public basis::named,
              public entrails::async_source<std::function<void(unsigned long count)>, void>
{
 public:
  /**
   * User callback type.
   *
   * The user callback must be Callable that can be assigned to function
   * type <tt>std::function<void(unsigned long)</tt>. When called,
   * the function receives the following parameters:
   *
   * @param count Number of times the timer triggered between two successive
   *   calls.
   *
   * @note If the timer is suspended through the call to suspend() it will
   *   still trigger at each period but without calling the user callback.
   *
   * @warning The user callback must not throw.
   */
  typedef std::function<void(unsigned long count)> handler_t;

private:
  typedef entrails::source_data<handler_t, void> context_t;

 public:
  /**
   * Create a timer object.
   *
   * Creates a timer object with the specified period.
   *
   * @param handler The user specified callback.
   * @param runner  The @ref cool::gcd::task::runner "runner" to use to
   *                call the user callback from
   * @param period  The period of the timer.
   *
   * @exception cool::exception::create_failure Thrown if timer cannot be created.
   * @exception cool::exception::illegal_argument Thrown if the period is set to 0.
   *
   * @note Upon creating the timer is inactive and must explicitly be activated
   *   via start().
   * @note The leeway of the timer is set to 1% of the period or to at least
   *   1 nanosecond.
   * @note The name of the timer object is prefixed with string <tt>timer-</tt>.
   */
  template <typename Rep, typename Period>
  timer(const handler_t& handler,
        const task::runner& runner,
        const std::chrono::duration<Rep, Period>& period)
      : timer(handler, runner)
  {
    set_period(period, std::chrono::duration<Rep, Period>::zero());
  }

  /**
   * Create a timer object.
   *
   * Creates a timer object with the specified period.
   *
   * @param prefix  The prefix to use for the timer object name
   * @param handler The user specified callback.
   * @param runner  The @ref cool::gcd::task::runner "runner" to use to
   *                call the user callback from
   * @param period  The period of the timer.
   *
   * @exception cool::exception::create_failure Thrown if timer cannot be created.
   * @exception cool::exception::illegal_argument Thrown if the period is set to 0.
   *
   * @note Upon creating the timer is inactive and must explicitly be activated
   *   via start().
   * @note The leeway of the timer is set to 1% of the period or to at least
   *   1 nanosecond.
   */
  template <typename Rep, typename Period>
  timer(const  std::string& prefix,
        const handler_t& handler,
        const task::runner& runner,
        const std::chrono::duration<Rep, Period>& period)
      : timer(prefix, handler, runner)
  {
    set_period(period, std::chrono::duration<Rep, Period>::zero());
  }

  /**
   * Create a timer object.
   *
   * Creates a timer object with the specified period and the specified leeway.
   *
   * @param handler The user specified callback.
   * @param runner  The @ref cool::gcd::task::runner "runner" to use to
   *                call the user callback from
   * @param period  The period of the timer.
   * @param leeway  The amount of time the system can defer the timer.
   *
   * @exception cool::exception::create_failure Thrown if timer cannot be created.
   * @exception cool::exception::illegal_argument Thrown if the period is set to 0.
   *
   * @note Upon creating the timer is inactive and must explicitly be activated
   *   via start().
   * @note The leeway is the hint from the application code up to which the system
   *   can defer the timer to align with other system activity and performance.
   *   Depending on the overall load the system may be forced to exceed the leeway.
   * @note The name of the timer object is prefixed with string <tt>timer-</tt>.
  */
  template <typename Rep, typename Period, typename Rep2, typename Period2>
  timer(handler_t& handler,
        const task::runner& runner,
        const std::chrono::duration<Rep, Period>& period,
        const std::chrono::duration<Rep2, Period2>& leeway)
      : timer(handler, runner)
  {
    set_period(period, leeway);
  }

  /**
   * Create a timer object.
   *
   * Creates a timer object with the specified interval and the specified leeway.
   *
   * @param prefix  The prefix to use for the timer object name
   * @param handler The user specified callback.
   * @param runner  The @ref cool::gcd::task::runner "runner" to use to
   *                call the user callback from
   * @param period  The period of the timer.
   * @param leeway  The amount of time the system can defer the timer.
   *
   * @exception cool::exception::create_failure Thrown if timer cannot be created.
   * @exception cool::exception::illegal_argument Thrown if the period is set to 0.
   *
   * @note Upon creating the timer is inactive and must explicitly be activated
   *   via start().
   * @note The leeway is the hint from the application code up to which the system
   *   can defer the timer to align with other system activity and performance.
   *   Depending on the overall load the system may be forced to exceed the leeway.
   */
  template <typename Rep, typename Period, typename Rep2 ,typename Period2>
  timer(const std::string& prefix,
        handler_t& handler,
        const task::runner& runner,
        const std::chrono::duration<Rep, Period>& period,
        const std::chrono::duration<Rep2, Period2>& leeway)
      : timer(prefix, handler, runner)
  {
    set_period(period, leeway);
  }

  /**
   * Create a timer object.
   *
   * Creates a timer object.
   *
   * @param handler The user specified callback.
   * @param runner  The @ref cool::gcd::task::runner "runner" to use to
   *                call the user callback from
   *
   * @note The timer period must be set via set_interval() before the timer
   *   is activated.
   * @note Upon creating the timer is inactive and must explicitly be activated
   *   via start().
   * @note The name of the timer object is prefixed with string <tt>timer-</tt>.
   */
  dlldecl timer(const handler_t& handler, const task::runner& runner)
      : timer("timer", handler, runner)
  { /* noop */ }

  /**
   * Create a timer object.
   *
   * Creates a timer object with the specified name prefix.
   *
   * @param prefix  The prefix to use for the timer object name
   * @param handler The user specified callback.
   * @param runner  The @ref cool::gcd::task::runner "runner" to use to
   *                call the user callback from
   *
   * @note The timer period must be set via set_interval() before the timer
   *   is activated.
   * @note Upon creating the timer is inactive and must explicitly be activated
   *   via start().
   */
  dlldecl timer(const std::string& prefix, const handler_t& handler, const task::runner& runner);
  /**
   * Create a timer object.
   *
   * Creates a timer object with the specified period and the specified leeway.
   *
   * @param handler The user specified callback.
   * @param runner  The @ref cool::gcd::task::runner "runner" to use to
   *                call the user callback from
   * @param period  The period of the timer in nanoseconds.
   * @param leeway  The amount of time the system can defer the timer in nanoseconds.
   *
   * @exception cool::exception::create_failure Thrown if timer cannot be created.
   * @exception cool::exception::illegal_argument Thrown if the period is set to 0.
   *
   * @note Upon creating the timer is inactive and must explicitly be activated
   *   via start().
   * @note The leeway is the hint from the application code up to which the system
   *   can defer the timer to align with other system activity and performance.
   *   Depending on the overall load the system may be forced to exceed the leeway.
   * @note The name of the timer object is prefixed with string <tt>timer-</tt>.
   * @note The leeway parameter is optional. If not specified it is set to 1% of
   *   the period or to at least 1 nanosecond.
   */
  dlldecl timer(const handler_t& handler,
        const task::runner& runner,
        uint64_t period,
        uint64_t leeway = 0)
      : timer("timer", handler, runner)
  {
    _set_period(period, leeway);
  }

  /**
   * Create a timer object.
   *
   * Creates a timer object with the specified period and the specified leeway.
   *
   * @param prefix  The prefix to use for the timer object name
   * @param handler The user specified callback.
   * @param runner  The @ref cool::gcd::task::runner "runner" to use to
   *                call the user callback from
   * @param period  The period of the timer in nanoseconds.
   * @param leeway  The amount of time the system can defer the timer in nanoseconds.
   *
   * @exception cool::exception::create_failure Thrown if timer cannot be created.
   * @exception cool::exception::illegal_argument Thrown if the period is set to 0.
   *
   * @note Upon creating the timer is inactive and must explicitly be activated
   *   via start().
   * @note The leeway is the hint from the application code up to which the system
   *   can defer the timer to align with other system activity and performance.
   *   Depending on the overall load the system may be forced to exceed the leeway.
   * @note The leeway parameter is optional. If not specified it is set to 1% of
   *   the period or to at least 1 nanosecond.
   */
  dlldecl timer(const std::string& prefix,
        const handler_t& handler,
        const task::runner& runner,
        uint64_t period,
        uint64_t leeway = 0)
      : timer(prefix, handler, runner)
  {
    _set_period(period, leeway);
  }

  /**
   * Set or change the period of the timer.
   *
   * Sets or changes the period of the timer. For the timers created through one
   * of the constructors that do not set the timer period at the construction
   * time the period must be set before they can be started. When changing the
   * period the new period becomes effective after the call to start().
   *
   * @param period  The period of the timer.
   *
   * @exception cool::exception::illegal_argument Thrown if the period is set to 0.
   *
   * @note The leeway of the timer is set to 1% of the period or to at least
   *   1 nanosecond.
   */
  template <typename Rep, typename Period>
  void set_period(const std::chrono::duration<Rep, Period>& period)
  {
    _set_period(std::chrono::duration_cast<std::chrono::nanoseconds>(period).count(), 0);
  }

  /**
   * Set or change the period of the timer.
   *
   * Sets or changes the period of the timer. For the timers created through one
   * of the constructors that do not set the timer period at the construction
   * time the period must be set before they can be started. When changing the
   * period the new period becomes effective after the call to start().
   *
   * @param period  The period of the timer.
   * @param leeway  The amount of time the system can defer the timer.
   *
   * @exception cool::exception::illegal_argument Thrown if the period is set to 0.
   */
  template <typename Rep, typename Period, typename Rep2 ,typename Period2>
  void set_period(const std::chrono::duration<Rep, Period>& period,
                    const std::chrono::duration<Rep2, Period2>& leeway)
  {
    _set_period(std::chrono::duration_cast<std::chrono::nanoseconds>(period).count(),
                std::chrono::duration_cast<std::chrono::nanoseconds>(leeway).count());
  }

  /**
   * Set or change the period of the timer.
   *
   * Sets or changes the period of the timer. For the timers created through one
   * of the constructors that do not set the timer period at the construction
   * time the period must be set before they can be started. When changing the
   * period the new period becomes effective after the call to start().
   *
   * @param period  The period of the timer in nanoseconds.
   * @param leeway  The amount of time, in nanoseconds, the system can defer the timer.
   *
   * @exception cool::exception::illegal_argument Thrown if the period is set to 0.
   *
   * @note The leeway parameter is optional. If not specified it is set to 1% of
   *   the period or to at least 1 nanosecond.
   */
  dlldecl void set_period(uint64_t period, uint64_t leeway = 0)
  {
    _set_period(period, leeway);
  }

  /**
   * Start or restart the timer.
   *
   * Starts the timer or the first time, or restarts it. In either case the timer
   * will trigger one full period after the call to start().
   *
   * @exception cool::exception::illegal_state Thrown if the timer period is not set.
   *
   * @note start() is required after the period of the timer is changed.
   */
  dlldecl void start();

  /**
   * Suspend the timer.
   *
   * Suspends the timer by disabling the calls to the user callback. The timer
   * is till running and is triggered at each period but without calling the
   * user callback.
   */
  dlldecl void suspend() { async_source::suspend(); }

  /**
   * Resume the timer.
   *
   * Resumes the timer by enabling the calls to the user callback. Since
   * the timer was running even when disabled, the first time the timer will
   * call the user callback is at the next period as measured internally by
   * the timer.
   */
  dlldecl void resume()
  {
    async_source::resume();
  }

 private:
  static void event_handler(void* ctx);
  static void cancel_handler(void *ctx);
#if HAS_QUEUE_WITH_TARGET != 1
  static void secondary_event_handler(void* ctx);
#endif
  dlldecl void _set_period(uint64_t period, uint64_t leeway);

 private:
  uint64_t  m_period;
  uint64_t  m_leeway;
};

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// ----
// ---- File System Observer
// ----
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------

/**
 * Observe file system object.
 *
 * The fs_observer class provides a mechanism to observe events occurring on
 * selected file system object. When the event is triggered by an external
 * source, the fs_observer object will call the user provided handler
 * asynchronously with regard to the program execution. If several different
 * events occur before the user handler is called, the fs_observer will merge
 * corresponding @ref Flags "flags" into a single value by OR'ing them into
 * a single value..
 *
 * @note The fs_observer object is not active upon creation, and will not
 *   call the user handler when triggered. Use start() method to activate it.
 *
 * <b>Thread Safety</b><br>
 *
 * Instances of cool::gcd::async::fs_observer class are thread safe.
 *
 * <b>Platform availability</b><br>
 *
 * This class is available on Mac OS/X and Linux.
 *
 */
#if defined(APPLE_TARGET) || defined(LINUX_TARGET)
class fs_observer : public entrails::async_source<std::function<void(unsigned long)>, int>
{
 public:
  /**
   * User handler type.
   *
   * The user handler must be a Callable that can be assigned to the function
   * object of this type.
   *
   * @param value Bitmask specifying events that occurred on the observed
   *   file system object. Bitmask contains OR'ed values from the @ref Flags
  *    enumeration.
   *
   * @note The value is reset to 0 after the call to the user handler.
   */
  typedef std::function<void(unsigned long)> handler_t;
  /**
   * Bitmap flags to use to specify the observed characteristics, and to
   * communicate to the user handler a set of characteristics that have changed
   */
  enum Flags {
    //! The observed file system object was deleted.
    Delete     = DISPATCH_VNODE_DELETE,
    //! Write operation occurred on the observed file system object.
    Write      = DISPATCH_VNODE_WRITE,
    //! The observed file system object changed in size.
    Extend     = DISPATCH_VNODE_EXTEND,
    //! The file system object metadata changed.
    Attributes = DISPATCH_VNODE_ATTRIB,
    //! The link count of the observed file system object changed.
    Link       = DISPATCH_VNODE_LINK,
    //! The observed file system object was renamed.
    Rename     = DISPATCH_VNODE_RENAME,
    //! The observed file system object was revoked.
    Revoke     = DISPATCH_VNODE_REVOKE
  };

 private:
  typedef entrails::source_data<handler_t, int> context_t;

 public:
  /**
   * Create fs_observer object.
   *
   * Creates a new fs_observer object.
   *
   * @param handler User handler to be called when the fs_observer object is triggered.
   * @param fd File descriptor of the opened file system object to observe
   * @param events A bit mask specifying events of interest to observe on the
   *   file system object. The mask is an OR'ed combination of values from the
   *   @ref Flags enumeration.
   * @param runner  cool::gcd::task:runner to use to execute the user handler. It
   *   defaults to @ref cool::gcd::task::runner::cool_default() "library global runner"
   *
   * @note The fs_observer object is not active upon creation, and will not
   *   call the user handler when triggered. Use start() method to activate it.
   * @warning The fs_object takes ownership of the file descriptor and will
   *   close it upon destruction.
   */
  fs_observer(const handler_t& handler,
              int fd,
              unsigned long events,
              const task::runner& runner = gcd::task::runner::cool_default());
  /**
   * Start delivering the events.
   *
   * This call will start, or restart the event delivery after creation or if
   * it was stopped by a call to stop().
   */
  void start() { resume();  }

  /**
   * Stop delivering the events.
   *
   * After this call the user handler will no longer be called and all events will
   * be coalesced and delivered after the observer object is started again.
   */
  void stop()  { suspend(); }

 private:
  static void event_handler(void* ctx);
  static void cancel_handler(void* ctx);
#if HAS_QUEUE_WITH_TARGET != 1
  static void secondary_event_handler(void* ctx);
#endif
};
#endif
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// ----
// ---- Data Observer
// ----
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
/**
 * User events with data.
 *
 * The data_observer class provides a mechanism to trigger events associated with
 * the file system objects. When the observed file system objects changes one or
 * more of its characteristics that are being observed, the data_observer object
 * will call the user provided handler with an integral bit field with bits
 * being set of characteristics that have changed.
 *
 * @note The data_observer object is not active upon creation, and will not
 *   call the user handler when triggered. Use start() method to activate it.
 *
 * <b>Thread Safety</b><br>
 * Instances of cool::gcd::async::data_observer class are thread safe.
 *
 * <b>Platform availability</b><br>
 *
 * This class is available on Mac OS/X, Linux and Windows.
 *
 */
class data_observer : public entrails::async_source<std::function<void(unsigned long)>, void>
{
 public:
  /**
   * User handler type.
   *
   * The user handler must be a Callable that can be assigned to the function
   * object of this type.
   *
   * @param value Coalesced value.
   *
   * @note The value is reset to 0 after the call to the user handler.
   */
  typedef std::function<void(unsigned long value)> handler_t;
  /**
   * Strategy to use to coalesce data when multiple values are send between
   * successive calls to the user callback.
   */
  enum CoalesceStrategy {
    Add,         //!< Values are added together ignoring the mask
    BitwiseOr    //!< Mask is applied to each value; results are merged using bitwise OR
  };

 private:
  typedef entrails::source_data<handler_t, void> context_t;

 public:
  /**
   * Create data_observer object.
   *
   * Creates a data_observer object.
   *
   * @param handler User handler to be called when the data_observer object is triggered.
   * @param runner  cool::gcd::task:runner to use to execute the user handler. It
   *   defaults to @ref cool::gcd::task::runner::cool_default() "library global runner"
   * @param strategy Strategy to use to merge data sent before calls to user handler. The default
   *   strategy is CoalesceStrategy::BitwiseOr.
   * @param mask  Mask to apply to each value before merging. Defaults to 0. Note that mask
   *   is ignored if merge strategy is set to CoalesceStrategy::Add.
   *
   * @note The data_observer object is not active upon creation, and will not
   *   call the user handler when triggered. Use start() method to activate it.
   */
  dlldecl data_observer(const handler_t& handler,
                const task::runner& runner = gcd::task::runner::cool_default(),
                CoalesceStrategy strategy = BitwiseOr,
                unsigned long mask = 0);

  /**
   * Start delivering the events.
   *
   * This call will start, or restart the event delivery after creation or if
   * it was stopped by a call to stop().
   */
  dlldecl void start() { resume(); }
  /**
   * Stop delivering the events.
   *
   * After this call the user handler will no longer be called and all data will
   * be coalesced until the observer object is started again.
   */
  dlldecl void stop() { suspend(); }
  /**
   * Send value to the observer and trigger the event delivery.
   *
   * @param value Value to pass to the observer.
   *
   * @warning If the <i>value</i> is equal to 0 no event will be triggered and the user
   *   handler will not be called.
   */
  dlldecl void send(unsigned long value);

 private:
  static void cancel_handler(void *ctx);
  static void event_handler(void* ctx);
#if HAS_QUEUE_WITH_TARGET != 1
  static void secondary_event_handler(void* ctx);
#endif
};

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// ----
// ---- Process Observer
// ----
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
/**
 * Observing another process.
 *
 * The proc_observer class provides a mechanism to trigger events associated with
 * another process. The proc_observer will observe the specified process and trigger
 * an event when one or more observed things happened to the observed process.
 *
 * @note The proc_observer object is not active upon creation, and will not
 *   call the user handler when triggered. Use start() method to activate it.
 *
 * <b>Thread Safety</b><br>
 * Instances of cool::gcd::async::proc_observer class are thread safe.
 *
 * <b>Platform availability</b><br>
 *
 * This class is available on Mac OS/X.
 *
 */
#if defined(APPLE_TARGET)
class proc_observer : public entrails::async_source<std::function<void(unsigned long)>, void>
{
 public:
  /**
   * User handler type.
   *
   * The user handler must be a Callable that can be assigned to the function
   * object of this type.
   *
   * @param value Coalesced value.
   *
   * @note The value is reset to 0 after the call to the user handler.
   */
  typedef std::function<void(unsigned long)> handler_t;

  static const uint64_t EXIT = DISPATCH_PROC_EXIT;
  static const uint64_t EXEC = DISPATCH_PROC_EXEC;
  static const uint64_t FORK = DISPATCH_PROC_FORK;
  static const uint64_t SIGNAL = DISPATCH_PROC_SIGNAL;

 private:
  typedef entrails::source_data<handler_t, void> context_t;

 public:
  /**
   * Create proc_observer object.
   *
   * Creates a proc_observer object.
   *
   * @param handler User handler to be called when the data_observer object is triggered.
   * @param pid     Process identifier (PID) of the process to observe.
   * @param runner  cool::gcd::task:runner to use to execute the user handler. It
   *   defaults to @ref cool::gcd::task::runner::cool_default() "library global runner"
   * @param mask    Mask of process related events to observe. A value consisting
   *   of OR'ed process event flags:
   *      - EXIT; notify when the observed process exits
   *      - FORK; notify when the observed process creates one or more child processes
   *      - EXEC; notify when the observed process becomes another executable
   *      - SIGNAL; notify when a software signal is delivered to the observed process
   * @note The proc_observer object is not active upon creation, and will not
   *   call the user handler when triggered. Use start() method to activate it.
   */
  proc_observer(const handler_t& handler,
                pid_t pid,
                const task::runner& runner = gcd::task::runner::cool_default(),
                unsigned long mask = EXIT);

  /**
   * Start delivering the events.
   *
   * This call will start, or restart the event delivery after creation or if
   * it was stopped by a call to stop().
   */
  void start() { resume();  }
  /**
   * Stop delivering the events.
   *
   * After this call the user handler will no longer be called and all data will
   * be coalesced until the observer object is started again.
   */
  void stop()  { suspend(); }

 private:
  static void cancel_handler(void *ctx);
  static void event_handler(void* ctx);
#if HAS_QUEUE_WITH_TARGET != 1
  static void secondary_event_handler(void* ctx);
#endif
};
#endif

} } } // namespace
#endif
