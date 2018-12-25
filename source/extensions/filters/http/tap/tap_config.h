#pragma once

#include "envoy/data/tap/v2alpha/http.pb.h"
#include "envoy/service/tap/v2alpha/common.pb.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace TapFilter {

/**
 * fixfix
 */
/*class TapConfig {
public:
  virtual ~TapConfig() = default;
};*/

/**
 * fixfix
 */
class HttpPerRequestTapper {
public:
  virtual ~HttpPerRequestTapper() = default;

  /**
   * fixfix
   */
  virtual void onRequestHeaders(const Http::HeaderMap& headers) PURE;

  /**
   * fixfix
   */
  virtual void onResponseHeaders(const Http::HeaderMap& headers) PURE;
};

typedef std::unique_ptr<HttpPerRequestTapper> HttpPerRequestTapperPtr;

/**
 * fixfix
 */
class HttpTapSink {
public:
  virtual ~HttpTapSink() = default;

  /**
   * fixfix
   */
  virtual void submitBufferedTrace(envoy::data::tap::v2alpha::HttpBufferedTrace&& trace) PURE;
};

/**
 * fixfix
 */
class HttpTapConfig {
public:
  virtual ~HttpTapConfig() = default;

  /**
   * fixfix
   */
  virtual HttpPerRequestTapperPtr newPerRequestTapper() PURE;
};

typedef std::shared_ptr<HttpTapConfig> HttpTapConfigSharedPtr;

/**
 * fixfix
 */
class TapConfigFactory {
public:
  virtual ~TapConfigFactory() = default;

  /**
   * fixfix
   */
  virtual HttpTapConfigSharedPtr
  createHttpConfigFromProto(envoy::service::tap::v2alpha::TapConfig&& proto_config,
                            HttpTapSink& admin_streamer) PURE;
};

typedef std::unique_ptr<TapConfigFactory> TapConfigFactoryPtr;

} // namespace TapFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
