// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/rtt_stats.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class RttStatsPeer {
 public:
  static QuicTime::Delta GetHalfWindowRtt(const RttStats* rtt_stats) {
    return rtt_stats->half_window_rtt_.rtt;
  }

  static QuicTime::Delta GetQuarterWindowRtt(const RttStats* rtt_stats) {
    return rtt_stats->quarter_window_rtt_.rtt;
  }
};

class RttStatsTest : public ::testing::Test {
 protected:
  RttStats rtt_stats_;
};

TEST_F(RttStatsTest, DefaultsBeforeUpdate) {
  EXPECT_LT(0u, rtt_stats_.initial_rtt_us());
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(rtt_stats_.initial_rtt_us()),
            rtt_stats_.MinRtt());
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(rtt_stats_.initial_rtt_us()),
            rtt_stats_.SmoothedRtt());
}

TEST_F(RttStatsTest, MinRtt) {
  rtt_stats_.UpdateRtt(QuicTime::Delta::FromMilliseconds(200),
                       QuicTime::Delta::Zero(),
                       QuicTime::Zero());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(200), rtt_stats_.MinRtt());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(200),
            rtt_stats_.recent_min_rtt());
  rtt_stats_.UpdateRtt(QuicTime::Delta::FromMilliseconds(10),
                       QuicTime::Delta::Zero(),
                       QuicTime::Zero().Add(
                           QuicTime::Delta::FromMilliseconds(10)));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.MinRtt());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.recent_min_rtt());
  rtt_stats_.UpdateRtt(QuicTime::Delta::FromMilliseconds(50),
                       QuicTime::Delta::Zero(),
                       QuicTime::Zero().Add(
                           QuicTime::Delta::FromMilliseconds(20)));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.MinRtt());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.recent_min_rtt());
  rtt_stats_.UpdateRtt(QuicTime::Delta::FromMilliseconds(50),
                       QuicTime::Delta::Zero(),
                       QuicTime::Zero().Add(
                           QuicTime::Delta::FromMilliseconds(30)));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.MinRtt());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.recent_min_rtt());
  rtt_stats_.UpdateRtt(QuicTime::Delta::FromMilliseconds(50),
                       QuicTime::Delta::Zero(),
                       QuicTime::Zero().Add(
                           QuicTime::Delta::FromMilliseconds(40)));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.MinRtt());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.recent_min_rtt());
}

TEST_F(RttStatsTest, RecentMinRtt) {
  rtt_stats_.UpdateRtt(QuicTime::Delta::FromMilliseconds(10),
                       QuicTime::Delta::Zero(),
                       QuicTime::Zero());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.MinRtt());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.recent_min_rtt());

  rtt_stats_.SampleNewRecentMinRtt(4);
  for (int i = 0; i < 3; ++i) {
    rtt_stats_.UpdateRtt(QuicTime::Delta::FromMilliseconds(50),
                       QuicTime::Delta::Zero(),
                       QuicTime::Zero());
    EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.MinRtt());
    EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10),
              rtt_stats_.recent_min_rtt());
  }
  rtt_stats_.UpdateRtt(QuicTime::Delta::FromMilliseconds(50),
                        QuicTime::Delta::Zero(),
                        QuicTime::Zero());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.MinRtt());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(50), rtt_stats_.recent_min_rtt());
}

TEST_F(RttStatsTest, WindowedRecentMinRtt) {
  // Set the window to 99ms, so 25ms is more than a quarter rtt.
  rtt_stats_.set_recent_min_rtt_window(QuicTime::Delta::FromMilliseconds(99));

  QuicTime now = QuicTime::Zero();
  QuicTime::Delta rtt_sample = QuicTime::Delta::FromMilliseconds(10);
  rtt_stats_.UpdateRtt(rtt_sample, QuicTime::Delta::Zero(), now);
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.MinRtt());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.recent_min_rtt());

  // Gradually increase the rtt samples and ensure the recent_min_rtt starts
  // rising.
  for (int i = 0; i < 8; ++i) {
    now = now.Add(QuicTime::Delta::FromMilliseconds(25));
    rtt_sample = rtt_sample.Add(QuicTime::Delta::FromMilliseconds(10));
    rtt_stats_.UpdateRtt(rtt_sample, QuicTime::Delta::Zero(), now);
    EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.MinRtt());
    EXPECT_EQ(rtt_sample, RttStatsPeer::GetQuarterWindowRtt(&rtt_stats_));
    EXPECT_EQ(rtt_sample.Subtract(QuicTime::Delta::FromMilliseconds(10)),
              RttStatsPeer::GetHalfWindowRtt(&rtt_stats_));
    if (i < 3) {
      EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10),
                rtt_stats_.recent_min_rtt());
    } else if (i < 5) {
      EXPECT_EQ(QuicTime::Delta::FromMilliseconds(30),
                rtt_stats_.recent_min_rtt());
    } else if (i < 7) {
      EXPECT_EQ(QuicTime::Delta::FromMilliseconds(50),
                rtt_stats_.recent_min_rtt());
    } else {
      EXPECT_EQ(QuicTime::Delta::FromMilliseconds(70),
                rtt_stats_.recent_min_rtt());
    }
  }

  // A new quarter rtt low sets that, but nothing else.
  rtt_sample = rtt_sample.Subtract(QuicTime::Delta::FromMilliseconds(5));
  rtt_stats_.UpdateRtt(rtt_sample, QuicTime::Delta::Zero(), now);
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.MinRtt());
  EXPECT_EQ(rtt_sample, RttStatsPeer::GetQuarterWindowRtt(&rtt_stats_));
  EXPECT_EQ(rtt_sample.Subtract(QuicTime::Delta::FromMilliseconds(5)),
            RttStatsPeer::GetHalfWindowRtt(&rtt_stats_));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(70),
            rtt_stats_.recent_min_rtt());

  // A new half rtt low sets that and the quarter rtt low.
  rtt_sample = rtt_sample.Subtract(QuicTime::Delta::FromMilliseconds(15));
  rtt_stats_.UpdateRtt(rtt_sample, QuicTime::Delta::Zero(), now);
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.MinRtt());
  EXPECT_EQ(rtt_sample, RttStatsPeer::GetQuarterWindowRtt(&rtt_stats_));
  EXPECT_EQ(rtt_sample, RttStatsPeer::GetHalfWindowRtt(&rtt_stats_));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(70),
            rtt_stats_.recent_min_rtt());

  // A new full window loss sets the recent_min_rtt, but not min_rtt.
  rtt_sample = QuicTime::Delta::FromMilliseconds(65);
  rtt_stats_.UpdateRtt(rtt_sample, QuicTime::Delta::Zero(), now);
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(10), rtt_stats_.MinRtt());
  EXPECT_EQ(rtt_sample, RttStatsPeer::GetQuarterWindowRtt(&rtt_stats_));
  EXPECT_EQ(rtt_sample, RttStatsPeer::GetHalfWindowRtt(&rtt_stats_));
  EXPECT_EQ(rtt_sample, rtt_stats_.recent_min_rtt());

  // A new all time low sets both the min_rtt and the recent_min_rtt.
  rtt_sample = QuicTime::Delta::FromMilliseconds(5);
  rtt_stats_.UpdateRtt(rtt_sample, QuicTime::Delta::Zero(), now);

  EXPECT_EQ(rtt_sample, rtt_stats_.MinRtt());
  EXPECT_EQ(rtt_sample, RttStatsPeer::GetQuarterWindowRtt(&rtt_stats_));
  EXPECT_EQ(rtt_sample, RttStatsPeer::GetHalfWindowRtt(&rtt_stats_));
  EXPECT_EQ(rtt_sample, rtt_stats_.recent_min_rtt());
}

TEST_F(RttStatsTest, ExpireSmoothedMetrics) {
  QuicTime::Delta initial_rtt = QuicTime::Delta::FromMilliseconds(10);
  rtt_stats_.UpdateRtt(initial_rtt, QuicTime::Delta::Zero(), QuicTime::Zero());
  EXPECT_EQ(initial_rtt, rtt_stats_.MinRtt());
  EXPECT_EQ(initial_rtt, rtt_stats_.recent_min_rtt());
  EXPECT_EQ(initial_rtt, rtt_stats_.SmoothedRtt());

  EXPECT_EQ(initial_rtt.Multiply(0.5), rtt_stats_.mean_deviation());

  // Update once with a 20ms RTT.
  QuicTime::Delta doubled_rtt = initial_rtt.Multiply(2);
  rtt_stats_.UpdateRtt(doubled_rtt, QuicTime::Delta::Zero(), QuicTime::Zero());
  EXPECT_EQ(initial_rtt.Multiply(1.125), rtt_stats_.SmoothedRtt());

  // Expire the smoothed metrics, increasing smoothed rtt and mean deviation.
  rtt_stats_.ExpireSmoothedMetrics();
  EXPECT_EQ(doubled_rtt, rtt_stats_.SmoothedRtt());
  EXPECT_EQ(initial_rtt.Multiply(0.875), rtt_stats_.mean_deviation());

  // Now go back down to 5ms and expire the smoothed metrics, and ensure the
  // mean deviation increases to 15ms.
  QuicTime::Delta half_rtt = initial_rtt.Multiply(0.5);
  rtt_stats_.UpdateRtt(half_rtt, QuicTime::Delta::Zero(), QuicTime::Zero());
  EXPECT_GT(doubled_rtt, rtt_stats_.SmoothedRtt());
  EXPECT_LT(initial_rtt, rtt_stats_.mean_deviation());
}

}  // namespace test
}  // namespace net
