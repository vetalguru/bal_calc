#include <ballistics/version.h>
#include <gtest/gtest.h>

TEST(Smoke, VersionExists) { EXPECT_STREQ(ballistics::version(), "0.4.1"); }
