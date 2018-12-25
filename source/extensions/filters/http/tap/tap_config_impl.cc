#include "extensions/filters/http/tap/tap_config_impl.h"

#include "common/common/assert.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace TapFilter {

HttpTapConfigImpl::HttpTapConfigImpl(envoy::service::tap::v2alpha::TapConfig&& proto_config,
                                     HttpTapSink& admin_streamer)
    : TapConfigBaseImpl(std::move(proto_config)), admin_streamer_(admin_streamer) {
  // The streaming admin output sink is the only currently supported sink.
  ASSERT(proto_config_.output_config().sinks()[0].has_streaming_admin());
}

bool HttpTapConfigImpl::matchesRequestHeaders(const Http::HeaderMap&) { return true; }

bool HttpTapConfigImpl::matchesResponseHeaders(const Http::HeaderMap&) { return true; }

HttpPerRequestTapperPtr HttpTapConfigImpl::newPerRequestTapper() {
  return std::make_unique<HttpPerRequestTapperImpl>(shared_from_this());
}

void HttpPerRequestTapperImpl::onRequestHeaders(const Http::HeaderMap&) {
  ASSERT(false); // fixfix
}

void HttpPerRequestTapperImpl::onResponseHeaders(const Http::HeaderMap&) {
  ASSERT(false); // fixfix
}

} // namespace TapFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
