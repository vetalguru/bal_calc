#include <ballistics/version.h>
#include <gtest/gtest.h>

TEST(Smoke, VersionExists) { EXPECT_STREQ(ballistics::version(), "1.0.0"); }
