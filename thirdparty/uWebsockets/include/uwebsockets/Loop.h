/*
 * Authored by Alex Hultman, 2018-2020.
 * Intellectual property of third-party.

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 *     http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UWS_LOOP_H
#define UWS_LOOP_H

/* The loop is lazily created per-thread and run with run() */

#include "LoopData.h"
#include <libusockets.h>
#include <iostream>

namespace uWS {
    class LoopCleaner;
struct Loop {
    friend class LoopCleaner;
private:

    Loop() = delete;
    ~Loop() = default;

    Loop *init() {
        new (us_loop_ext((us_loop_t *) this)) LoopData;
        return this;
    }



public:

    /* Freeing the default loop should be done once */
    void free() {
        LoopData *loopData = (LoopData *) us_loop_ext((us_loop_t *) this);
        loopData->~LoopData();
        /* uSockets will track whether this loop is owned by us or a borrowed alien loop */
        us_loop_free((us_loop_t *) this);
    }

    void addPostHandler(void *key, MoveOnlyFunction<void(Loop *)> &&handler) {
        LoopData *loopData = (LoopData *) us_loop_ext((us_loop_t *) this);

        loopData->postHandlers.emplace(key, std::move(handler));
    }

    /* Bug: what if you remove a handler while iterating them? */
    void removePostHandler(void *key) {
        LoopData *loopData = (LoopData *) us_loop_ext((us_loop_t *) this);

        loopData->postHandlers.erase(key);
    }

    void addPreHandler(void *key, MoveOnlyFunction<void(Loop *)> &&handler) {
        LoopData *loopData = (LoopData *) us_loop_ext((us_loop_t *) this);

        loopData->preHandlers.emplace(key, std::move(handler));
    }

    /* Bug: what if you remove a handler while iterating them? */
    void removePreHandler(void *key) {
        LoopData *loopData = (LoopData *) us_loop_ext((us_loop_t *) this);

        loopData->preHandlers.erase(key);
    }

    /* Defer this callback on Loop's thread of execution */
    void defer(MoveOnlyFunction<void()> &&cb) {
        LoopData *loopData = (LoopData *) us_loop_ext((us_loop_t *) this);

        //if (std::thread::get_id() == ) // todo: add fast path for same thread id
        loopData->deferMutex.lock();
        loopData->deferQueues[loopData->currentDeferQueue].emplace_back(std::move(cb));
        loopData->deferMutex.unlock();

        us_wakeup_loop((us_loop_t *) this);
    }

    /* Actively block and run this loop */
    void run() {
        us_loop_run((us_loop_t *) this);
    }

    /* Passively integrate with the underlying default loop */
    /* Used to seamlessly integrate with third parties such as Node.js */
    void integrate() {
        us_loop_integrate((us_loop_t *) this);
    }

    /* Dynamically change this */
    void setSilent(bool silent) {
        ((LoopData *) us_loop_ext((us_loop_t *) this))->noMark = silent;
    }
};
    class LoopCleaner {
        private:
        static void wakeupCb(us_loop_t *loop) {
            LoopData *loopData = (LoopData *) us_loop_ext(loop);

            /* Swap current deferQueue */
            loopData->deferMutex.lock();
            int oldDeferQueue = loopData->currentDeferQueue;
            loopData->currentDeferQueue = (loopData->currentDeferQueue + 1) % 2;
            loopData->deferMutex.unlock();

            /* Drain the queue */
            for (auto &x : loopData->deferQueues[oldDeferQueue]) {
                x();
            }
            loopData->deferQueues[oldDeferQueue].clear();
        }

        static void preCb(us_loop_t *loop) {
            LoopData *loopData = (LoopData *) us_loop_ext(loop);

            for (auto &p : loopData->preHandlers) {
                p.second((Loop *) loop);
            }
        }

        static void postCb(us_loop_t *loop) {
            LoopData *loopData = (LoopData *) us_loop_ext(loop);

            for (auto &p : loopData->postHandlers) {
                p.second((Loop *) loop);
            }

            /* After every event loop iteration, we must not hold the cork buffer */
            if (loopData->corkedSocket) {
                std::cerr << "Error: Cork buffer must not be held across event loop iterations!" << std::endl;
                std::terminate();
            }
        }
        public:
        Loop *const loop;
        ~LoopCleaner() {
            loop->free();
        }
        LoopCleaner()
            :loop(((Loop *) us_create_loop(nullptr, wakeupCb, preCb, postCb, sizeof(LoopData)))->init())
        {}
    };
}

#endif // UWS_LOOP_H
