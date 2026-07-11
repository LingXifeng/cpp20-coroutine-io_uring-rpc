/**
 * @file service_discovery.hpp
 * @brief 服务发现
 * @author RPC Framework
 */

#pragma once

#include "../loadbalance/load_balancer.hpp"
#include <unordered_map>
#include <mutex>
#include <functional>
#include <chrono>

namespace rpc {

/**
 * @brief 服务发现配置
 */
struct ServiceDiscoveryConfig {
    int refresh_interval_ms = 10000;  // 刷新间隔
    int heartbeat_interval_ms = 5000; // 心跳间隔
    int unhealthy_threshold = 3;      // 不健康阈值
};

/**
 * @brief 服务发现接口
 */
class ServiceDiscovery {
public:
    using ServiceChangeCallback = std::function<void(
        const std::string& service,
        const std::vector<Endpoint>& endpoints
    )>;
    
    virtual ~ServiceDiscovery() = default;
    
    virtual std::vector<Endpoint> discover(const std::string& service) = 0;
    virtual void register_service(const std::string& service, 
                                  const Endpoint& endpoint) = 0;
    virtual void deregister_service(const std::string& service,
                                    const Endpoint& endpoint) = 0;
    virtual void set_callback(ServiceChangeCallback cb) = 0;
};

/**
 * @brief 静态服务发现
 */
class StaticServiceDiscovery : public ServiceDiscovery {
public:
    void add_endpoint(const std::string& service, const Endpoint& ep) {
        std::unique_lock<std::mutex> lock(mutex_);
        services_[service].push_back(ep);
    }
    
    std::vector<Endpoint> discover(const std::string& service) override {
        std::unique_lock<std::mutex> lock(mutex_);
        auto it = services_.find(service);
        if (it != services_.end()) {
            return it->second;
        }
        return {};
    }
    
    void register_service(const std::string& service,
                          const Endpoint& endpoint) override {
        add_endpoint(service, endpoint);
    }
    
    void deregister_service(const std::string& service,
                            const Endpoint& endpoint) override {
        std::unique_lock<std::mutex> lock(mutex_);
        auto it = services_.find(service);
        if (it != services_.end()) {
            auto& eps = it->second;
            eps.erase(std::remove_if(eps.begin(), eps.end(),
                [&](const Endpoint& ep) {
                    return ep.host == endpoint.host && ep.port == endpoint.port;
                }), eps.end());
        }
    }
    
    void set_callback(ServiceChangeCallback cb) override {
        callback_ = std::move(cb);
    }
    
private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::vector<Endpoint>> services_;
    ServiceChangeCallback callback_;
};

} // namespace rpc
