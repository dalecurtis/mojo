// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sky/engine/testing/platform/platform_impl.h"

#include <cmath>

#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "net/base/data_url.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "sky/engine/public/platform/WebWaitableEvent.h"
#include "sky/engine/testing/platform/webthread_impl.h"

namespace sky {
namespace {

class WebWaitableEventImpl : public blink::WebWaitableEvent {
 public:
  WebWaitableEventImpl() : impl_(new base::WaitableEvent(false, false)) {}
  virtual ~WebWaitableEventImpl() {}

  virtual void wait() { impl_->Wait(); }
  virtual void signal() { impl_->Signal(); }

  base::WaitableEvent* impl() {
    return impl_.get();
  }

 private:
  scoped_ptr<base::WaitableEvent> impl_;
  DISALLOW_COPY_AND_ASSIGN(WebWaitableEventImpl);
};

}  // namespace

PlatformImpl::PlatformImpl()
    : main_loop_(base::MessageLoop::current()),
      shared_timer_func_(NULL),
      shared_timer_fire_time_(0.0),
      shared_timer_fire_time_was_set_while_suspended_(false),
      shared_timer_suspended_(0),
      current_thread_slot_(&DestroyCurrentThread) {
}

PlatformImpl::~PlatformImpl() {
}

blink::WebMimeRegistry* PlatformImpl::mimeRegistry() {
  return &mime_registry_;
}

blink::WebThemeEngine* PlatformImpl::themeEngine() {
  return &theme_engine_;
}

blink::WebString PlatformImpl::defaultLocale() {
  return blink::WebString::fromUTF8("en-US");
}

double PlatformImpl::currentTime() {
  return base::Time::Now().ToDoubleT();
}

double PlatformImpl::monotonicallyIncreasingTime() {
  return base::TimeTicks::Now().ToInternalValue() /
      static_cast<double>(base::Time::kMicrosecondsPerSecond);
}

void PlatformImpl::cryptographicallyRandomValues(unsigned char* buffer,
                                                      size_t length) {
  base::RandBytes(buffer, length);
}

void PlatformImpl::setSharedTimerFiredFunction(void (*func)()) {
  shared_timer_func_ = func;
}

void PlatformImpl::setSharedTimerFireInterval(
    double interval_seconds) {
  shared_timer_fire_time_ = interval_seconds + monotonicallyIncreasingTime();
  if (shared_timer_suspended_) {
    shared_timer_fire_time_was_set_while_suspended_ = true;
    return;
  }

  // By converting between double and int64 representation, we run the risk
  // of losing precision due to rounding errors. Performing computations in
  // microseconds reduces this risk somewhat. But there still is the potential
  // of us computing a fire time for the timer that is shorter than what we
  // need.
  // As the event loop will check event deadlines prior to actually firing
  // them, there is a risk of needlessly rescheduling events and of
  // needlessly looping if sleep times are too short even by small amounts.
  // This results in measurable performance degradation unless we use ceil() to
  // always round up the sleep times.
  int64 interval = static_cast<int64>(
      ceil(interval_seconds * base::Time::kMillisecondsPerSecond)
      * base::Time::kMicrosecondsPerMillisecond);

  if (interval < 0)
    interval = 0;

  shared_timer_.Stop();
  shared_timer_.Start(FROM_HERE, base::TimeDelta::FromMicroseconds(interval),
                      this, &PlatformImpl::DoTimeout);
}

void PlatformImpl::stopSharedTimer() {
  shared_timer_.Stop();
}

void PlatformImpl::callOnMainThread(
    void (*func)(void*), void* context) {
  main_loop_->PostTask(FROM_HERE, base::Bind(func, context));
}

blink::WebUnitTestSupport* PlatformImpl::unitTestSupport() {
  return &unit_test_support_;
}

blink::WebScrollbarBehavior* PlatformImpl::scrollbarBehavior() {
  return &scrollbar_behavior_;
}

const unsigned char* PlatformImpl::getTraceCategoryEnabledFlag(
    const char* category_name) {
  static const unsigned char buf[] = "*";
  return buf;
}

blink::WebData PlatformImpl::parseDataURL(
    const blink::WebURL& url,
    blink::WebString& mimetype_out,
    blink::WebString& charset_out) {
  std::string mimetype, charset, data;
  if (net::DataURL::Parse(url, &mimetype, &charset, &data)
      && net::IsSupportedMimeType(mimetype)) {
    mimetype_out = blink::WebString::fromUTF8(mimetype);
    charset_out = blink::WebString::fromUTF8(charset);
    return data;
  }
  return blink::WebData();
}

blink::WebThread* PlatformImpl::currentThread() {
  WebThreadImplForMessageLoop* thread =
      static_cast<WebThreadImplForMessageLoop*>(current_thread_slot_.Get());
  if (thread)
    return (thread);

  scoped_refptr<base::MessageLoopProxy> message_loop =
      base::MessageLoopProxy::current();
  if (!message_loop.get())
    return NULL;

  thread = new WebThreadImplForMessageLoop(message_loop.get());
  current_thread_slot_.Set(thread);
  return thread;
}

blink::WebWaitableEvent* PlatformImpl::createWaitableEvent() {
  return new WebWaitableEventImpl();
}

blink::WebWaitableEvent* PlatformImpl::waitMultipleEvents(
    const blink::WebVector<blink::WebWaitableEvent*>& web_events) {
  std::vector<base::WaitableEvent*> events;
  for (size_t i = 0; i < web_events.size(); ++i)
    events.push_back(static_cast<WebWaitableEventImpl*>(web_events[i])->impl());
  size_t idx = base::WaitableEvent::WaitMany(
      vector_as_array(&events), events.size());
  DCHECK_LT(idx, web_events.size());
  return web_events[idx];
}

// static
void PlatformImpl::DestroyCurrentThread(void* thread) {
  WebThreadImplForMessageLoop* impl =
      static_cast<WebThreadImplForMessageLoop*>(thread);
  delete impl;
}

}  // namespace sky
