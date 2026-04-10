#include "get_base.hpp"

#include <sstream>

#include "../../driver/driver.hpp"

namespace mcp_runtime {
    json tool_get_base( const json& ) {
        const auto base_address = static_cast< uintptr_t >( driver::km->m_base_addr );
        std::ostringstream output_stream;
        output_stream << "base=0x" << std::hex << base_address;

        return make_tool_result( output_stream.str( ) , { { "base" , base_address } } );
    }
} // namespace mcp_runtime
