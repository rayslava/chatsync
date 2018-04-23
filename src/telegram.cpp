#include "telegram.hpp"
#include "http.hpp"
#include "net.hpp"

#include <future>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace telegram {
  const channeling::ChannelCreatorImpl<TgChannel> TgChannel::creator("telegram");

  static const api::Message parseUpdate(const rapidjson::Value& update);

  TgChannel::TgChannel(Hub::Hub* hub, const std::string& config) :
    channeling::Channel(hub, config),
    _botid(_config["botid"]),
    _hash(_config["hash"]),
    _server("https://" + telegram_api_srv + ":" + std::to_string(telegram_api_port)),
    _endpoint("/" + _botid + ":" + _hash + "/"),
    _chat(_config["chat"]),
    _last_update_id(0),
    _reconnect_attempt(0)
  {}

  void TgChannel::pollThread() {
    DEBUG << "Starting telegram thread";
    while (_pipeRunning) {
      if (_reconnect_attempt > max_reconnects)
        throw channeling::connection_error(_name, "Can't connect to telegram server");
      rapidjson::StringBuffer s;
      rapidjson::Writer<rapidjson::StringBuffer> writer(s);
      writer.StartObject();
      writer.Key("timeout");
      writer.Int(60);
      writer.Key("offset");
      writer.Int(_last_update_id);
      writer.EndObject();
      const auto& body_line = std::string(s.GetString());
      apiRequest("getUpdates", body_line);
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
    const auto textmsg = messaging::TextMessage::fromMessage(msg);
    int msglen = snprintf(message, tg_message_max, "@%s: %s",
                          textmsg->user()->name().c_str(),
                          textmsg->data().c_str());
    rapidjson::StringBuffer s;
    rapidjson::Writer<rapidjson::StringBuffer> writer(s);
    writer.StartObject();
    writer.Key("chat_id");
    writer.Int(_chat);
    writer.Key("text");
    writer.String(message, msglen);
    writer.EndObject();
    const auto& body_line = std::string(s.GetString());
    apiRequest("sendMessage", body_line);
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

    if (doc["result"].IsArray()) {
      const auto& updates = doc["result"].GetArray();
      if (updates.Size() == 0)
        throw parse_error(0, "Empty array");
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
      return buildTextMessage(std::move(item));
    } else {
      const auto& update = doc["result"];
      const auto& item = parseUpdate(doc["result"]);
      _last_update_id = update["update_id"].GetInt() + 1;
      TRACE << "Last update :" << _last_update_id;
      return buildTextMessage(std::move(item));
    }
    throw telegram_error("Message without text:" + std::string(line));
  }

  const messaging::message_ptr TgChannel::buildTextMessage(const api::Message& msg) const {
    return std::make_shared<const messaging::TextMessage>(_id,
                                                          std::make_shared<const messaging::User>( messaging::User(msg.from.first_name.c_str())),
                                                          msg.text.c_str());
  }

#ifndef _UNIT_TEST_BUILD
  std::unique_ptr<http::HTTPResponse>
  TgChannel::httpRequest(const std::string& srv, const http::HTTPRequest& req) const {
    auto hr = http::PerformHTTPRequest(srv, req);
    while (_pipeRunning) {
      if (hr.wait_for(std::chrono::milliseconds (100)) == std::future_status::ready)
        return hr.get();
    }
    return nullptr;
  }
#endif

  void TgChannel::apiRequest(const std::string& uri, const std::string& body) {
    if (_reconnect_attempt)
      std::this_thread::sleep_for(_reconnect_attempt * reconnect_timeout);
    const auto body_size = body.length();
    std::unique_ptr<char[]> req_body (new char[body_size]);
    memcpy(req_body.get(), body.c_str(), body_size);
    http::HTTPRequest req(http::HTTPRequestType::POST,
                          telegram_api_srv,
                          _endpoint + uri);
    req.setBody(std::move(req_body), body_size);
    req.addHeader("content-type",   "application/json");
    req.addHeader("content-length", std::to_string(body_size));
    try {
      const auto response = httpRequest(_server, req);
      if (!response)
        return;
      DEBUG << "Got HTTP " << response->code() << " from telegram server.";
      const auto& [buffer, size] = response->data();
      const char* charbuf = static_cast<const char *>(buffer);
      try {
        _hub->newMessage(parse(charbuf));
      } catch (parse_error e) {
        if (e.code == 0) {
          DEBUG << "Empty array from Telegram. Apparently timeout triggered";
        } else {
          ERROR << "Wrong answer received from server:" << std::string(e.what());
          DEBUG << "The response is \n" << charbuf;
        }
      }
      _reconnect_attempt = 0;
    } catch (http::http_error& e) {
      WARNING << "Got HTTP protocol error " << std::to_string(e.code)
              << ". Trying to reconnect.";
      _reconnect_attempt++;
    } catch (networking::network_error& e) {
      WARNING << "Got network error " << e.what()
              << ". Trying to reconnect.";
      _reconnect_attempt++;
    }
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
    const auto& json_msg = [&update](){
                             if (update.HasMember("message"))
                               return update.FindMember("message");
                             else if (update.HasMember("result"))
                               return update.FindMember("result");
                             throw parse_error(1, std::string("Could parse neither message nor result from ") + update.GetString());
                           } ()->value;
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
