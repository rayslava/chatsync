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

  int number = option;

  ASSERT_EQ(number, 123);
}

TEST(configParser, empty)
{
  EXPECT_THROW({
      const ConfigParser parser("");}, config_error);
}

TEST(configParser, data)
{
  const std::string testval1 = "testval";
  const std::string testval2 = "value2";
  const ConfigParser parser("data://test = testval\ntest2\t=\tvalue2");

  const std::string mapval1 = parser["test"];
  const std::string mapval2 = parser["test2"];
  ASSERT_EQ(testval1, mapval1);
  ASSERT_EQ(testval2, mapval2);
}
