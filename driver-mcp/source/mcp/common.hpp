#pragma once

#include <functional>
#include <nlohmann/json.hpp>
#include <string>

namespace mcp_runtime {
    using json = nlohmann::json;
    using tool_function = std::function<json( const json& )>;

    struct tool_definition {
        std::string name;
        std::string description;
        json input_schema;
        bool read_only_hint;
        tool_function function;
    };

    json make_tool_result( const std::string& text , const json& structured_content = json::object( ) , bool is_error = false );

} // namespace mcp_runtime
