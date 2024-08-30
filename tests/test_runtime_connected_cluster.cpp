#include "CelteRuntime.hpp"
#include <gtest/gtest.h>

TEST(RuntimeConnect, DirectConnect)
{
    auto runtime = celte::runtime::CelteRuntime::GetInstance();
    runtime.Start(celte::runtime::RuntimeMode::CLIENT);
    runtime.ConnectToCluster("127.0.0.1", 80);
    ASSERT_EQ(runtime.IsConnectedToCluster(), true);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}