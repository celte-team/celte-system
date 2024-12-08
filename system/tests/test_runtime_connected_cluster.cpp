#include "CelteRuntime.hpp"
#include <gtest/gtest.h>

TEST(RuntimeConnect, DirectConnect)
{
    auto runtime = celte::runtime::CelteRuntime::GetInstance();
    runtime.Start(celte::runtime::RuntimeMode::SERVER);
    ASSERT_EQ(runtime.IsConnectedToCluster(), false);
    std::string ip = std::getenv("CELTE_CLUSTER_HOST");
    runtime.ConnectToCluster(ip, 80);
    ASSERT_EQ(runtime.IsConnectedToCluster(), true);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}