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

#include <unistd.h>

#include <iostream>
#include <vector>

#include "gtest/gtest.h"

#include "scheduler.hpp"

using namespace coro_scheduler;

class TaskA: public TaskBase {
public:

    TaskA(std::string name, uint8_t prio, int expected)
        : TaskBase{prio}, name_{name}, expected_{expected}
    { }

    void init() override {
        std::cerr << "init [ " << name_ << " ]\n";
        inited_ = true;
    }

    TaskRet loop() override {
        csched_task_start();
        for (int i = 0; i < 4; ++i) {
            std::cerr << "loop (" << i << ") [ " << name_ << " ]\n";
            csched_suspend(10000);
        }
        co_return expected_;
    }

    std::string name_;
    int expected_;
    bool inited_ = false;
};

using namespace std::literals;

TEST(CoroutineTaskScheduler, OneTasks)
{
    Scheduler<SchedulingPolicy> sched{AutoStopOnEmptyQueue};

    std::vector<TaskA> tasks {
        TaskA("T1"s, 0, 9),
        TaskA("T2"s, 0, 4),
    };

    for (auto& task: tasks)
        sched.add_task(&task);

    sched.start();

    for (auto const& task: tasks) {
        ASSERT_EQ(task.inited_, true);
        ASSERT_EQ(task.expected_, any_cast<int>(task.get_ret_value()));
    }
}
