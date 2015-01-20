#include "../src/config.hpp"
#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

using namespace Config;

TEST(option, create)
{
  const auto& name = "option";
  const auto& value = "value";
  ConfigOption option(name, value);
}

TEST(option, string)
{
  const auto& name = "option";
  const auto& value = "value";
  ConfigOption option(name, value);
  
  std::string line = option;

  ASSERT_EQ(line, value);
}

TEST(option, integer)
{
  const auto& name = "option";
  const auto& value = "123";
  ConfigOption option(name, value);
  
  int line = option;

  ASSERT_EQ(line, 123);
}

TEST(configParser, create)
{
  const ConfigParser parser("");
}
