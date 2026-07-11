/**
 * @file code_generator.hpp
 * @brief C++代码生成器
 * @author RPC Framework
 */

#pragma once

#include "idl_parser.hpp"
#include <sstream>
#include <fstream>

namespace rpc {
namespace idl {

/**
 * @brief C++代码生成器
 */
class CodeGenerator {
public:
    struct Config {
        std::string output_dir = ".";
        bool generate_serialization = true;
        bool generate_client = true;
        bool generate_server = true;
    };
    
    explicit CodeGenerator(const Config& config = {}) : config_(config) {}
    
    bool generate(const IDLDocument& doc, const std::string& filename) {
        // 生成头文件
        std::string header = generate_header(doc, filename);
        std::string header_path = config_.output_dir + "/" + filename + ".hpp";
        
        std::ofstream ofs(header_path);
        if (!ofs) return false;
        ofs << header;
        ofs.close();
        
        return true;
    }
    
private:
    Config config_;
    
    static std::string to_upper(const std::string& s) {
        std::string result = s;
        for (char& c : result) c = std::toupper(c);
        return result;
    }
    
    static std::string to_cpp_type(FieldType type, const std::string& type_name) {
        switch (type) {
            case FieldType::INT8: return "int8_t";
            case FieldType::INT16: return "int16_t";
            case FieldType::INT32: return "int32_t";
            case FieldType::INT64: return "int64_t";
            case FieldType::UINT8: return "uint8_t";
            case FieldType::UINT16: return "uint16_t";
            case FieldType::UINT32: return "uint32_t";
            case FieldType::UINT64: return "uint64_t";
            case FieldType::FLOAT: return "float";
            case FieldType::DOUBLE: return "double";
            case FieldType::STRING: return "std::string";
            case FieldType::BOOL: return "bool";
            case FieldType::VECTOR: return type_name;
            case FieldType::MAP: return type_name;
            default: return type_name;
        }
    }
    
    std::string generate_header(const IDLDocument& doc, const std::string& filename) {
        std::ostringstream oss;
        
        // 文件头
        oss << "/**\n";
        oss << " * @file " << filename << ".hpp\n";
        oss << " * @brief Auto-generated from IDL\n";
        oss << " * @author RPC Framework IDL Generator\n";
        oss << " */\n\n";
        oss << "#pragma once\n\n";
        oss << "#include \"../framework.hpp\"\n\n";
        
        // 命名空间
        if (!doc.namespace_name.empty()) {
            oss << "namespace " << doc.namespace_name << " {\n\n";
        }
        
        // 生成结构体
        for (const auto& s : doc.structs) {
            oss << generate_struct(s);
        }
        
        // 生成服务
        for (const auto& s : doc.services) {
            oss << generate_service(s);
        }
        
        // 关闭命名空间
        if (!doc.namespace_name.empty()) {
            oss << "} // namespace " << doc.namespace_name << "\n";
        }
        
        return oss.str();
    }
    
    std::string generate_struct(const StructDef& s) {
        std::ostringstream oss;
        
        // 结构体定义
        oss << "struct " << s.name << " {\n";
        for (const auto& f : s.fields) {
            oss << "    " << to_cpp_type(f.type, f.type_name) << " " << f.name << ";\n";
        }
        oss << "};\n\n";
        
        // 序列化
        if (config_.generate_serialization) {
            oss << generate_serialization_code(s);
        }
        
        return oss.str();
    }
    
    std::string generate_serialization_code(const StructDef& s) {
        std::ostringstream oss;
        
        // serialize
        oss << "namespace rpc::rpc {\n";
        oss << "template<>\n";
        oss << "std::vector<uint8_t> serialize(const " << s.name << "& obj) {\n";
        oss << "    std::vector<uint8_t> buffer;\n";
        for (const auto& f : s.fields) {
            oss << "    BinarySerializer::encode(buffer, obj." << f.name << ");\n";
        }
        oss << "    return buffer;\n";
        oss << "}\n\n";
        
        // deserialize
        oss << "template<>\n";
        oss << "bool deserialize(const std::vector<uint8_t>& buffer, " << s.name << "& obj) {\n";
        oss << "    size_t offset = 0;\n";
        for (const auto& f : s.fields) {
            oss << "    auto r_" << f.name << " = BinarySerializer::decode(\n";
            oss << "        buffer.data() + offset, buffer.size() - offset, obj." << f.name << ");\n";
            oss << "    if (!r_" << f.name << ".success) return false;\n";
            oss << "    offset += r_" << f.name << ".bytes_read;\n";
        }
        oss << "    return true;\n";
        oss << "}\n";
        oss << "} // namespace rpc::rpc\n\n";
        
        return oss.str();
    }
    
    std::string generate_service(const ServiceDef& s) {
        std::ostringstream oss;
        
        // 服务接口
        oss << "class " << s.name << "Service {\n";
        oss << "public:\n";
        oss << "    virtual ~" << s.name << "Service() = default;\n\n";
        
        for (const auto& m : s.methods) {
            oss << "    virtual " << to_cpp_type(m.return_type.type, m.return_type.type_name);
            oss << " " << m.name << "(";
            for (size_t i = 0; i < m.params.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << to_cpp_type(m.params[i].type, m.params[i].type_name);
                oss << " " << m.params[i].name;
            }
            oss << ") = 0;\n";
        }
        oss << "};\n\n";
        
        // 生成客户端代理
        if (config_.generate_client) {
            oss << "class " << s.name << "Client {\n";
            oss << "public:\n";
            oss << "    explicit " << s.name << "Client(RpcClient& client)\n";
            oss << "        : client_(client) {}\n\n";
            
            for (const auto& m : s.methods) {
                oss << "    coroutine::Task<" << to_cpp_type(m.return_type.type, m.return_type.type_name);
                oss << "> " << m.name << "(";
                for (size_t i = 0; i < m.params.size(); ++i) {
                    if (i > 0) oss << ", ";
                    oss << to_cpp_type(m.params[i].type, m.params[i].type_name);
                    oss << " " << m.params[i].name;
                }
                oss << ") {\n";
                
                // 构建请求
                if (!m.params.empty()) {
                    oss << "        " << m.name << "Request req;\n";
                    for (const auto& p : m.params) {
                        oss << "        req." << p.name << " = " << p.name << ";\n";
                    }
                    oss << "        auto resp = co_await client_.call<"
                        << m.name << "Response>(\"" << s.name << "\", \"" << m.name 
                        << "\", req);\n";
                } else {
                    oss << "        auto resp = co_await client_.call<"
                        << m.name << "Response>(\"" << s.name << "\", \"" << m.name << "\");\n";
                }
                oss << "        co_return resp;\n";
                oss << "    }\n\n";
            }
            
            oss << "private:\n";
            oss << "    RpcClient& client_;\n";
            oss << "};\n\n";
        }
        
        return oss.str();
    }
};

} // namespace idl
} // namespace rpc
