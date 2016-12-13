#include "../src/logging.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

TEST(Logging, log)
{
  testing::internal::CaptureStderr();
  std::string line = "asdf";
  auto sink = std::make_shared<logging::LogSinkPrinter>();
  logging::LoggerImpl::get().setOutput(std::static_pointer_cast<logging::LogSink>(sink));
  TRACE << "TRACE";
  DEBUG << "DEBUG";
  INFO << "INFO";
  WARNING << "WARNING";
  ERROR << "ERROR";
  FATAL << "FATAL";
  std::this_thread::sleep_for(std::chrono::milliseconds (100));
  sink.reset();
  std::string output = testing::internal::GetCapturedStderr();
  std::cout << "Captured: " << output;
  ASSERT_THAT(output, testing::HasSubstr("TRACE"));
  ASSERT_THAT(output, testing::HasSubstr("DEBUG"));
  ASSERT_THAT(output, testing::HasSubstr("INFO"));
  ASSERT_THAT(output, testing::HasSubstr("WARNING"));
  ASSERT_THAT(output, testing::HasSubstr("ERROR"));
  ASSERT_THAT(output, testing::HasSubstr("FATAL"));
}
