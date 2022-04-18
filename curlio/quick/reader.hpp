#pragma once

#include "../error.hpp"
#include "../response.hpp"

#include <boost/asio.hpp>
#include <memory>
#include <string>

namespace curlio::quick {

/**
 * Reads the content from the given stream into a string.The handler signature is
 * `void(boost::system::error_code, std::string)`.
 *
 * @param stream This object must live as long as this operation is running.
 * @tparam Size_step The maximum allowed of read size at once.
 */
template<std::size_t Size_step, typename Async_read_stream, typename Token>
inline auto async_read_all(Async_read_stream& stream, Token&& token)
{
	return boost::asio::async_compose<Token, void(boost::system::error_code, std::string)>(
	  [&stream, old_size = std::size_t{ 0 }, str = std::string{}](auto& self, boost::system::error_code ec = {},
	                                                              std::size_t bytes_read = 0) mutable {
		  str.resize(old_size + bytes_read);
		  if (ec == boost::asio::error::eof) {
			  self.complete(boost::system::error_code{}, std::move(str));
		  } else if (ec) {
			  self.complete(ec, std::move(str));
		  } else {
			  old_size = str.size();
			  str.resize(old_size + Size_step);
			  stream.async_read_some(boost::asio::buffer(str.data() + old_size, Size_step), std::move(self));
		  }
	  },
	  token, stream);
}

/**
 * Reads the content from the given stream into a string.The handler signature is
 * `void(boost::system::error_code, std::string)`.
 *
 * @pre `response.is_active() == true`
 * @param response This object must live as long as this operation is running.
 */
template<typename Token>
inline auto async_read_all(Response& response, Token&& token)
{
	if (!response.is_active()) {
		throw boost::system::system_error{ Code::request_not_active };
	}

	return boost::asio::async_initiate<Token, void(boost::system::error_code, std::string)>(
	  [&response](auto handler) {
		  const auto length = response.content_length();
		  CURLIO_DEBUG("Content length for reading " << length);
		  if (length < 0) {
			  async_read_all<512>(response, std::move(handler));
		  } else {
			  auto str = std::make_unique<std::string>();
			  str->resize(static_cast<std::size_t>(length));
			  auto buffer   = boost::asio::buffer(*str);
			  auto executor = boost::asio::get_associated_executor(handler, response.get_executor());
			  boost::asio::async_read(response, buffer,
			                          boost::asio::bind_executor(
			                            executor, [handler = std::move(handler), str = std::move(str)](
			                                        boost::system::error_code ec, std::size_t bytes_read) mutable {
				                            CURLIO_DEBUG("Done so" << bytes_read);
				                            str->resize(bytes_read);
				                            std::move(handler)(ec, std::move(*str));
			                            }));
		  }
	  },
	  token);
}

} // namespace curlio::quick
