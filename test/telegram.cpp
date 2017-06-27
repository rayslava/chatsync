#include "../src/telegram.hpp"
#include "../src/channel.hpp"
#include "../src/http.hpp"
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstdio>
#include <fcntl.h>

namespace telegram {

  static std::atomic_bool _keep_connected(ATOMIC_FLAG_INIT);
  std::atomic_bool _read(ATOMIC_FLAG_INIT);

  struct MockedConnection: public http::ConnectionManager {
    MockedConnection(const std::string& data) : _data(data) {};
    ~MockedConnection() {};
    ssize_t recv(void* buffer, size_t count) override {
      if (_read) {
        while (_keep_connected)
          std::this_thread::sleep_for( std::chrono::milliseconds (200) );
        return 0;
      }
      _read = true;
      memcpy(buffer, _data.c_str(), std::min(_data.length(), count));
      return std::min(_data.length(), count);
    }
    ssize_t send(const void * const buffer, size_t count) override { return 0; }
    ssize_t pending() {return _read ? 0 : _data.length();};
  private:
    const std::string _data;
  };


  static std::string response_contents;

  std::unique_ptr<http::HTTPResponse>
  TgChannel::httpRequest(const std::string& srv, const http::HTTPRequest& req) const {
    if (_read) {
      return nullptr;
    }
    std::string line = "HTTP/1.1 200 OK\r\nContent-Length: " +
                       std::to_string(response_contents.length()) + "\r\n\r\n" +
                       response_contents;
    std::cout << "Testing with '" << line << "'" << std::endl;
    auto conn = std::make_unique<MockedConnection>(line);
    auto res = std::make_unique<http::HTTPResponse>(std::move(conn));
    return res;
  }

  TEST(Telegram, Name)
  {
    DEFAULT_LOGGING;
    const auto hub = new Hub::Hub ("Hub");
    const auto ich = channeling::ChannelFactory::create("telegram", hub, "data://direction=inout\nname=tg\nbotid=a\nhash=b\nchat=c");
    // Check that at least constructor works
    ASSERT_EQ(ich->name(),	"tg");
    ASSERT_EQ(ich->direction(), channeling::ChannelDirection::Bidirectional);
    delete hub;
  }

  TEST(Telegram, empty)
  {
    DEFAULT_LOGGING
    rapidjson::StringBuffer s;
    rapidjson::Writer<rapidjson::StringBuffer> writer(s);
    writer.StartObject();
    writer.Key("ok");
    writer.Bool(true);
    writer.Key("result");
    writer.StartArray();
    writer.EndArray();
    writer.EndObject();
    response_contents = s.GetString();
    const auto hub = new Hub::Hub ("Hub");
    //    const auto ich = channeling::ChannelFactory::create("telegram", hub, "data://direction=inout\nname=tg\nbotid=" + botid + "\nhash=" + hash + "\nchat=" + chat);
    const auto ich = channeling::ChannelFactory::create("telegram", hub, "data://direction=inout\nname=tg\nbotid=a\nhash=b\nchat=c");
    channeling::ChannelFactory::create("file", hub, "data://direction=output\nname=outfile");
    auto msg = std::make_shared<const messaging::TextMessage>(0,
                                                              std::make_shared<const messaging::User>( messaging::User("R")), "test");

    _read = false;
    _keep_connected = true;
    hub->activate();
    std::this_thread::sleep_for( std::chrono::milliseconds (10) );
    hub->deactivate();
    _keep_connected = false;

    // Check that at least constructor works
    delete hub;
  }

  TEST(Telegram, message)
  {
    DEFAULT_LOGGING
      response_contents = "{\"ok\":true,\"result\":[{\"update_id\":544181644, \"message\":{\"message_id\":173,\"from\":{\"id\":336435018,\"first_name\":\"tester\",\"language_code\":\"en-RU\"},\"chat\":{\"id\":336435018,\"first_name\":\"ray\",\"type\":\"private\"},\"date\":1498543700,\"text\":\"\u0422\u0435\u0441\u0442\"}}]}";
    const auto hub = new Hub::Hub ("Hub");
    //    const auto ich = channeling::ChannelFactory::create("telegram", hub, "data://direction=inout\nname=tg\nbotid=" + botid + "\nhash=" + hash + "\nchat=" + chat);
    const auto ich = channeling::ChannelFactory::create("telegram", hub, "data://direction=inout\nname=tg\nbotid=a\nhash=b\nchat=c");
    channeling::ChannelFactory::create("file", hub, "data://direction=output\nname=outfile");

    _read = false;
    _keep_connected = true;
    hub->activate();
    std::this_thread::sleep_for( std::chrono::milliseconds (10) );
    hub->deactivate();
    _keep_connected = false;

    // Check that at least constructor works
    delete hub;
    const std::string valid_line = "tester: Тест";
    const int buffer_size = valid_line.length() + 1;
    const auto buffer = new char[buffer_size];
    int fd = open("output", O_RDONLY | O_SYNC);
    ASSERT_NE(fd, -1);
    bzero(buffer, buffer_size);
    int err = read(fd, buffer, buffer_size - 1);
    ASSERT_EQ(err, buffer_size - 1);
    close(fd);
    ASSERT_STREQ(buffer, valid_line.c_str());
    delete[] buffer;
  }



}
