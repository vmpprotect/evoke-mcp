#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "common.hpp"

namespace mcp_runtime {
    class mcp_server {
    public:
        void init( );
        std::optional<json> handle( const json& message );

    private:
        std::vector<tool_definition> tools;
        std::unordered_map<std::string , tool_definition> tools_by_name;
    };
} // namespace mcp_runtime
