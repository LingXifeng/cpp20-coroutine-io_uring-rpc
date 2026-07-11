/**
 * @file load_balancer.hpp
 * @brief 负载均衡策略
 * @author RPC Framework
 */

#pragma once

#include <vector>
#include <string>
#include <random>
#include <atomic>
#include <algorithm>

namespace rpc {

/**
 * @brief 服务端点
 */
struct Endpoint {
    std::string host;
    uint16_t port;
    int weight;
    bool healthy;
    
    std::string to_string() const {
        return host + ":" + std::to_string(port);
    }
};

/**
 * @brief 负载均衡器基类
 */
class LoadBalancer {
public:
    virtual ~LoadBalancer() = default;
    
    virtual void update(const std::vector<Endpoint>& endpoints) = 0;
    virtual std::optional<Endpoint> select() = 0;
    virtual void mark_success(const Endpoint& ep) = 0;
    virtual void mark_failure(const Endpoint& ep) = 0;
};

/**
 * @brief 轮询负载均衡
 */
class RoundRobinLoadBalancer : public LoadBalancer {
public:
    void update(const std::vector<Endpoint>& endpoints) override {
        endpoints_ = endpoints;
        current_.store(0);
    }
    
    std::optional<Endpoint> select() override {
        if (endpoints_.empty()) return std::nullopt;
        
        size_t idx = current_.fetch_add(1) % endpoints_.size();
        return endpoints_[idx];
    }
    
    void mark_success(const Endpoint&) override {}
    void mark_failure(const Endpoint&) override {}
    
private:
    std::vector<Endpoint> endpoints_;
    std::atomic<size_t> current_{0};
};

/**
 * @brief 加权轮询负载均衡
 */
class WeightedRoundRobinLoadBalancer : public LoadBalancer {
public:
    void update(const std::vector<Endpoint>& endpoints) override {
        endpoints_ = endpoints;
        current_.store(0);
        
        // 构建加权索引
        weighted_indices_.clear();
        for (size_t i = 0; i < endpoints.size(); ++i) {
            int weight = std::max(1, endpoints[i].weight);
            for (int j = 0; j < weight; ++j) {
                weighted_indices_.push_back(i);
            }
        }
    }
    
    std::optional<Endpoint> select() override {
        if (weighted_indices_.empty()) return std::nullopt;
        
        size_t idx = current_.fetch_add(1) % weighted_indices_.size();
        return endpoints_[weighted_indices_[idx]];
    }
    
    void mark_success(const Endpoint&) override {}
    void mark_failure(const Endpoint&) override {}
    
private:
    std::vector<Endpoint> endpoints_;
    std::vector<size_t> weighted_indices_;
    std::atomic<size_t> current_{0};
};

/**
 * @brief 随机负载均衡
 */
class RandomLoadBalancer : public LoadBalancer {
public:
    void update(const std::vector<Endpoint>& endpoints) override {
        endpoints_ = endpoints;
    }
    
    std::optional<Endpoint> select() override {
        if (endpoints_.empty()) return std::nullopt;
        
        std::uniform_int_distribution<size_t> dist(0, endpoints_.size() - 1);
        return endpoints_[dist(rng_)];
    }
    
    void mark_success(const Endpoint&) override {}
    void mark_failure(const Endpoint&) override {}
    
private:
    std::vector<Endpoint> endpoints_;
    std::mt19937 rng_{std::random_device{}()};
};

} // namespace rpc
