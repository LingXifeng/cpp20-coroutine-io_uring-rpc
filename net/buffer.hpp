/**
 * @file buffer.hpp
 * @brief 高性能缓冲区管理
 * @author RPC Framework
 * @version 1.0
 * 
 * 特性：
 * - 零拷贝设计
 * - 引用计数
 * - 支持分散/聚集I/O
 */

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <cstring>
#include <span>
#include <atomic>

namespace rpc {
namespace net {

/**
 * @brief 缓冲区块
 * 支持引用计数的内存块
 */
class BufferBlock {
public:
    explicit BufferBlock(size_t capacity = 4096)
        : data_(new char[capacity])
        , capacity_(capacity)
        , size_(0)
        , ref_count_(1)
    {}
    
    ~BufferBlock() {
        delete[] data_;
    }
    
    // 禁止拷贝
    BufferBlock(const BufferBlock&) = delete;
    BufferBlock& operator=(const BufferBlock&) = delete;
    
    // 引用计数管理
    void add_ref() { ref_count_.fetch_add(1); }
    void release() { if (ref_count_.fetch_sub(1) == 1) delete this; }
    
    char* data() { return data_; }
    const char* data() const { return data_; }
    size_t capacity() const { return capacity_; }
    size_t size() const { return size_; }
    void set_size(size_t s) { size_ = s; }
    
private:
    char* data_;
    size_t capacity_;
    size_t size_;
    std::atomic<int> ref_count_;
};

/**
 * @brief 缓冲区
 * 支持读写分离、零拷贝
 */
class Buffer {
public:
    Buffer() : read_pos_(0), write_pos_(0) {}
    
    explicit Buffer(size_t capacity)
        : block_(std::make_shared<BufferBlock>(capacity))
        , read_pos_(0)
        , write_pos_(0)
    {}
    
    // 从现有数据构造
    Buffer(const char* data, size_t size) : Buffer(size) {
        write(data, size);
    }
    
    explicit Buffer(const std::string& str) : Buffer(str.data(), str.size()) {}
    
    // ==================== 写入操作 ====================
    
    void write(const void* data, size_t size) {
        ensure_writable(size);
        std::memcpy(write_ptr(), data, size);
        write_pos_ += size;
    }
    
    void write(const std::string& str) {
        write(str.data(), str.size());
    }
    
    template<typename T>
    void write(T value) {
        write(&value, sizeof(T));
    }
    
    // ==================== 读取操作 ====================
    
    size_t read(void* data, size_t size) {
        size = std::min(size, readable());
        std::memcpy(data, read_ptr(), size);
        read_pos_ += size;
        return size;
    }
    
    template<typename T>
    T read() {
        T value;
        read(&value, sizeof(T));
        return value;
    }
    
    // ==================== 零拷贝操作 ====================
    
    std::span<const char> as_span() const {
        return {read_ptr(), readable()};
    }
    
    std::string as_string() const {
        return {read_ptr(), readable()};
    }
    
    // ==================== 状态查询 ====================
    
    size_t readable() const { return write_pos_ - read_pos_; }
    size_t writable() const { return block_ ? block_->capacity() - write_pos_ : 0; }
    bool empty() const { return readable() == 0; }
    void reset() { read_pos_ = 0; write_pos_ = 0; }
    size_t capacity() const { return block_ ? block_->capacity() : 0; }
    
    char* read_ptr() { return block_->data() + read_pos_; }
    const char* read_ptr() const { return block_->data() + read_pos_; }
    char* write_ptr() { return block_->data() + write_pos_; }
    const char* write_ptr() const { return block_->data() + write_pos_; }
    
    // ==================== 位置操作 ====================
    
    void skip(size_t n) { read_pos_ += std::min(n, readable()); }
    void retreat(size_t n) { read_pos_ -= std::min(n, read_pos_); }
    
    void clear() {
        read_pos_ = 0;
        write_pos_ = 0;
    }
    
    void compact() {
        if (read_pos_ > 0) {
            size_t size = readable();
            std::memmove(block_->data(), read_ptr(), size);
            read_pos_ = 0;
            write_pos_ = size;
        }
    }
    
private:
    std::shared_ptr<BufferBlock> block_;
    size_t read_pos_;
    size_t write_pos_;
    
    void ensure_writable(size_t size) {
        if (!block_) {
            block_ = std::make_shared<BufferBlock>(std::max(size, size_t(4096)));
            return;
        }
        
        if (writable() >= size) {
            return;
        }
        
        // 尝试压缩
        compact();
        if (writable() >= size) {
            return;
        }
        
        // 扩容
        size_t new_capacity = std::max(block_->capacity() * 2, 
                                       write_pos_ + size);
        auto new_block = std::make_shared<BufferBlock>(new_capacity);
        std::memcpy(new_block->data(), block_->data(), write_pos_);
        block_ = new_block;
    }
};

/**
 * @brief 缓冲区链
 * 用于大数据传输，支持分散/聚集I/O
 */
class BufferChain {
public:
    BufferChain() = default;
    
    void append(Buffer buffer) {
        buffers_.push_back(std::move(buffer));
    }
    
    void append(const std::string& str) {
        buffers_.emplace_back(str);
    }
    
    size_t total_size() const {
        size_t total = 0;
        for (const auto& buf : buffers_) {
            total += buf.readable();
        }
        return total;
    }
    
    std::string to_string() const {
        std::string result;
        result.reserve(total_size());
        for (const auto& buf : buffers_) {
            result.append(buf.read_ptr(), buf.readable());
        }
        return result;
    }
    
    // 获取iovec数组（用于分散/聚集I/O）
    std::vector<iovec> to_iovec() const {
        std::vector<iovec> vecs;
        vecs.reserve(buffers_.size());
        for (const auto& buf : buffers_) {
            if (buf.readable() > 0) {
                vecs.push_back({
                    .iov_base = const_cast<char*>(buf.read_ptr()),
                    .iov_len = buf.readable()
                });
            }
        }
        return vecs;
    }
    
    void clear() { buffers_.clear(); }
    bool empty() const { return buffers_.empty(); }
    size_t count() const { return buffers_.size(); }
    
private:
    std::vector<Buffer> buffers_;
};

} // namespace net
} // namespace rpc
