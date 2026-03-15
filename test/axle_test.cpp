#include "axle/axle.h"

#include "gtest/gtest.h"

namespace axle {

TEST(AxleTest, Version) {
    ASSERT_NE(axle::get_version(), "");
}

} // namespace axle
