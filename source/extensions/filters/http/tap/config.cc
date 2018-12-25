#include "extensions/filters/http/tap/config.h"

#include "envoy/registry/registry.h"

#include "extensions/filters/http/tap/tap_config_impl.h"
#include "extensions/filters/http/tap/tap_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace TapFilter {

class TapConfigFactoryImpl : public TapConfigFactory {
public:
  HttpTapConfigSharedPtr
  createHttpConfigFromProto(envoy::service::tap::v2alpha::TapConfig&& proto_config,
                            HttpTapSink& admin_streamer) override {
    return std::make_shared<HttpTapConfigImpl>(std::move(proto_config), admin_streamer);
  }
};

Http::FilterFactoryCb TapFilterFactory::createFilterFactoryFromProtoTyped(
    const envoy::config::filter::http::tap::v2alpha::Tap& proto_config,
    const std::string& stats_prefix, Server::Configuration::FactoryContext& context) {

  FilterConfigSharedPtr filter_config(new FilterConfig(
      proto_config, stats_prefix, std::make_unique<TapConfigFactoryImpl>(), context.scope(),
      context.admin(), context.singletonManager(), context.threadLocal()));
  return [filter_config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(std::make_shared<Filter>(filter_config));
  };
}

/**
 * Static registration for the tap filter. @see RegisterFactory.
 */
static Registry::RegisterFactory<TapFilterFactory,
                                 Server::Configuration::NamedHttpFilterConfigFactory>
    register_;

} // namespace TapFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
