#include "../src/config.hpp"
#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

using namespace Config;

TEST(option, create)
{
  const auto& value = "value";
  ConfigOption option(value);
}

TEST(option, string)
{
  const auto& value1 = "value";
  const auto& value2 = "value2";
  ConfigOption option(value1);
  
  std::string line = option;

  ASSERT_EQ(line, value1);
  ConfigOption a = std::string(value2);

  std::string line2 = a;
  ASSERT_EQ(line2, value2);
}

TEST(option, integer)
{
  const auto& value = "123";
  ConfigOption option(value);

  int line = option;

  ASSERT_EQ(line, 123);
}

TEST(configParser, create)
{
  const ConfigParser parser("");
}
