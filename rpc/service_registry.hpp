/**
 * @file service_registry.hpp
 * @brief 服务注册中心
 * @author RPC Framework
 * @version 1.0
 */

#pragma once

#include "protocol.hpp"
#include "serializer.hpp"
#include "../coroutine/coroutine.hpp"
#include <unordered_map>
#include <string>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <any>

namespace rpc {

/**
 * @brief 服务方法处理器
 */
using MethodHandler = std::function<rpc::coroutine::Task<std::vector<uint8_t>>(
    const std::vector<uint8_t>&  // 请求数据
)>;

/**
 * @brief 服务信息
 */
struct ServiceInfo {
    std::string name;
    std::unordered_map<std::string, MethodHandler> methods;
    std::unordered_map<std::string, std::string> method_descriptions;
};

/**
 * @brief 服务注册中心
 * 
 * 管理服务的注册、发现和调用
 */
class ServiceRegistry {
public:
    static ServiceRegistry& instance() {
        static ServiceRegistry registry;
        return registry;
    }
    
    /**
     * @brief 注册服务
     */
    template<typename Service>
    void register_service(const std::string& name, Service* service) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        services_[name] = ServiceInfo{name};
        // 具体方法注册由生成的代码完成
    }
    
    /**
     * @brief 注册方法
     */
    void register_method(
        const std::string& service_name,
        const std::string& method_name,
        MethodHandler handler,
        const std::string& description = ""
    ) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        auto it = services_.find(service_name);
        if (it == services_.end()) {
            services_[service_name] = ServiceInfo{service_name};
            it = services_.find(service_name);
        }
        
        it->second.methods[method_name] = std::move(handler);
        if (!description.empty()) {
            it->second.method_descriptions[method_name] = description;
        }
    }
    
    /**
     * @brief 注销服务
     */
    void unregister_service(const std::string& name) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        services_.erase(name);
    }
    
    /**
     * @brief 查找方法
     */
    MethodHandler find_method(
        const std::string& service_name,
        const std::string& method_name
    ) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        auto service_it = services_.find(service_name);
        if (service_it == services_.end()) {
            return nullptr;
        }
        
        auto method_it = service_it->second.methods.find(method_name);
        if (method_it == service_it->second.methods.end()) {
            return nullptr;
        }
        
        return method_it->second;
    }
    
    /**
     * @brief 检查服务是否存在
     */
    bool has_service(const std::string& name) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return services_.find(name) != services_.end();
    }
    
    /**
     * @brief 检查方法是否存在
     */
    bool has_method(
        const std::string& service_name,
        const std::string& method_name
    ) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        auto service_it = services_.find(service_name);
        if (service_it == services_.end()) {
            return false;
        }
        
        return service_it->second.methods.find(method_name) != 
               service_it->second.methods.end();
    }
    
    /**
     * @brief 获取所有服务名
     */
    std::vector<std::string> list_services() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        std::vector<std::string> names;
        names.reserve(services_.size());
        for (const auto& [name, _] : services_) {
            names.push_back(name);
        }
        return names;
    }
    
    /**
     * @brief 获取服务的所有方法
     */
    std::vector<std::string> list_methods(const std::string& service_name) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        std::vector<std::string> methods;
        
        auto service_it = services_.find(service_name);
        if (service_it != services_.end()) {
            methods.reserve(service_it->second.methods.size());
            for (const auto& [name, _] : service_it->second.methods) {
                methods.push_back(name);
            }
        }
        
        return methods;
    }
    
    /**
     * @brief 获取服务信息
     */
    std::optional<ServiceInfo> get_service_info(const std::string& name) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        auto it = services_.find(name);
        if (it != services_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, ServiceInfo> services_;
};

/**
 * @brief 服务注册辅助类
 */
template<typename Service>
class ServiceRegistrar {
public:
    ServiceRegistrar(const std::string& name, Service* service) {
        ServiceRegistry::instance().register_service(name, service);
    }
};

/**
 * @brief 方法注册辅助宏
 */
#define RPC_REGISTER_METHOD(service, method, handler, desc) \
    rpc::rpc::ServiceRegistry::instance().register_method( \
        service, method, handler, desc)

} // namespace rpc
