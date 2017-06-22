#include "telegram.hpp"
#include "http.hpp"
#include "net.hpp"

namespace telegram {
  static const api::Message parseUpdate(const rapidjson::Value& update);

  TgChannel::TgChannel(Hub::Hub* hub, const std::string& config) :
    channeling::Channel(hub, config),
    _botid(_config["botid"]),
    _hash(_config["hash"]),
    _endpoint(telegram_api_srv + "/" + _botid + ":" + _hash + "/")
  {
    std::cout << "Parsed: " << apiRequest("getUpdates")["ok"].GetBool() << std::endl;
  }

  void TgChannel::pollThread() {
    DEBUG << "Starting telegram thread";
    while (_pipeRunning) {
      std::this_thread::sleep_for( std::chrono::milliseconds (100));
    }
  }

  std::future<void> TgChannel::activate() {
    return std::async(std::launch::async, [this]() {
      if (_active)
        return;
      _pipeRunning = true;
      _fd = networking::tcp_connect(telegram_api_srv);
      _thread = std::make_unique<std::thread>(std::thread(&TgChannel::pollThread, this));
      _active = true;
    });
  }

  TgChannel::~TgChannel() {
    if (_thread) {
      _pipeRunning = false;
      _thread->join();
    }
  }

  void TgChannel::incoming(const messaging::message_ptr&& msg) {}

  const messaging::message_ptr TgChannel::parse(const char* line) const
  {
    rapidjson::Document doc;
    if (doc.Parse(line).HasParseError())
      throw telegram_error("Can't parse answer:\n" + std::string(line));

    bool ok = doc["ok"].GetBool();
    if (!ok) {
      int err = doc["error_code"].GetInt();
      throw telegram_error("Telegram reports an error " + std::to_string(err) + "\n" + std::string(line));
    }

    assert(doc["result"].IsArray());
    const auto& updates = doc["result"].GetArray();
    if (updates.Size() > 1)
      for (const auto& upd : updates) {
        if (upd.HasMember("message")) {
          const auto& msg = parseUpdate(upd);
          const auto message = buildTextMessage(msg);
          _hub->newMessage(std::move(message));
        }
      }

    const auto& item = parseUpdate(updates[updates.Size() - 1]);
    return buildTextMessage(item);
    throw telegram_error("Message without text:" + std::string(line));
  }

  const messaging::message_ptr TgChannel::buildTextMessage(const api::Message& msg) const {
    const auto message =
      std::												 make_shared<const messaging::TextMessage>(_id,
                                                                                                    std::make_shared<const messaging::User>(	   messaging::User(msg.from.first_name.c_str())),
                                                                                                    msg.text.c_str());
    return message;
  }

  rapidjson::Document TgChannel::apiRequest(const std::string& uri) {
    auto hr = http::PerformHTTPRequest(_endpoint + uri);
    hr.wait();
    auto response = hr.get();
    std::cout << response->code();
    const void* buffer;
    size_t size;
    std::tie(buffer, size) = response->data();
    const char* charbuf = static_cast<const char *>(buffer);

    rapidjson::Document doc;
    return doc;
  }

#define __TG_API_CHECK_OR_ADD_STR(obj, field) obj.HasMember(field) ? obj[field].GetString() : ""
  static const api::Message parseUpdate(const rapidjson::Value& update) {
    const auto& json_msg = update["message"];
    assert(json_msg.HasMember("from"));
    const auto& json_user = json_msg["from"];
    assert(json_user.HasMember("id"));
    const api::User user {json_user["id"].GetInt(),
                          __TG_API_CHECK_OR_ADD_STR(json_user, "first_name"),
                          __TG_API_CHECK_OR_ADD_STR(json_user, "last_name"),
                          __TG_API_CHECK_OR_ADD_STR(json_user, "lang"), };
    assert(json_msg.HasMember("message_id"));
    assert(json_msg.HasMember("date"));
    const api::Message message {json_msg["message_id"].GetInt(),
                                json_msg["date"].GetInt(),
                                __TG_API_CHECK_OR_ADD_STR(json_msg, "text"),
                                user};
    return message;
  }
#undef __TG_API_CHECK_OR_ADD_STR
}
