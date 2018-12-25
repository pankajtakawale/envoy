#pragma once

#include "envoy/http/header_map.h"

#include "common/http/header_utility.h"

#include "extensions/filters/http/tap/tap_config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace TapFilter {

class TapConfigBaseImpl {
protected:
  TapConfigBaseImpl(envoy::service::tap::v2alpha::TapConfig&& proto_config)
      : proto_config_(std::move(proto_config)) {}

  const envoy::service::tap::v2alpha::TapConfig proto_config_;
};

class HttpTapConfigImpl : TapConfigBaseImpl,
                          public HttpTapConfig,
                          public std::enable_shared_from_this<HttpTapConfigImpl> {
public:
  HttpTapConfigImpl(envoy::service::tap::v2alpha::TapConfig&& proto_config,
                    HttpTapSink& admin_streamer);

  bool matchesRequestHeaders(const Http::HeaderMap& headers);
  bool matchesResponseHeaders(const Http::HeaderMap& headers);
  HttpTapSink& sink() { return admin_streamer_; }

  // TapFilter::HttpTapConfig
  HttpPerRequestTapperPtr newPerRequestTapper() override;

private:
  HttpTapSink& admin_streamer_;
  std::vector<Http::HeaderUtility::HeaderData> request_headers_to_match_;
  std::vector<Http::HeaderUtility::HeaderData> response_headers_to_match_;
};

typedef std::shared_ptr<HttpTapConfigImpl> HttpTapConfigImplSharedPtr;

class HttpPerRequestTapperImpl : public HttpPerRequestTapper {
public:
  HttpPerRequestTapperImpl(HttpTapConfigImplSharedPtr config) : config_(config) {}

  // TapFilter::HttpPerRequestTapper
  void onRequestHeaders(const Http::HeaderMap& headers) override;
  void onResponseHeaders(const Http::HeaderMap& headers) override;

private:
  HttpTapConfigImplSharedPtr config_;
  // bool tapping_{};
};

} // namespace TapFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
