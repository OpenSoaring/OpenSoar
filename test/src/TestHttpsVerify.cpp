// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "CoInstance.hpp"
#include "TestUtil.hpp"
#include "lib/curl/CoRequest.hxx"
#include "lib/curl/Easy.hxx"
#include "lib/curl/Setup.hxx"
#include "net/http/Init.hpp"

// TODO(August2111): this test is wrong, because Curl::CoRequest don't make an
// exception. The windows test sseemsto be ok, because FetchOk returned with 
// false, but on Linux the return value is true in the error case - and this 
// shows a wrong test result!
// The exception handler of Curl::CoRequest is not called, because the error 
// is deferred and the productive code is not prepared for this. Then the test 
// could be enabled again.

struct Instance : CoInstance {
  const Net::ScopeInit net_init{GetEventLoop()};
};

/**
 * Test whether an HTTPS URL can be successfully fetched.
 * @return true if fetch succeeds with 2xx status, false on error or cert failure
 */
static Co::Task<bool>
FetchOk(CurlGlobal &curl, const char *url)
{
  try {
    CurlEasy easy{url};
    Curl::Setup(easy);
    easy.SetFailOnError();
    const auto response = co_await Curl::CoRequest(curl, std::move(easy));
    co_return response.status >= 200 && response.status < 300;
  } catch (...) {
    co_return false;
  }
}

int
main()
{
#if !defined(HAVE_HTTP)
  plan_skip_all("HTTP disabled");
  return exit_status();
#elif defined(ANDROID) || defined(KOBO)
  plan_skip_all("TLS verification disabled on this platform");
  return exit_status();
#else
  plan_tests(4);

  Instance instance;

  ok1(instance.Run(FetchOk(*Net::curl,
    "https://download.xcsoar.org/repository")));
  // http instead of https should not work:
  ok1(!instance.Run(FetchOk(*Net::curl,
    "http://download.xcsoar.org/repository")));
  // a wrong subpage should not work:too
  ok1(!instance.Run(FetchOk(*Net::curl,
    "https://download.xcsoar.org/unknown")));
  // a complete wrong url should also not work:
   ok1(!instance.Run(FetchOk(*Net::curl,
    "https://wrong.host.badssl.com/")));

} catch (...) {
#if 1 // !!!!!!!!!!!!!!!!!!
  std::cerr << "Exception !!!!! " << std::endl;
#endif
}
  return exit_status();
#endif
}
