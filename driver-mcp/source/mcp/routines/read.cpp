#include "read.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

#include "../../driver/driver.hpp"
#include "type_dispatch.hpp"

namespace {
    template<typename read_function_type>
    mcp_runtime::json dispatch_read( const std::string& type_name , uintptr_t address , read_function_type&& read_function ) {
        const auto parsed_type = mcp_runtime::parse_primitive_type( type_name );
        if ( !parsed_type.has_value( ) ) {
            throw std::runtime_error( "bad_type" );
        }

        switch ( parsed_type.value( ) ) {
            case mcp_runtime::primitive_type::byte: return read_function.template operator() < BYTE > ( address );
            case mcp_runtime::primitive_type::u8: return read_function.template operator() < uint8_t > ( address );
            case mcp_runtime::primitive_type::u16: return read_function.template operator() < uint16_t > ( address );
            case mcp_runtime::primitive_type::u32: return read_function.template operator() < uint32_t > ( address );
            case mcp_runtime::primitive_type::u64: return read_function.template operator() < uint64_t > ( address );
            case mcp_runtime::primitive_type::i8: return read_function.template operator() < int8_t > ( address );
            case mcp_runtime::primitive_type::i16: return read_function.template operator() < int16_t > ( address );
            case mcp_runtime::primitive_type::i32: return read_function.template operator() < int32_t > ( address );
            case mcp_runtime::primitive_type::i64: return read_function.template operator() < int64_t > ( address );
            case mcp_runtime::primitive_type::f32: return read_function.template operator() < float > ( address );
            case mcp_runtime::primitive_type::f64: return read_function.template operator() < double > ( address );
            default: throw std::runtime_error( "bad_type" );
        }
    }
} // namespace

namespace mcp_runtime {
    json tool_read( const json& args ) {
        const auto address = args.at( "addr" ).get<uintptr_t>( );
        const auto type_name = args.at( "type" ).get<std::string>( );

        const auto value = dispatch_read( type_name , address , [ & ]<typename value_type>( uintptr_t input_address ) {
            return driver::km->read<value_type>( input_address );
        } );

        return make_tool_result( "read_success" , { { "value" , value } } );
    }
} // namespace mcp_runtime
