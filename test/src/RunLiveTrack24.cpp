// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "CoInstance.hpp"
#include "Tracking/LiveTrack24/Client.hpp"
#include "net/http/Init.hpp"
#include "time/BrokenDateTime.hpp"
#include "Units/System.hpp"
#include "system/Args.hpp"
#include "Operation/ConsoleOperationEnvironment.hpp"
#include "DebugReplay.hpp"
#include "co/Task.hxx"
#include "util/PrintException.hxx"

#include <cstdio>

using namespace LiveTrack24;

struct Instance : CoInstance {
  const Net::ScopeInit net_init{GetEventLoop()};
};

static Co::InvokeTask
TestTracking(int argc, char *argv[], LiveTrack24::Client &client)
{
  Args args(argc, argv, "[DRIVER] FILE [USERNAME [PASSWORD]]");
  DebugReplay *replay = CreateDebugReplay(args);
  if (replay == NULL)
    throw std::runtime_error("CreateDebugReplay() failed");

  bool has_user_id;
  UserID user_id;
  std::string username, password;
  if (args.IsEmpty()) {
    username = "";
    password = "";
    has_user_id = false;
  } else {
    username = args.ExpectNextT();
    password = args.IsEmpty() ? "" : args.ExpectNextT();

    user_id = co_await client.GetUserID(username.c_str(), password.c_str());
    has_user_id = (user_id != 0);
  }

  SessionID session = has_user_id ?
                      GenerateSessionID(user_id) : GenerateSessionID();
  printf("Generated session id: %u\n", session);


  printf("Starting tracking ... ");
  co_await client.StartTracking(session, username.c_str(),
                                password.c_str(), 10,
                                VehicleType::GLIDER, "Hornet");
  printf("done\n");

  BrokenDate now = BrokenDate::TodayUTC();

  printf("Sending positions ");
  unsigned package_id = 2;
  while (replay->Next()) {
    if (package_id % 10 == 0) {
      putchar('.');
      fflush(stdout);
    }
    const MoreData &basic = replay->Basic();

    const BrokenTime time = basic.date_time_utc;
    BrokenDateTime datetime(now.year, now.month, now.day, time.hour,
                            time.minute, time.second);

    co_await client.SendPosition(session, package_id, basic.location,
                                 (unsigned)basic.nav_altitude,
                                 (unsigned)Units::ToUserUnit(basic.ground_speed,
                                                             Unit::KILOMETER_PER_HOUR),
                                 basic.track, datetime.ToTimePoint());

    package_id++;
  }
  printf("done\n");

  printf("Stopping tracking ... ");
  co_await client.EndTracking(session, package_id);
  printf("done\n");
}

int
main(int argc, char *argv[])
try {
  Instance instance;

  Client client{*Net::curl};
  client.SetServer("www.livetrack24.com");
  instance.Run(TestTracking(argc, argv, client));
  return EXIT_SUCCESS;
} catch (...) {
  PrintException(std::current_exception());
  return EXIT_FAILURE;
}
