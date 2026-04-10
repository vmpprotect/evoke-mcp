#include "write.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

#include "../../driver/driver.hpp"
#include "type_dispatch.hpp"

namespace {
    template<typename write_function_type>
    void dispatch_write( const std::string& type_name , uintptr_t address , const mcp_runtime::json& value , write_function_type&& write_function ) {
        const auto parsed_type = mcp_runtime::parse_primitive_type( type_name );
        if ( !parsed_type.has_value( ) ) {
            throw std::runtime_error( "bad_type" );
        }

        switch ( parsed_type.value( ) ) {
            case mcp_runtime::primitive_type::byte: return write_function.template operator() < BYTE > ( address , value.get<BYTE>( ) );
            case mcp_runtime::primitive_type::u8: return write_function.template operator() < uint8_t > ( address , value.get<uint8_t>( ) );
            case mcp_runtime::primitive_type::u16: return write_function.template operator() < uint16_t > ( address , value.get<uint16_t>( ) );
            case mcp_runtime::primitive_type::u32: return write_function.template operator() < uint32_t > ( address , value.get<uint32_t>( ) );
            case mcp_runtime::primitive_type::u64: return write_function.template operator() < uint64_t > ( address , value.get<uint64_t>( ) );
            case mcp_runtime::primitive_type::i8: return write_function.template operator() < int8_t > ( address , value.get<int8_t>( ) );
            case mcp_runtime::primitive_type::i16: return write_function.template operator() < int16_t > ( address , value.get<int16_t>( ) );
            case mcp_runtime::primitive_type::i32: return write_function.template operator() < int32_t > ( address , value.get<int32_t>( ) );
            case mcp_runtime::primitive_type::i64: return write_function.template operator() < int64_t > ( address , value.get<int64_t>( ) );
            case mcp_runtime::primitive_type::f32: return write_function.template operator() < float > ( address , value.get<float>( ) );
            case mcp_runtime::primitive_type::f64: return write_function.template operator() < double > ( address , value.get<double>( ) );
            default: throw std::runtime_error( "bad_type" );
        }
    }
} // namespace

namespace mcp_runtime {
    json tool_write( const json& args ) {
        const auto address = args.at( "addr" ).get<uintptr_t>( );
        const auto type_name = args.at( "type" ).get<std::string>( );
        const auto value = args.at( "value" );

        dispatch_write( type_name , address , value , [ & ]<typename value_type>( uintptr_t input_address , value_type input_value ) {
            driver::km->write<value_type>( input_address , input_value );
        } );

        return make_tool_result( "write_success" , { { "ok" , true } } );
    }
} // namespace mcp_runtime
