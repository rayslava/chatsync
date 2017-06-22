#include "telegram.hpp"
#include "http.hpp"
#include "net.hpp"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace telegram {
  static const api::Message parseUpdate(const rapidjson::Value& update);

  TgChannel::TgChannel(Hub::Hub* hub, const std::string& config) :
    channeling::Channel(hub, config),
    _botid(_config["botid"]),
    _hash(_config["hash"]),
    _endpoint("https://" + telegram_api_srv + ":" + std::to_string(telegram_api_port)
              + "/" + _botid + ":" + _hash + "/"),
    _chat(_config["chat"]),
    _last_update_id(0)
  {}

  void TgChannel::pollThread() {
    DEBUG << "Starting telegram thread";
    while (_pipeRunning) {
      apiRequest("getUpdates");
      std::this_thread::sleep_for( std::chrono::milliseconds (100));
    }
  }

  std::future<void> TgChannel::activate() {
    return std::async(std::launch::async, [this]() {
      if (_active)
        return;
      _pipeRunning = true;
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

  void TgChannel::incoming(const messaging::message_ptr&& msg) {
    constexpr int tg_message_max = 4096;
    const std::string uri = "sendMessage";
    char message[tg_message_max];
    rapidjson::StringBuffer s;
    rapidjson::Writer<rapidjson::StringBuffer> writer(s);
    const auto textmsg = messaging::TextMessage::fromMessage(msg);
    int msglen = snprintf(message, tg_message_max, "@%s: [%s]", textmsg->user()->name().c_str(), textmsg->data().c_str());
    writer.StartObject();
    writer.Key("chat_id");
    writer.Int(_chat);
    writer.Key("text");
    writer.String(message, msglen);
    writer.EndObject();
    const auto& body_line = s.GetString();
    const size_t body_size = strlen(body_line);
    std::unique_ptr<char[]> req_body = std::unique_ptr<char[]>(new char[body_size]);
    memcpy(req_body.get(), body_line, body_size);
    http::HTTPRequest req(http::HTTPRequestType::POST,
                          telegram_api_srv,
                          "/" + _botid + ":" + _hash + "/" + uri);
    req.setBody(std::move(req_body), body_size);
    req.addHeader("content-type",   "application/json");
    req.addHeader("content-length", std::to_string(body_size));
    std::this_thread::sleep_for( std::chrono::milliseconds (1000));
    auto hr = http::PerformHTTPRequest(_endpoint + uri, req);
    hr.wait();
    auto response = hr.get();
    std::cout << response->code();
  }

  bool TgChannel::messageMatch(const api::Message& msg) const {
    return
      msg.chat.type == api::ChatType::Group &&
      msg.chat.id == _chat;
  }

  const messaging::message_ptr TgChannel::parse(const char* line) const
  {
    rapidjson::Document doc;
    if (doc.Parse(line).HasParseError()) {
      ERROR << line;
      throw telegram_error("Can't parse answer:\n" + std::string(line));
    }

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
          if (messageMatch(msg)) {
            _last_update_id = upd["update_id"].GetInt() + 1;
            TRACE << "Last update :" << _last_update_id;
            _hub->newMessage(std::move(message));
          }
        }
      }
    const auto& last_update = updates[updates.Size() - 1];
    const auto& item = parseUpdate(last_update);
    _last_update_id = last_update["update_id"].GetInt() + 1;
    TRACE << "Last update :" << _last_update_id;
    return buildTextMessage(item);
    throw telegram_error("Message without text:" + std::string(line));
  }

  const messaging::message_ptr TgChannel::buildTextMessage(const api::Message& msg) const {
    return std::make_shared<const messaging::TextMessage>(_id,
                                                          std::make_shared<const messaging::User>( messaging::User(msg.from.first_name.c_str())),
                                                          msg.text.c_str());
  }

  rapidjson::Document TgChannel::apiRequest(const std::string& uri) {
    rapidjson::StringBuffer s;
    rapidjson::Writer<rapidjson::StringBuffer> writer(s);
    writer.StartObject();
    writer.Key("timeout");
    writer.Int(60);
    writer.Key("offset");
    writer.Int(_last_update_id);
    writer.EndObject();
    const auto& body_line = s.GetString();
    const size_t body_size = strlen(body_line);
    std::unique_ptr<char[]> req_body = std::unique_ptr<char[]>(new char[body_size]);
    memcpy(req_body.get(), body_line, body_size);
    http::HTTPRequest req(http::HTTPRequestType::POST,
                          telegram_api_srv,
                          "/" + _botid + ":" + _hash + "/" + uri);
    req.setBody(std::move(req_body), body_size);
    req.addHeader("content-type",   "application/json");
    req.addHeader("content-length", std::to_string(body_size));
    std::this_thread::sleep_for( std::chrono::milliseconds (1000));
    auto hr = http::PerformHTTPRequest(_endpoint + uri, req);
    hr.wait();
    auto response = hr.get();
    std::cout << response->code();
    const void* buffer;
    size_t size;
    std::tie(buffer, size) = response->data();
    const char* charbuf = static_cast<const char *>(buffer);
    parse(charbuf);
    rapidjson::Document doc;
    return doc;
  }

  static inline api::ChatType parseChatType(const std::string& str) {
    const static std::map<const std::string, api::ChatType> types = {
      {"private", api::ChatType::Private},
      {"group", api::ChatType::Group},
      {"supergroup", api::ChatType::Supergroup},
      {"channel", api::ChatType::Channel},
    };
    return types.at(str);
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
    assert(json_msg.HasMember("chat"));
    const auto& json_chat = json_msg["chat"];
    assert(json_chat.HasMember("type"));
    assert(json_chat.HasMember("id"));
    const api::Chat chat {json_chat["id"].GetInt(),
                          parseChatType(json_chat["type"].GetString()),
                          __TG_API_CHECK_OR_ADD_STR(json_chat, "title")};
    assert(json_msg.HasMember("message_id"));
    assert(json_msg.HasMember("date"));
    const api::Message message {json_msg["message_id"].GetInt(),
                                json_msg["date"].GetInt(),
                                __TG_API_CHECK_OR_ADD_STR(json_msg, "text"),
                                user,
                                chat};
    TRACE << message.from.first_name << ":" << message.text;
    return message;
  }
#undef __TG_API_CHECK_OR_ADD_STR
}
