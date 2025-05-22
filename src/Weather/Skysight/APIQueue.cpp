// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef SKYSIGHT_FORECAST 
/*
*  TODO(August2111) : remove this w/o FORECAST!For this the SkysightAPI has
*  to be prepared...  
*/

#include "SkysightAPI.hpp"
#include "APIQueue.hpp"

#include "Layers.hpp"
#include "ui/event/Timer.hpp"
#include "time/DateTime.hpp"
#include "LogFile.hpp"

#include <string>
#include <vector>

SkysightAPIQueue::~SkysightAPIQueue() {
	LogFormat("SkysightAPIQueue::~SkysightAPIQueue %d", timer.IsActive());
  timer.Cancel();
}

void 
SkysightAPIQueue::DoClearingQueue() {
  timer.Cancel();
  is_clearing = false;
}

void
SkysightAPIQueue::AddDecodeJob(std::unique_ptr<CDFDecoder> &&job) {
  decode_queue.emplace_back(std::move(job));
  if(!is_busy)
    Process();
}

void 
SkysightAPIQueue::Process()
{
  is_busy = true;

  if (is_clearing) {
    DoClearingQueue();
  }
  else {

    if (!empty(decode_queue)) {
#ifdef SKYSIGHT_FORECAST 
      auto &&decode_job = decode_queue.begin();
      switch ((*decode_job)->GetStatus()) {
        case CDFDecoder::Status::Idle:
          (*decode_job)->DecodeAsync();
          if (!timer.IsActive())
            timer.Schedule(std::chrono::milliseconds(300));
          break;
        case CDFDecoder::Status::Complete:
        case CDFDecoder::Status::Error:
          (*decode_job)->Done();
          try
          {
            decode_queue.erase(decode_job);
          }
          catch (std::exception &e)
          {
            LogError(std::current_exception(),
              e.what());
          }
          break;
        case CDFDecoder::Status::Busy:
          break;
      }
#endif    
    }
  }

  if (empty(decode_queue))
    timer.Cancel();

  is_busy = false;
}

void
SkysightAPIQueue::Clear(const std::string msg)
{
  LogFormat("SkysightAPIQueue::Clear %s", msg.c_str());
  is_clearing = true;
}
#endif   // SKYSIGHT_FORECAST 
