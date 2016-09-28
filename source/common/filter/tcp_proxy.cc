#include "tcp_proxy.h"

#include "envoy/buffer/buffer.h"
#include "envoy/event/dispatcher.h"
#include "envoy/event/timer.h"
#include "envoy/stats/stats.h"
#include "envoy/upstream/cluster_manager.h"
#include "envoy/upstream/upstream.h"

#include "common/common/assert.h"
#include "common/json/json_loader.h"

namespace Filter {

TcpProxyConfig::TcpProxyConfig(const Json::Object& config,
                               Upstream::ClusterManager& cluster_manager, Stats::Store& stats_store)
    : cluster_name_(config.getString("cluster")),
      stats_(generateStats(config.getString("stat_prefix"), stats_store)) {
  if (!cluster_manager.get(cluster_name_)) {
    throw EnvoyException(fmt::format("tcp proxy: unknown cluster '{}'", cluster_name_));
  }
}

TcpProxy::TcpProxy(TcpProxyConfigPtr config, Upstream::ClusterManager& cluster_manager)
    : config_(config), cluster_manager_(cluster_manager), downstream_callbacks_(*this),
      upstream_callbacks_(new UpstreamCallbacks(*this)) {}

TcpProxy::~TcpProxy() {
  if (upstream_connection_) {
    read_callbacks_->upstreamHost()->cluster().stats().upstream_cx_destroy_.inc();
    read_callbacks_->upstreamHost()->cluster().stats().upstream_cx_active_.dec();
    read_callbacks_->upstreamHost()->stats().cx_active_.dec();
    read_callbacks_->upstreamHost()
        ->cluster()
        .resourceManager(Upstream::ResourcePriority::Default)
        .connections()
        .dec();
    connected_timespan_->complete();
  }
}

TcpProxyStats TcpProxyConfig::generateStats(const std::string& name, Stats::Store& store) {
  std::string final_prefix = fmt::format("tcp.{}.", name);
  return {ALL_TCP_PROXY_STATS(POOL_COUNTER_PREFIX(store, final_prefix),
                              POOL_GAUGE_PREFIX(store, final_prefix))};
}

void TcpProxy::initializeUpstreamConnection() {
  Upstream::ResourceManager& upstream_cluster_resource_manager =
      cluster_manager_.get(config_->clusterName())
          ->resourceManager(Upstream::ResourcePriority::Default);

  if (!upstream_cluster_resource_manager.connections().canCreate()) {
    read_callbacks_->connection().close(Network::ConnectionCloseType::NoFlush);
    return;
  }
  Upstream::Host::CreateConnectionData conn_info =
      cluster_manager_.tcpConnForCluster(config_->clusterName());

  upstream_connection_ = std::move(conn_info.connection_);
  read_callbacks_->upstreamHost(conn_info.host_description_);
  if (!upstream_connection_) {
    read_callbacks_->connection().close(Network::ConnectionCloseType::NoFlush);
    return;
  }
  upstream_cluster_resource_manager.connections().inc();

  upstream_connection_->addReadFilter(upstream_callbacks_);
  upstream_connection_->addConnectionCallbacks(*upstream_callbacks_);
  upstream_connection_->connect();
  upstream_connection_->noDelay(true);

  connect_timeout_timer_ = read_callbacks_->connection().dispatcher().createTimer(
      [this]() -> void { onConnectTimeout(); });
  connect_timeout_timer_->enableTimer(
      cluster_manager_.get(config_->clusterName())->connectTimeout());

  read_callbacks_->upstreamHost()->cluster().stats().upstream_cx_total_.inc();
  read_callbacks_->upstreamHost()->cluster().stats().upstream_cx_active_.inc();
  read_callbacks_->upstreamHost()->stats().cx_total_.inc();
  read_callbacks_->upstreamHost()->stats().cx_active_.inc();
  connect_timespan_ =
      read_callbacks_->upstreamHost()->cluster().stats().upstream_cx_connect_ms_.allocateSpan();
  connected_timespan_ =
      read_callbacks_->upstreamHost()->cluster().stats().upstream_cx_length_ms_.allocateSpan();
}

void TcpProxy::onConnectTimeout() {
  conn_log_debug("connect timeout", read_callbacks_->connection());
  read_callbacks_->upstreamHost()->cluster().stats().upstream_cx_connect_timeout_.inc();
  upstream_connection_->close(Network::ConnectionCloseType::NoFlush);
  read_callbacks_->connection().close(Network::ConnectionCloseType::NoFlush);
}

Network::FilterStatus TcpProxy::onData(Buffer::Instance& data) {
  if (!upstream_connection_) {
    // TODO: This is done here vs. the constructor because currently the filter manager is not built
    //       to handle the downstream connection being closed during construction. The better long
    //       term solution is to have an initialize() method that is passed the relevant data and
    //       can also cause the filter manager to stop execution.
    initializeUpstreamConnection();
    if (!upstream_connection_) {
      return Network::FilterStatus::StopIteration;
    }
  }

  conn_log_trace("received {} bytes", read_callbacks_->connection(), data.length());
  upstream_connection_->write(data);
  data.drain(data.length());
  return Network::FilterStatus::StopIteration;
}

void TcpProxy::onDownstreamBufferChange(Network::ConnectionBufferType type, uint64_t,
                                        int64_t delta) {
  if (type == Network::ConnectionBufferType::Write) {
    if (delta > 0) {
      config_->stats().downstream_cx_tx_bytes_total_.add(delta);
      config_->stats().downstream_cx_tx_bytes_buffered_.add(delta);
    } else {
      config_->stats().downstream_cx_tx_bytes_buffered_.sub(std::abs(delta));
    }
  }
}

void TcpProxy::onDownstreamEvent(uint32_t event) {
  if (event & Network::ConnectionEvent::RemoteClose && upstream_connection_) {
    // TODO: If we close without flushing here we may drop some data. The downstream connection
    //       is about to go away. So to support this we need to either have a way for the downstream
    //       connection to stick around, or, we need to be able to pass this connection to a flush
    //       worker which will attempt to flush the remaining data with a timeout.
    upstream_connection_->close(Network::ConnectionCloseType::NoFlush);
  }
}

void TcpProxy::onUpstreamBufferChange(Network::ConnectionBufferType type, uint64_t, int64_t delta) {
  if (type == Network::ConnectionBufferType::Write) {
    if (delta > 0) {
      read_callbacks_->upstreamHost()->cluster().stats().upstream_cx_tx_bytes_total_.add(delta);
      read_callbacks_->upstreamHost()->cluster().stats().upstream_cx_tx_bytes_buffered_.add(delta);
    } else {
      read_callbacks_->upstreamHost()->cluster().stats().upstream_cx_tx_bytes_buffered_.sub(
          std::abs(delta));
    }
  }
}

void TcpProxy::onUpstreamData(Buffer::Instance& data) {
  read_callbacks_->connection().write(data);
  data.drain(data.length());
}

void TcpProxy::onUpstreamEvent(uint32_t event) {
  if (event & Network::ConnectionEvent::RemoteClose) {
    read_callbacks_->upstreamHost()->cluster().stats().upstream_cx_destroy_remote_.inc();
  }

  if (event & Network::ConnectionEvent::LocalClose) {
    read_callbacks_->upstreamHost()->cluster().stats().upstream_cx_destroy_local_.inc();
  }

  if (event & Network::ConnectionEvent::RemoteClose) {
    if (connect_timeout_timer_) {
      read_callbacks_->upstreamHost()->cluster().stats().upstream_cx_connect_fail_.inc();
      read_callbacks_->upstreamHost()->stats().cx_connect_fail_.inc();
    }

    // TODO: If we close without flushing here we may drop some data. For flushing to work
    //       downstream we need to drop any received data while we are waiting to flush and also
    //       setup a flush timer.
    read_callbacks_->connection().close(Network::ConnectionCloseType::NoFlush);
  } else if (event & Network::ConnectionEvent::Connected) {
    connect_timespan_->complete();
  }

  if (connect_timeout_timer_) {
    connect_timeout_timer_->disableTimer();
    connect_timeout_timer_.reset();
  }
}

} // Filter
