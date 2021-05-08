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


/**
 * Depending on whether LIBRARY_MODE preprocessor macro is defined or not,
 * this is file gets included in schduler.hpp or scheduler_lib.cpp file.
 * This enables the user to use this library in both header-only or as a
 * library.
 * See the 'LIBRARY_MODE' option in the top-level CMakeLists.txt file.
 */

namespace coro_scheduler {
namespace corosched_detail {

void
called_at_most_once()
{
    static bool already_called = false;
    assert (!already_called);
    already_called = true;
}

template <bool B>
void
SchedulerBase_<B>::start()
{
    assert(tasks_.size() >= 1);

    while (!termination_requested_) {
        TaskBase* next_task = get_idle_task();
        assert(next_task != nullptr);

        if (auto policty_verdict = get_next_task(true))
            next_task = policty_verdict;

        current_task_ = next_task;
        next_task->resume();
        current_task_ = nullptr;

        if (next_task->done()) {
            remove_task(next_task);
        }

        if (task_queue_empty() && (flags_ & AutoStopOnEmptyQueue))
            break;
    }
}

template <bool B>
void
SchedulerBase_<B>::add_task(TaskBase* task)
{
    task->set_scheduler(this);
    init_task_(task);
    tasks_.push_back(task);
}

template <bool B>
void
SchedulerBase_<B>::stop()
{
    assert(!termination_requested_);
    termination_requested_ = true;
}

template <bool B>
SchedulerBase_<B>::SchedulerBase_(SchedulerFlags flags)
    : flags_{flags}
{
    idle_task_ = std::make_unique<corosched_detail::IdleTask<true>>();
    add_task(idle_task_.get());
}

template <bool B>
auto
SchedulerBase_<B>::get_idle_task()
{
    assert(tasks_.size() >= 1);
    return idle_task_.get();
}

template <bool B>
TaskBase*
SchedulerBase_<B>::find_task(TaskBase* t)
{
    if (auto task = lookup_task_(t);
            task != end(tasks_))
        return *task;

    return nullptr;

}

template <bool B>
void
SchedulerBase_<B>::remove_task(TaskBase* t)
{
    assert(find_task(t) != nullptr);
    assert(tasks_.size() >= 2);
    assert(typeid(*t) != typeid(corosched_detail::IdleTask<true>));
    assert(current_task_ != t);
    
    auto task = lookup_task_(t);
    tasks_.erase(task);

    assert(tasks_.size() >= 1);
}

template <bool B>
bool
SchedulerBase_<B>::task_queue_empty()
{
    assert(tasks_.size() >= 1);
    return (tasks_.size() == 1);
}

template <bool B>
void
SchedulerBase_<B>::migrate_to(corosched_detail::SchedulerConcept auto const& sched, TaskBase* t)
{
    remove_task(t);
    sched.add_task(t);
}

template <bool B>
void
SchedulerBase_<B>::init_task_(TaskBase* task_ptr)
{
    task_ptr->init_master();
}

template <bool B>
auto
SchedulerBase_<B>::lookup_task_(TaskBase* t)
{
    auto task = std::find_if(begin(tasks_), end(tasks_),
                            [t](auto const& up) {
                                    return up == t;
                                });
    return task;
}

template <bool B>
Task<B>::Task(uint8_t prio)
    : priority_{prio}
{ }

template <bool B>
void
Task<B>::init_master()
{
    if (inited_) {
        assert(false);
        return;
    }
    inited_ = true;
    init();
    tr = loop();
}

template <bool B>
void
Task<B>::resume()
{
    this->tr->resume();
}

template <bool B>
bool
Task<B>::done()
{
    return tr->done();
}

template <bool B>
bool
Task<B>::ready()
{
    if (((now()) >= sleep_for_) && !tr->done())
        return true;
    return false;
}

template <bool B>
Task<B>&
Task<B>::set_next_activation(uint64_t usec)
{
    sleep_for_ = now() + usec;
    return *this;
}

template <bool B>
void
Task<B>::set_scheduler(SchedulerBase* scheduler)
{
    this->scheduler_ = scheduler;
}

SchedulerAwaiter::SchedulerAwaiter(SchedulerBase& sched, TaskBase* task)
    :scheduler_{sched}, task_{task}
{
}

bool
SchedulerAwaiter::await_ready() const noexcept
{
    auto fast_resume = (task_ == scheduler_.get_next_task(false));
    if (fast_resume)
        log(5, "fast_resume");

    return fast_resume;
}

bool
SchedulerAwaiter::await_suspend(std::coroutine_handle<> const&) const noexcept
{
    return true;
}

void
SchedulerAwaiter::await_resume() const noexcept
{
}

template <bool B>
void
SchedulingPolicyRet_<B>::SchedulingPolicyPromise::set_step(bool step)
{
    step_forward_ = step;
}

template <bool B>
bool
SchedulingPolicyRet_<B>::SchedulingPolicyPromise::get_step()
{
    return step_forward_;
}

template <bool B>
SchedulerAwaiter
SchedulingPolicyRet_<B>::SchedulingPolicyPromise::await_transform(TaskBase* task)
{
    return SchedulerAwaiter{*task->get_scheduler(), task};
}


template <bool B>
SchedulerAwaiter
Task<B>::operator co_await()
{
    return SchedulerAwaiter{*this->scheduler_, this};
}


template <bool B>
auto
Task<B>::get_prio()
{
    return priority_;
}

template <bool B>
std::any
Task<B>::get_ret_value() const
{
    return tr->get_ret_value();
}

template<bool B>
SchedulingPolicy_RR_<B>::SchedulingPolicy_RR_(std::list<TaskBase*> const& tasks)
    : tasks_{tasks}
{ }

template<bool B>
SchedulingPolicyRet
SchedulingPolicy_RR_<B>::select_next_task()
{
    while (true) {
        if (tasks_.size() == 1) {
            co_yield *begin(tasks_);
            continue;
        }
        bool any_runnable = false;
        for (auto iter = ++begin(tasks_); iter != end(tasks_); ++iter) {
            auto task = *iter;
            if (!task->done() && task->ready()) {
                any_runnable = true;
            }
        }
        if (!any_runnable) {
            co_yield *begin(tasks_);
            continue;
        }
        for (auto iter = ++begin(tasks_); iter != end(tasks_);) {
            auto task = *iter++;
            if (!task->done() && task->ready()) {
                while (true) {
                    bool step = co_yield task;
                    if (step) break;
                    if (iter == end(tasks_))
                        break;
                    task = *iter;
                }
            }
        }
    }
}

template <bool B>
TaskBase*
SchedulingPolicy_RR_<B>::current_task()
{
    return current_task_;
}

template <bool B>
SchedulingPolicyRet_<B>
SchedulingPolicyRet_<B>::SchedulingPolicyPromise::get_return_object()
{
    return SchedulingPolicyRet_{handle_type::from_promise(*this)};
}

template <bool B>
std::suspend_always
SchedulingPolicyRet_<B>::SchedulingPolicyPromise::initial_suspend()
{
    return {};
}

template <bool B>
std::suspend_always
SchedulingPolicyRet_<B>::SchedulingPolicyPromise::final_suspend()
{
    return {};
}

template <bool B>
std::suspend_never
SchedulingPolicyRet_<B>::SchedulingPolicyPromise::return_void()
{
    return {};
}

template <bool B>
void
SchedulingPolicyRet_<B>::set_step(bool step)
{
    coro_.promise().set_step(step); 
}

template <bool B>
bool
SchedulingPolicyRet_<B>::get_step()
{
    return coro_.promise().get_step(); 
}

template <bool B>
void
SchedulingPolicyRet_<B>::SchedulingPolicyPromise::unhandled_exception()
{
    try {
        std::rethrow_exception(std::current_exception());
    } catch (std::exception& exp) {
        log(0, exp.what());
    }
}

template <bool B>
auto
SchedulingPolicyRet_<B>::SchedulingPolicyPromise::yield_value(TaskBase* task) noexcept -> SchedulingPolicyAwaiter
{
    current_task_ = task;
    return SchedulingPolicyAwaiter(*this);
}

template <bool B>
SchedulingPolicyRet_<B>::SchedulingPolicyAwaiter::SchedulingPolicyAwaiter(promise_type& p)
    : prom_{p}
{ }

template <bool B>
bool
SchedulingPolicyRet_<B>::SchedulingPolicyAwaiter::await_resume() noexcept
{
    return prom_.get_step();
}

template <bool B>
SchedulingPolicyRet_<B>::SchedulingPolicyRet_(handle_type const& c)
    : coro_{c}
{ }

template <bool B>
SchedulingPolicyRet_<B>::SchedulingPolicyRet_(SchedulingPolicyRet_ const& other) noexcept
    : coro_{other.coro_}
{ }

template <bool B>
SchedulingPolicyRet_<B>::SchedulingPolicyRet_(SchedulingPolicyRet_&& other) noexcept
    : coro_{std::move(other.coro_)}
{
    other.coro_ = nullptr;
}

template <bool B>
SchedulingPolicyRet_<B>&
SchedulingPolicyRet_<B>::operator=(SchedulingPolicyRet_ const& other) noexcept
{
    return *this;
}

template <bool B>
SchedulingPolicyRet_<B>&
SchedulingPolicyRet_<B>::operator=(SchedulingPolicyRet_&& other) noexcept
{
    coro_ = std::move(other.coro_);
    other.coro_ = nullptr;
    return *this;
}

template <bool B>
bool
SchedulingPolicyRet_<B>::done()
{
    return coro_.done();
}

template <bool B>
void
SchedulingPolicyRet_<B>::resume()
{
    coro_.resume();
}

template <bool B>
TaskBase*
SchedulingPolicyRet_<B>::get_ret_value() const noexcept
{
    return coro_.promise().get_ret_value();
}

template <bool B>
SchedulingPolicyRet_<B>::~SchedulingPolicyRet_()
{
    if (coro_ != nullptr)
        coro_.destroy();
}

template <bool B>
TaskBase*
SchedulingPolicyRet_<B>::SchedulingPolicyPromise::get_ret_value() const noexcept
{
    return current_task_;
}

template <bool B>
TaskRet_<B>
TaskRet_<B>::TaskPromise::get_return_object()
{
    return TaskRet_{handle_type::from_promise(*this)};
}

template <bool B>
std::suspend_always
TaskRet_<B>::TaskPromise::initial_suspend()
{
    return {};
}

template <bool B>
std::suspend_always
TaskRet_<B>::TaskPromise::final_suspend()
{
    return {};
}

template <bool B>
std::suspend_never
TaskRet_<B>::TaskPromise::return_void()
{
    return {};
}

template <bool B>
std::any
TaskRet_<B>::TaskPromise::get_ret_value() const
{
    return val_;
}

template <bool B>
std::any
TaskRet_<B>::get_ret_value() const
{
    return coro_.promise().get_ret_value();
}

template <bool B>
void
TaskRet_<B>::TaskPromise::return_value(std::any val)
{
    val_ = val;
}

template <bool B>
void
TaskRet_<B>::TaskPromise::unhandled_exception()
{
    try {
        std::rethrow_exception(std::current_exception());
    } catch (std::exception& exp) {
        log(0, exp.what());
    }
}

template <bool B>
TaskRet_<B>::TaskRet_(handle_type const& c)
    : coro_{c}
{ }

template <bool B>
TaskRet_<B>::TaskRet_(TaskRet_ const& other) noexcept
    : coro_{other.coro_}
{
}

template <bool B>
TaskRet_<B>::TaskRet_(TaskRet_&& other) noexcept
    : coro_{std::move(other.coro_)}
{
    other.coro_ = nullptr;
}

template <bool B>
TaskRet_<B>&
TaskRet_<B>::operator=(TaskRet_ const& other) noexcept
{
    coro_ = other.coro_;
    return *this;
}

template <bool B>
TaskRet_<B>&
TaskRet_<B>::operator=(TaskRet_&& other) noexcept
{
    coro_ = std::move(other.coro_);
    other.coro_ = nullptr;
    return *this;
}

template <bool B>
bool
TaskRet_<B>::done()
{
    return coro_.done();
}

template <bool B>
void
TaskRet_<B>::resume()
{
    coro_.resume();
}

template <bool B>
TaskRet_<B>::~TaskRet_()
{
    if (coro_ != nullptr)
        coro_.destroy();
}

template <bool B>
IdleTask<B>::IdleTask()
    :TaskBase{0}
{};

template <bool B>
void
IdleTask<B>::init()
{ called_at_most_once(); }

template <bool B>
TaskRet_<true>
IdleTask<B>::loop()
{
    csched_task_start();

    while (true) {
        log(6, ".");
        sched_sleep(50000);
        csched_yield();
    }
}



} // namespace corosched_detail

} // namespace coro_scheduler
