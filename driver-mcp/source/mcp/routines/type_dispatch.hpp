#pragma once

#include <array>
#include <optional>
#include <string_view>
#include <utility>

namespace mcp_runtime {
    enum class primitive_type {
        byte,
        u8,
        u16,
        u32,
        u64,
        i8,
        i16,
        i32,
        i64,
        f32,
        f64
    };

    inline std::optional<primitive_type> parse_primitive_type( std::string_view type_name ) {
        using type_pair = std::pair<std::string_view, primitive_type>;
        constexpr std::array<type_pair, 11> known_types = {
            type_pair{ "byte" , primitive_type::byte } ,
            type_pair{ "u8" , primitive_type::u8 } ,
            type_pair{ "u16" , primitive_type::u16 } ,
            type_pair{ "u32" , primitive_type::u32 } ,
            type_pair{ "u64" , primitive_type::u64 } ,
            type_pair{ "i8" , primitive_type::i8 } ,
            type_pair{ "i16" , primitive_type::i16 } ,
            type_pair{ "i32" , primitive_type::i32 } ,
            type_pair{ "i64" , primitive_type::i64 } ,
            type_pair{ "float" , primitive_type::f32 } ,
            type_pair{ "double" , primitive_type::f64 }
        };

        for ( const auto& [ name , mapped_type ] : known_types ) {
            if ( type_name == name ) {
                return mapped_type;
            }
        }

        return std::nullopt;
    }
} // namespace mcp_runtime
