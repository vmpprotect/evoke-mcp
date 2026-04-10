#include "server.hpp"

#include <exception>

#include "routines/get_base.hpp"
#include "routines/read.hpp"
#include "routines/write.hpp"

namespace mcp_runtime {
    namespace {
        json make_jsonrpc_result( const json& id , const json& result ) {
            return {
                { "jsonrpc" , "2.0" } ,
                { "id" , id } ,
                { "result" , result }
            };
        }

        json make_jsonrpc_error( const json& id , int code , const char* message ) {
            return {
                { "jsonrpc" , "2.0" } ,
                { "id" , id } ,
                { "error" , { { "code" , code } , { "message" , message } } }
            };
        }
    } // namespace

    json make_tool_result( const std::string& text , const json& structured_content , bool is_error ) {
        json result = {
            { "content" , json::array( { { { "type" , "text" } , { "text" , text } } } ) } ,
            { "isError" , is_error }
        };

        if ( !structured_content.is_null( ) && !structured_content.empty( ) ) {
            result[ "structuredContent" ] = structured_content;
        }

        return result;
    }

    void mcp_server::init( ) {
        const json type_schema = {
            { "type" , "string" } ,
            { "enum" , json::array( { "byte" , "u8" , "u16" , "u32" , "u64" , "i8" , "i16" , "i32" , "i64" , "float" , "double" } ) }
        };

        tools = {
            {
                "get_base_address" ,
                "get_the_attached_process_image_base_address." ,
                { { "type" , "object" } , { "properties" , json::object( ) } , { "additionalProperties" , false } } ,
                true ,
                tool_get_base
            } ,
            {
                "read_memory" ,
                "read_a_primitive_value_from_process_memory_at_an_address." ,
                {
                    { "type" , "object" } ,
                    { "properties" ,
                        {
                            { "addr" , { { "type" , "integer" } , { "minimum" , 0 } , { "description" , "target_virtual_address." } } } ,
                            { "type" , type_schema }
                        } } ,
                    { "required" , json::array( { "addr" , "type" } ) } ,
                    { "additionalProperties" , false }
                } ,
                true ,
                tool_read
            } ,
            {
                "write_memory" ,
                "write_a_primitive_value_to_process_memory_at_an_address." ,
                {
                    { "type" , "object" } ,
                    { "properties" ,
                        {
                            { "addr" , { { "type" , "integer" } , { "minimum" , 0 } , { "description" , "target_virtual_address." } } } ,
                            { "type" , type_schema } ,
                            { "value" , { { "description" , "value_to_write_must_match_selected_type." } } }
                        } } ,
                    { "required" , json::array( { "addr" , "type" , "value" } ) } ,
                    { "additionalProperties" , false }
                } ,
                false ,
                tool_write
            }
        };

        for ( auto& tool : tools ) {
            tools_by_name[ tool.name ] = tool;
        }
    }

    std::optional<json> mcp_server::handle( const json& message ) {
        const bool has_id = message.contains( "id" );
        const json id = has_id ? message.at( "id" ) : json( nullptr );
        const auto method = message.value( "method" , "" );
        const auto params = message.value( "params" , json::object( ) );

        if ( method == "notifications/initialized" ) {
            return std::nullopt;
        }

        if ( method == "ping" ) {
            if ( !has_id ) {
                return std::nullopt;
            }

            return std::make_optional( make_jsonrpc_result( id , json::object( ) ) );
        }

        if ( method == "initialize" ) {
            if ( !has_id ) {
                return std::nullopt;
            }

            constexpr const char* server_protocol = "2025-03-26";
            return std::make_optional( make_jsonrpc_result( id , {
                { "protocolVersion" , server_protocol } ,
                { "capabilities" , { { "tools" , json::object( ) } } } ,
                { "serverInfo" , { { "name" , "mem" } , { "version" , "1" } } }
                } ) );
        }

        if ( method == "tools/list" ) {
            if ( !has_id ) {
                return std::nullopt;
            }

            json tools_array = json::array( );
            for ( auto& tool : tools ) {
                tools_array.push_back( {
                    { "name" , tool.name } ,
                    { "description" , tool.description } ,
                    { "inputSchema" , tool.input_schema } ,
                    { "annotations" , { { "readOnlyHint" , tool.read_only_hint } } }
                    } );
            }

            return std::make_optional( make_jsonrpc_result( id , { { "tools" , tools_array } } ) );
        }

        if ( method == "tools/call" ) {
            if ( !has_id ) {
                return std::nullopt;
            }

            const auto tool_name = params.at( "name" ).get<std::string>( );
            const auto arguments = params.value( "arguments" , json::object( ) );
            const auto tool_iterator = tools_by_name.find( tool_name );

            if ( tool_iterator == tools_by_name.end( ) ) {
                return std::make_optional( make_jsonrpc_error( id , -32602 , "unknown_tool" ) );
            }

            try {
                const auto result = tool_iterator->second.function( arguments );
                return std::make_optional( make_jsonrpc_result( id , result ) );
            }
            catch ( const std::exception& exception ) {
                return std::make_optional( make_jsonrpc_result( id , make_tool_result( exception.what( ) , json::object( ) , true ) ) );
            }
        }

        if ( !has_id ) {
            return std::nullopt;
        }

        return std::make_optional( make_jsonrpc_error( id , -1 , "bad_method" ) );
    }
} // namespace mcp_runtime
