#include "../httplib/httplib.h"

#include <array>
#include <iostream>
#include <string>

#include "../driver/driver.hpp"
#include "../mcp/server.hpp"

int main( ) {
    driver::km->init( );
    driver::km->attach( L"FortniteClient-Win64-Shipping.exe" );

    mcp_runtime::mcp_server server_core;
    server_core.init( );

    httplib::Server http_server;
    const auto origin_allowed = [ ]( const std::string& origin ) {
        if ( origin.empty( ) ) {
            return true;
        }

        constexpr std::array<const char*, 4> allowed_prefixes = {
            "http://127.0.0.1" ,
            "https://127.0.0.1" ,
            "http://localhost" ,
            "https://localhost"
        };

        for ( const auto* prefix : allowed_prefixes ) {
            if ( origin.rfind( prefix , 0 ) == 0 ) {
                return true;
            }
        }

        return false;
        };

    http_server.set_default_headers( {
        { "Access-Control-Allow-Origin" , "*" } ,
        { "Access-Control-Allow-Headers" , "content-type, mcp-session-id" } ,
        { "Access-Control-Allow-Methods" , "GET, POST, OPTIONS" }
        } );

    http_server.Get( "/mcp" , [ & ]( const httplib::Request& request , httplib::Response& response ) {
        const auto origin = request.get_header_value( "Origin" );
        if ( !origin_allowed( origin ) ) {
            response.status = 403;
            return;
        }

        const auto accept = request.get_header_value( "Accept" );
        if ( accept.find( "text/event-stream" ) != std::string::npos ) {
            response.status = 405;
            return;
        }

        const mcp_runtime::json body = {
            { "name" , "mem" } ,
            { "status" , "ok" } ,
            { "transport" , "streamable-http" } ,
            { "hint" , "send_json_rpc_post_requests_to_/mcp" }
        };
        response.set_content( body.dump( ) , "application/json" );
        } );

    http_server.Options( "/mcp" , [ & ]( const httplib::Request& request , httplib::Response& response ) {
        const auto origin = request.get_header_value( "Origin" );
        if ( !origin_allowed( origin ) ) {
            response.status = 403;
            return;
        }

        response.status = 204;
        } );

    http_server.Get( "/health" , [ ]( const httplib::Request& , httplib::Response& response ) {
        response.set_content( "ok" , "text/plain" );
        } );

    http_server.Post( "/mcp" , [ & ]( const httplib::Request& request , httplib::Response& response ) {
        try {
            const auto origin = request.get_header_value( "Origin" );
            if ( !origin_allowed( origin ) ) {
                response.status = 403;
                return;
            }

            const auto request_json = mcp_runtime::json::parse( request.body );
            const auto output = server_core.handle( request_json );
            if ( output.has_value( ) ) {
                response.set_content( output->dump( ) , "application/json" );
            }
            else {
                response.status = 202;
            }
        }
        catch ( ... ) {
            response.status = 400;
        }
        } );

    std::cout << "listening on port 8888\n";
    if ( !http_server.listen( "127.0.0.1" , 8888 ) ) {
        std::cerr << "cant bind to port, is port 8888 used?\n";
        return 1;
    }

    return 0;
}
