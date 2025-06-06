// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "thread/Thread.hpp"

#include <atomic>
#include <exception>
#include <cassert>

class Job;
class OperationEnvironment;
class ThreadedOperationEnvironment;
namespace UI { class Notify; }

/**
 * An environment that runs a #Job in another thread.  It does not
 * wait for completion.  After creating this object, launch a job by
 * calling Start().  The object can be reused after Wait() has been
 * called for the previous #Job.
 */
class AsyncJobRunner final : private Thread {
  Job *job = nullptr;
  ThreadedOperationEnvironment *env = nullptr;
  UI::Notify *notify = nullptr;
  std::atomic<bool> running;

  /**
   * The exception thrown by Job::Run(), to be rethrown by Wait().
   */
  std::exception_ptr exception;

public:
#ifdef HAVE_POSIX
  AsyncJobRunner() : running(false) {}
#else
  AsyncJobRunner() : Thread("AsyncJob"), running(false) {}
#endif

  ~AsyncJobRunner() {
    /* force the caller to invoke Wait() */
    assert(!IsBusy());
  }

  /**
   * Is a #Job currently scheduled, running or finished?
   */
  bool IsBusy() const {
    return IsDefined();
  }

  /**
   * Has Job::Run() returned already?
   */
  bool HasFinished() const {
    assert(IsBusy());

    return !running.load(std::memory_order_relaxed);
  }

  /**
   * Start the specified #Job.  This object must be idle; to clear the
   * previous job, call Wait().
   *
   * @param env an OperationEnvironment that is passed to the #Job;
   * this class will wrap it inside a #ThreadedOperationEnvironment;
   * it must be valid until Wait() returns
   * @param notify an optional object that gets notified when the job
   * finishes
   */
  void Start(Job *job, OperationEnvironment &env, UI::Notify *notify=nullptr);

  /**
   * Cancel the current #Job.  Returns immediately; to wait for the
   * #Job to return, call Wait().
   *
   * This suppresses the notification: after this method returns, no
   * notification will be delivered.  It would be dangerous to deliver
   * the notification, because the notification handler will call
   * Wait() a second time.
   */
  void Cancel();

  /**
   * Wait synchronously for completion.  It clears the Job pointer and
   * returns ownership to the caller, who is responsible for deleting
   * it.
   *
   * If Job::Run() threw an exception, it gets rethrown by this method.
   *
   * This method must be called before this object is destructed.
   */
  Job *Wait();

private:
  /* virtual methods from class Thread */
  void Run() noexcept override;
};
