// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef PACKIO_CONFIG_H
#define PACKIO_CONFIG_H

#if defined(PACKIO_STANDALONE_ASIO)
#include <asio.hpp>
#else // defined(PACKIO_STANDALONE_ASIO)
#include <boost/asio.hpp>
#endif // defined(PACKIO_STANDALONE_ASIO)

namespace packio {

#if defined(PACKIO_STANDALONE_ASIO)
namespace asio = ::asio;
namespace err = ::asio;
#else // defined(PACKIO_STANDALONE_ASIO)
namespace asio = ::boost::asio;
namespace err = ::boost::system;
#endif // defined(PACKIO_STANDALONE_ASIO)


#if defined(BOOST_ASIO_HAS_CO_AWAIT) || defined(ASIO_HAS_CO_AWAIT)
#define PACKIO_HAS_CO_AWAIT 1
#endif // defined(ASIO_HAS_CO_AWAIT) || defined(ASIO_HAS_CO_AWAIT)

#if defined(BOOST_ASIO_HAS_LOCAL_SOCKETS) || defined(ASIO_HAS_LOCAL_SOCKETS)
#define PACKIO_HAS_LOCAL_SOCKETS 1
#endif // defined(BOOST_ASIO_HAS_LOCAL_SOCKETS) || defined(ASIO_HAS_LOCAL_SOCKETS)

#if defined(BOOST_ASIO_DEFAULT_COMPLETION_TOKEN)
#define PACKIO_DEFAULT_COMPLETION_TOKEN(e) \
    BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(e)
#elif defined(ASIO_DEFAULT_COMPLETION_TOKEN)
#define PACKIO_DEFAULT_COMPLETION_TOKEN(e) ASIO_DEFAULT_COMPLETION_TOKEN(e)
#else // defined(BOOST_ASIO_DEFAULT_COMPLETION_TOKEN)
#define PACKIO_DEFAULT_COMPLETION_TOKEN(e)
#endif // defined(BOOST_ASIO_DEFAULT_COMPLETION_TOKEN)

#if defined(BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE)
#define PACKIO_DEFAULT_COMPLETION_TOKEN_TYPE(e) \
    BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(e)
#elif defined(ASIO_DEFAULT_COMPLETION_TOKEN_TYPE)
#define PACKIO_DEFAULT_COMPLETION_TOKEN_TYPE(e) \
    ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(e)
#else // defined(BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE)
#define PACKIO_DEFAULT_COMPLETION_TOKEN_TYPE(e)
#endif // defined(BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE)

#if defined(BOOST_ASIO_COMPLETION_TOKEN_FOR)
#define PACKIO_COMPLETION_TOKEN_FOR(s) BOOST_ASIO_COMPLETION_TOKEN_FOR(s)
#elif defined(ASIO_COMPLETION_TOKEN_FOR)
#define PACKIO_COMPLETION_TOKEN_FOR(s) ASIO_COMPLETION_TOKEN_FOR(s)
#else // defined(BOOST_ASIO_COMPLETION_TOKEN_FOR)
#define PACKIO_COMPLETION_TOKEN_FOR(s) typename
#endif // defined (BOOST_ASIO_COMPLETION_TOKEN_FOR)

} // packio

#endif // PACKIO_CONFIG_H
