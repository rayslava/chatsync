#pragma once
#include <memory>
#include <stdexcept>
#include "logging.hpp"

#ifdef TLS_SUPPORT
#include <gnutls/gnutls.h>
#include <gnutls/gnutlsxx.h>
#endif

namespace networking {
  /**
   * Generic network error
   */
  class network_error: public std::runtime_error {
  public:
    network_error(std::string const& message) :
      std::runtime_error(message)
    {};
  };

  /**
   * Name resolution error
   */
  class hostname_error: public network_error {
  public:
    hostname_error(std::string const& message) :
      network_error(message)
    {};
  };

  /**
   * TLS error
   */
  class tls_error: public network_error {
  public:
    const size_t code;
    tls_error(size_t _code, std::string const& message) :
      network_error(message),
      code(_code)
    {};
  };
  int tcp_connect(const std::string& host);

#ifdef TLS_SUPPORT
  class TLSConnection {
    gnutls::client_session session;                /**< gnutls session */
    gnutls::certificate_credentials credentials;   /**< gnutls credentials TODO: support customization */
  public:
    /**
     * Create a connection
     *
     * \param tcp_fd A file descriptor of socket to the server.
     *               \b MUST be opened. E.g. using \c tcp_connect()
     */
    TLSConnection(int tcp_fd);
    ~TLSConnection();
    /**
     * Perform TLS handshake
     */
    int handshake();

    /**
     * Number of TLS bytes in socket (may differ from tcp_fd socket bytes)
     */
    ssize_t pending_bytes ();

    /**
     * Receive data from TLS
     *
     * \param buffer Allocated memory for data
     * \param count Size of \c buffer
     */
    ssize_t recv(void* buffer, size_t count);

    /**
     * Send bytes to TLS
     *
     * \param buffer Data to send
     * \param count Size of \c buffer
     */
    ssize_t send(const void * const buffer, size_t count);
  };
  std::unique_ptr<TLSConnection> tls_connect(const std::string& host);
  std::unique_ptr<TLSConnection> tls_connect(int fd);
#endif
}
