/**
 * @file idl_parser.hpp
 * @brief IDL解析器
 * @author RPC Framework
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace rpc {
namespace idl {

/**
 * @brief 字段类型
 */
enum class FieldType {
    INT8, INT16, INT32, INT64,
    UINT8, UINT16, UINT32, UINT64,
    FLOAT, DOUBLE,
    STRING, BOOL,
    VECTOR, MAP, CUSTOM
};

/**
 * @brief 字段定义
 */
struct Field {
    std::string name;
    FieldType type;
    std::string type_name;  // 自定义类型名
    FieldType element_type = FieldType::INT32;  // vector元素类型
    int field_id = 0;
};

/**
 * @brief 结构体定义
 */
struct StructDef {
    std::string name;
    std::vector<Field> fields;
};

/**
 * @brief 方法参数
 */
struct MethodParam {
    std::string name;
    FieldType type;
    std::string type_name;
};

/**
 * @brief 方法定义
 */
struct MethodDef {
    std::string name;
    std::vector<MethodParam> params;
    MethodParam return_type;
};

/**
 * @brief 服务定义
 */
struct ServiceDef {
    std::string name;
    std::vector<MethodDef> methods;
};

/**
 * @brief IDL文档
 */
struct IDLDocument {
    std::string namespace_name;
    std::vector<StructDef> structs;
    std::vector<ServiceDef> services;
};

/**
 * @brief IDL解析器
 */
class IDLParser {
public:
    std::optional<IDLDocument> parse(const std::string& content) {
        IDLDocument doc;
        
        std::istringstream stream(content);
        std::string line;
        int line_num = 0;
        
        while (std::getline(stream, line)) {
            ++line_num;
            line = trim(line);
            
            if (line.empty() || line[0] == '#') continue;
            
            if (line.starts_with("namespace")) {
                doc.namespace_name = parse_namespace(line);
            } else if (line.starts_with("struct")) {
                auto s = parse_struct(stream, line);
                if (s) doc.structs.push_back(*s);
            } else if (line.starts_with("service")) {
                auto s = parse_service(stream, line);
                if (s) doc.services.push_back(*s);
            }
        }
        
        return doc;
    }
    
private:
    static std::string trim(const std::string& s) {
        auto start = s.find_first_not_of(" \t\r\n");
        auto end = s.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        return s.substr(start, end - start + 1);
    }
    
    static std::string parse_namespace(const std::string& line) {
        auto pos = line.find("namespace");
        return trim(line.substr(pos + 9));
    }
    
    static FieldType parse_type(const std::string& type_str) {
        if (type_str == "int8") return FieldType::INT8;
        if (type_str == "int16") return FieldType::INT16;
        if (type_str == "int32") return FieldType::INT32;
        if (type_str == "int64") return FieldType::INT64;
        if (type_str == "uint8") return FieldType::UINT8;
        if (type_str == "uint16") return FieldType::UINT16;
        if (type_str == "uint32") return FieldType::UINT32;
        if (type_str == "uint64") return FieldType::UINT64;
        if (type_str == "float") return FieldType::FLOAT;
        if (type_str == "double") return FieldType::DOUBLE;
        if (type_str == "string") return FieldType::STRING;
        if (type_str == "bool") return FieldType::BOOL;
        if (type_str.starts_with("vector")) return FieldType::VECTOR;
        if (type_str.starts_with("map")) return FieldType::MAP;
        return FieldType::CUSTOM;
    }
    
    static std::optional<StructDef> parse_struct(std::istringstream& stream, 
                                                  const std::string& first_line) {
        StructDef s;
        
        // 解析 struct Name {
        auto pos = first_line.find("struct");
        auto brace_pos = first_line.find('{');
        if (brace_pos == std::string::npos) return std::nullopt;
        
        s.name = trim(first_line.substr(pos + 6, brace_pos - pos - 6));
        
        std::string line;
        while (std::getline(stream, line)) {
            line = trim(line);
            if (line == "\"}\"") break;
            if (line.empty() || line[0] == '#') continue;
            
            // 解析字段: type name;
            auto space_pos = line.find(' ');
            if (space_pos == std::string::npos) continue;
            
            std::string type_str = trim(line.substr(0, space_pos));
            std::string name_str = trim(line.substr(space_pos));
            
            // 移除末尾分号
            if (!name_str.empty() && name_str.back() == ';') {
                name_str.pop_back();
            }
            
            Field field;
            field.name = name_str;
            field.type = parse_type(type_str);
            field.type_name = type_str;
            s.fields.push_back(field);
        }
        
        return s;
    }
    
    static std::optional<ServiceDef> parse_service(std::istringstream& stream,
                                                    const std::string& first_line) {
        ServiceDef s;
        
        auto pos = first_line.find("service");
        auto brace_pos = first_line.find('{');
        if (brace_pos == std::string::npos) return std::nullopt;
        
        s.name = trim(first_line.substr(pos + 7, brace_pos - pos - 7));
        
        std::string line;
        while (std::getline(stream, line)) {
            line = trim(line);
            if (line == "\"}\"") break;
            if (line.empty() || line[0] == '#') continue;
            
            // 解析方法: ReturnType method(Param1 p1, Param2 p2);
            auto paren_open = line.find('(');
            auto paren_close = line.find(')');
            if (paren_open == std::string::npos || paren_close == std::string::npos) continue;
            
            MethodDef method;
            
            // 解析返回类型和方法名
            std::string before_paren = trim(line.substr(0, paren_open));
            auto last_space = before_paren.find_last_of(' ');
            if (last_space == std::string::npos) continue;
            
            method.return_type.type = parse_type(trim(before_paren.substr(0, last_space)));
            method.return_type.type_name = trim(before_paren.substr(0, last_space));
            method.name = trim(before_paren.substr(last_space));
            
            // 解析参数
            std::string params_str = line.substr(paren_open + 1, paren_close - paren_open - 1);
            if (!params_str.empty()) {
                std::istringstream param_stream(params_str);
                std::string param;
                while (std::getline(param_stream, param, ',')) {
                    param = trim(param);
                    auto space = param.find(' ');
                    if (space != std::string::npos) {
                        MethodParam p;
                        p.type = parse_type(trim(param.substr(0, space)));
                        p.type_name = trim(param.substr(0, space));
                        p.name = trim(param.substr(space));
                        method.params.push_back(p);
                    }
                }
            }
            
            s.methods.push_back(method);
        }
        
        return s;
    }
};

} // namespace idl
} // namespace rpc
