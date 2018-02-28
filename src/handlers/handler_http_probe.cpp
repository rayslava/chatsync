#include "handler_http_probe.hpp"
#include "logging.hpp"
#include "http.hpp"
#include <regex>
#include <sstream>
#include <iterator>

namespace pipelining {
  const HandlerCreatorImpl<HttpProbeHandler> HttpProbeHandler::creator("http_probe");

  /**
   * Extact all URLs from the message
   */
#ifndef _UNIT_TEST_BUILD
  static
#endif
  auto extractUrl(const std::string& message) {
    const std::regex urlRe("https?://[-a-zA-Z0-9@:%._\\+~#=]{2,256}(\\.[a-z]{2,256})?\\b([-a-zA-Z0-9@o:%_\\+.~#?&//=]*)");
    std::smatch urlMatches;
    std::list<std::string> results;
    std::string::const_iterator searchStart(message.cbegin());
    while (std::regex_search(searchStart, message.cend(), urlMatches, urlRe)) {
      results.push_back(urlMatches[0]);
      searchStart += urlMatches.position() + urlMatches.length();
      TRACE << "Found url: " << urlMatches[0];
    }
    return results;
  }

  /**
   * Extract page title from page body
   *
   * Supports <title> and <meta name="description">
   */
#ifndef _UNIT_TEST_BUILD
  static
#endif
  std::string extract_title(const std::string& html) {
    const static std::regex titleRe("<\\s?title\\s?>(.*?)<\\s?/title\\s?>");
    const static std::regex metaRe("<\\s?meta\\s(.*?(?!\\/>))name=[\"']description[\"']\\s(.*?(?!\\/>))content=[\"'](.*?)[\"']");
    std::smatch titleMatches;
    if (std::regex_search(html.cbegin(), html.cend(), titleMatches, metaRe))
      return titleMatches[3];
    if (std::regex_search(html.cbegin(), html.cend(), titleMatches, titleRe))
      return titleMatches[1];
    return "Can't extract title";
  }

  static std::shared_ptr<messaging::TextMessage> probeUrl(const uint16_t id,
							  const std::string& url) {
    const auto server_start = url.find("//") + 2;
    const auto server_end =
      url.find("/", server_start) == std::string::npos ?
      url.length() :
      url.find("/", server_start);

    const std::string host = url.substr(server_start, server_end - server_start);
    const std::string uri = url.substr(0, server_end);
    const std::string page = server_end == url.length() ? "/" : url.substr(server_end);
    DEBUG  << url << " Start: " << server_start << " End: " << server_end << " Host: " << host << "  Uri: " << uri << " Page: " << page;

    http::HTTPRequest req(http::HTTPRequestType::HEAD, host, uri);
    auto hr = http::PerformHTTPRequest(url.substr(0, server_end), req, false);
    hr.wait();
    auto result = hr.get();
    std::ostringstream result_line;
    std::future<std::pair<const void * const, size_t>> data_recv;
    result_line << "URL: [";
    if (!result->header("content-type").empty()) {
      result_line << result->header("content-type");
      if (result->header("content-type") == "text/html") {
	data_recv = std::async([&result](){ return result->data(); });
      }
    }
    if (!result->header("content-length").empty())
      result_line << ", " << result->header("content-length") << " b";
    result_line << "]";
    if (data_recv.valid()) {
      data_recv.wait();
      const auto& data = data_recv.get();
      const std::string page(static_cast<const char*>(data.first));
      result_line << ": " << extract_title(page);
    }

    return std::make_shared<messaging::TextMessage>(id,
						    std::make_shared
						    <const messaging::User>
						    (messaging::User("HTTProbe")),
						    result_line.str());
  }

  HttpProbeHandler::HttpProbeHandler(std::any _initializer) :
    Handler(_initializer),
    _send(std::any_cast<Sender&>(_initializer))
  {
  }

  std::future<message_ptr> HttpProbeHandler::processMessage(message_ptr&& msg) {
    TRACE << "Processing";
    return std::async(std::launch::async, [](uint16_t id, Sender send,
					     message_ptr&& msg){
    const auto& m = messaging::TextMessage::fromMessage(msg);
	const auto& urls = extractUrl(m->data());
	for (const auto& url : urls) {
	  const auto tm = probeUrl(id, url);
	  send(tm);
	}
	return msg; }, _id, _send, std::move(msg));
  }
}