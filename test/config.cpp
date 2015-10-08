#include "../src/config.hpp"
#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>
#include <cstdio>

using namespace config;

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

TEST(option, boolean)
{
  const auto& value_true = "True";
  const auto& value_false = "False";
  ConfigOption option_true(value_true);
  ConfigOption option_false(value_false);

  bool result = option_true;
  ASSERT_EQ(result, true);
  result = option_false;
  ASSERT_EQ(result, false);
}

TEST(option, direction)
{
  const auto& valueIn = "input";
  const auto& valueOut = "output";
  const auto& valueInOut = "inout";
  const auto& valueWr = "wrong";
  ConfigOption optionIn(valueIn);
  ConfigOption optionOut(valueOut);
  ConfigOption optionInOut(valueInOut);
  ConfigOption optionWr(valueWr);

  channeling::ChannelDirection dirIn = optionIn;
  channeling::ChannelDirection dirOut = optionOut;
  channeling::ChannelDirection dirInOut = optionInOut;
  channeling::ChannelDirection dirWr;

  ASSERT_EQ(dirIn,    channeling::ChannelDirection::Input);
  ASSERT_EQ(dirOut,   channeling::ChannelDirection::Output);
  ASSERT_EQ(dirInOut, channeling::ChannelDirection::Bidirectional);
  EXPECT_THROW({
    ConfigOption optionWr(valueWr);
    dirWr = optionWr;
  }, option_error);
}

TEST(configParser, empty)
{
  EXPECT_THROW({
    const ConfigParser parser("");
  }, config_error);
}

TEST(configParser, data)
{
  const std::string testval1 = "testval";
  const std::string testval2 = "value2";
  const ConfigParser parser("data://test = testval\ntest2\t=value2");

  const std::string mapval1 = parser["test"];
  const std::string mapval2 = parser["test2"];

  ASSERT_EQ(testval1, mapval1);
  ASSERT_EQ(testval2, mapval2);

  EXPECT_THROW({
    parser["no_such_option"];
  }, option_error);
}

TEST(configParser, file)
{
  // Prepare config file
  const auto configLine = "test = testval\ntest2\t=\tvalue2\n";
  char nameBuffer [L_tmpnam];
  const auto tmpnamres = tmpnam(nameBuffer);

  FILE* config = fopen(nameBuffer, "w");
  int result = fputs(configLine, config);
  if (result == EOF)
    throw std::runtime_error("Can't create temp file");

  fflush(config);
  fclose(config);

  // And apply the same test
  const std::string testval1 = "testval";
  const std::string testval2 = "value2";

  const ConfigParser parser("file://" + std::string(nameBuffer));
  if (unlink(nameBuffer))
    throw std::runtime_error("Can't delete temp file");

  const std::string mapval1 = parser["test"];
  const std::string mapval2 = parser["test2"];

  ASSERT_EQ(testval1, mapval1);
  ASSERT_EQ(testval2, mapval2);

  EXPECT_THROW({
    parser["no_such_option"];
  }, option_error);
}
