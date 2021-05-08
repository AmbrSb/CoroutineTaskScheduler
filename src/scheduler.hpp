/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2021, Amin Saba
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <memory>
#include <list>
#include <cassert>
#include <exception>
#include <any>
#include <chrono>
#include <coroutine>

#include "common.hpp"

/**
 * This macro should appear at the begining of the loop function
 * of Task classes. This gives the scheduler the control to
 * initialize the 'loop' as a coroutine without actually running
 * its body.
 */
#define csched_task_start()    co_await std::suspend_always()
/**
 * This macro should be used when the loop function can give up
 * control control although it is able to continue running.
 */
#define csched_yield()         co_await set_next_activation(0)
/**
 * This macro informs the scheduler that this loop function is
 * likely not to be ready for execution withing 'usec' microseconds
 * from now.
 */
#define csched_suspend(usec)   co_await set_next_activation(usec)

namespace coro_scheduler {
namespace corosched_detail {

template <bool>
class Task;

template <bool>
class SchedulerBase_;

class SchedulerAwaiter;

template <typename T>
concept SchedPolicyConcept = requires(T& t) {
    T{std::declval<std::list<Task<true>*> const&>()};
    (std::declval<T>()).select_next_task();
};

template <typename T>
concept SchedulerConcept = requires(T& t) {
    t.start();
    t.stop();
    t.add_task(std::declval<Task<true>*>());
};

template <SchedPolicyConcept>
class Scheduler;

static inline uint64_t
now() {
    return std::chrono::duration_cast<std::chrono::microseconds>
              (std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

/**
 * Coroutine return type. This class template (through 'TaskRet' type alias)
 * should necessarily be declared as the return type of the loop function
 * of any custom derivative of the Task class, defined by the user of this
 * library
 */
template <bool>
class TaskRet_ {
public:
    class TaskPromise {
    public:
        TaskRet_ get_return_object();
        std::suspend_always  initial_suspend();
        std::suspend_always  final_suspend();
        std::suspend_never   return_void();
        void return_value(std::any val);
        /**
         * std::any is used instead of templatization, because adding another
         * template parameter to this template would have interfered with
         * the flexibility of using this library as header-only/compiled lib.
         */
        std::any get_ret_value() const;
        void unhandled_exception();

    private:
        /**
         * Would eventually hold the value 'co_return'ed by the loop coroutine
         */
        std::any val_ = 0;
    };

    using promise_type = TaskPromise;
    using handle_type = std::coroutine_handle<promise_type>;

    TaskRet_(handle_type const& c);
    TaskRet_(TaskRet_ const& other) noexcept;
    TaskRet_(TaskRet_&& other) noexcept;
    TaskRet_& operator=(TaskRet_ const& other) noexcept;
    TaskRet_& operator=(TaskRet_&& other) noexcept;
    bool done();
    void resume();
    std::any get_ret_value() const;
    ~TaskRet_();

private:
    handle_type coro_;
};

HEADER_ONLY(template class TaskRet_<true>);
using TaskRet = TaskRet_<true>;

enum SchedulerFlags {
    AutoStopOnEmptyQueue        = 0x00000001,
};

template <bool>
class Task {
public:
    Task(uint8_t prio);

    virtual void    init() = 0;
    virtual TaskRet loop() = 0;

    void init_master();

    void resume();
    bool done();
    bool ready();
    Task& set_next_activation(uint64_t usec);
    void set_scheduler(SchedulerBase_<true>* scheduler);
    SchedulerBase_<true>* get_scheduler() { return scheduler_; }
    SchedulerAwaiter operator co_await();
    auto get_prio();
    std::any get_ret_value() const;
    virtual ~Task() = default;

private:
    mutable SchedulerBase_<true>* scheduler_;
    std::optional<TaskRet> tr;
    uint64_t sleep_for_ = 0;
    uint8_t priority_;
    bool inited_ = false;
};

HEADER_ONLY(template class Task<true>);
using TaskBase = Task<true>;

template <bool B>
class IdleTask: public TaskBase {
public:
    IdleTask();
    void init() override;
    TaskRet loop() override;
};

HEADER_ONLY(template class IdleTask<true>);

template <bool>
class SchedulerBase_ {
public:
    SchedulerBase_(SchedulerBase_ const&) = delete;
    SchedulerBase_(SchedulerBase_&&) = delete;
    SchedulerBase_& operator=(SchedulerBase_ const&) = delete;
    SchedulerBase_& operator=(SchedulerBase_&&) = delete;

    friend SchedulerAwaiter;

    void add_task(TaskBase* task);
    virtual TaskBase* get_next_task(bool step = true) = 0;
    void start();
    void stop();

protected:
    enum class SchedState {
        RUNNING, STOPPED
    };

    SchedulerBase_(SchedulerFlags flags = static_cast<SchedulerFlags>(0));

    auto get_idle_task();
    TaskBase* find_task(TaskBase* t);
    void remove_task(TaskBase* t);
    bool task_queue_empty();
    void migrate_to(corosched_detail::SchedulerConcept auto const& sched, TaskBase* t);
    void init_task_(TaskBase* task_ptr);
    auto lookup_task_(TaskBase* t);

    std::list<TaskBase*> tasks_;
    bool termination_requested_ = false;
    uint32_t flags_;
    TaskBase* current_task_;
    std::unique_ptr<IdleTask<true>> idle_task_;
};

HEADER_ONLY(template class SchedulerBase_<true>);
using SchedulerBase = SchedulerBase_<true>;

/**
 * This is the awaiter type used by the task management framework.
 */
class SchedulerAwaiter {
public:
    SchedulerAwaiter(SchedulerBase& sched, TaskBase* task);

    bool await_ready() const noexcept;
    bool await_suspend(std::coroutine_handle<> const&) const noexcept;
    void await_resume() const noexcept;

private:
    SchedulerBase& scheduler_;
    TaskBase* task_;
};

/**
 * This is the return type for the SchedulingPolicy coroutine.
 * Every call to this coroutine should return the next ready
 * task.
 */
template<bool>
class SchedulingPolicyRet_ {
public:
    class SchedulingPolicyAwaiter;

    class SchedulingPolicyPromise {
    public:
        SchedulingPolicyRet_ get_return_object();
        std::suspend_always initial_suspend();
        std::suspend_always final_suspend();
        std::suspend_never  return_void();
        void unhandled_exception();
        SchedulingPolicyAwaiter yield_value(TaskBase* task) noexcept;
        SchedulerAwaiter await_transform(TaskBase* task);

        TaskBase* get_ret_value() const noexcept;
        void set_step(bool step);
        bool get_step();

    private:
        TaskBase* current_task_;
        bool step_forward_;
    };

    using promise_type = SchedulingPolicyPromise;
    using handle_type = std::coroutine_handle<promise_type>;

    /**
     * This is the awaiter type used the scheduling policy algorithms
     */
    class SchedulingPolicyAwaiter: public std::suspend_always {
    public:
        SchedulingPolicyAwaiter(promise_type& p);
        bool await_resume() noexcept;

    private:
        promise_type& prom_;
    };

    SchedulingPolicyRet_(handle_type const& c);
    SchedulingPolicyRet_(SchedulingPolicyRet_ const& other) noexcept;
    SchedulingPolicyRet_(SchedulingPolicyRet_&& other) noexcept;
    SchedulingPolicyRet_& operator=(SchedulingPolicyRet_ const& other) noexcept;
    SchedulingPolicyRet_& operator=(SchedulingPolicyRet_&& other) noexcept;
    bool done();
    void resume();
    TaskBase* get_ret_value() const noexcept;
    void set_step(bool step);
    bool get_step();
    ~SchedulingPolicyRet_();

private:
    handle_type coro_;
};

using SchedulingPolicyRet = SchedulingPolicyRet_<true>;

/**
 * A SchedulingPolicy class is a plugin for the Scheduler template.
 * It determine the low-level scheduling algorithm/policy of the
 * scheduler.
 * 'SchedulingPolicy_RR_' as an example policy which implements a
 * simple round robin scheduler.
 */
template <bool>
class SchedulingPolicy_RR_ {
public:
    SchedulingPolicy_RR_(std::list<TaskBase*> const& tasks);

    SchedulingPolicyRet
    select_next_task();
    TaskBase* current_task();

private:
    TaskBase* current_task_;
    std::list<TaskBase*> const& tasks_;
};

HEADER_ONLY(template class SchedulingPolicy_RR_<true>);
using SchedulingPolicy       = SchedulingPolicy_RR_<true>;

HEADER_ONLY(template class SchedulingPolicyRet_<true>);
using SchedulingPolicyRet    = SchedulingPolicyRet_<true>;

} // namespace corosched_detail

using TaskBase                 = corosched_detail::TaskBase;
using TaskRet                  = corosched_detail::TaskRet;
using SchedulingPolicy         = corosched_detail::SchedulingPolicy;
using SchedulingPolicyRet      = corosched_detail::SchedulingPolicyRet;

using corosched_detail::AutoStopOnEmptyQueue;

/**
 * Scheduler is the skeleton and execution framework for individual
 * SchedulingPolicy plugins.
 *
 * @tparam SchedPolicy  The plugin that implements the actual scheduling algorithm
 */
template <corosched_detail::SchedPolicyConcept SchedPolicy>
class Scheduler final: private corosched_detail::SchedulerBase {
public:
    Scheduler(corosched_detail::SchedulerFlags flags = static_cast<corosched_detail::SchedulerFlags>(0));

    using SchedulerBase_::add_task;
    using SchedulerBase_::start;
    using SchedulerBase_::stop;

    TaskBase* get_next_task(bool step = true) override;

private:
    SchedPolicy sched_policy_;
    std::optional<SchedulingPolicyRet> sched_policy_ret_;
};

template <corosched_detail::SchedPolicyConcept SchedPolicy>
Scheduler<SchedPolicy>::Scheduler(corosched_detail::SchedulerFlags flags)
    : SchedulerBase_{flags}, sched_policy_{tasks_}
{
     sched_policy_ret_ = sched_policy_.select_next_task();
}

template <corosched_detail::SchedPolicyConcept SchedPolicy>
TaskBase*
Scheduler<SchedPolicy>::get_next_task(bool step)
{
    sched_policy_ret_->set_step(step);
    sched_policy_ret_->resume();
    return sched_policy_ret_->get_ret_value();
}

} // namespace coro_scheduler

#ifndef LIBRARY_MODE
#include "scheduler_lib_impl.cpp"
#endif // LIBRARY_MODE
