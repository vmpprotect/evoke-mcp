#pragma once

#include <chrono>
#include <cstdio>
#include <string>

#include <Windows.h>
#include <tlhelp32.h>
#include <winternl.h>

#include "communications.hpp"

namespace driver
{
    class c_driver
    {
    public:
        using nt_device_io_control_file_t = NTSTATUS ( NTAPI* )(
            HANDLE ,
            HANDLE ,
            PIO_APC_ROUTINE ,
            PVOID ,
            PIO_STATUS_BLOCK ,
            ULONG ,
            PVOID ,
            ULONG ,
            PVOID ,
            ULONG );

        HANDLE m_handle = INVALID_HANDLE_VALUE;
        bool m_kernel_available = false;
        u32 m_pid { 0 };
        u64 m_base_addr { 0 };
        u64 m_dtb{ 0 };
        u64 m_peb { 0 };
        nt_device_io_control_file_t m_nt_device_io_control_file = nullptr;

        static bool nt_success( NTSTATUS status ) {
            return status >= 0;
        }

        bool ioctl_native( u32 code , void* in_buf , u32 in_size , void* out_buf , u32 out_size , DWORD* bytes = nullptr ) {
            if ( !m_nt_device_io_control_file ) {
                DWORD ret = 0;
                BOOL ok = DeviceIoControl( m_handle , code , in_buf , in_size , out_buf , out_size , &ret , nullptr );
                if ( bytes ) *bytes = ret;
                return !!ok;
            }

            IO_STATUS_BLOCK io_status {};
            HANDLE complete_event = CreateEventW( nullptr , TRUE , FALSE , nullptr );
            if ( !complete_event ) {
                DWORD ret = 0;
                BOOL ok = DeviceIoControl( m_handle , code , in_buf , in_size , out_buf , out_size , &ret , nullptr );
                if ( bytes ) *bytes = ret;
                return !!ok;
            }

            NTSTATUS status = m_nt_device_io_control_file(
                m_handle ,
                complete_event ,
                nullptr ,
                nullptr ,
                &io_status ,
                code ,
                in_buf ,
                in_size ,
                out_buf ,
                out_size );

            if ( status == STATUS_PENDING ) {
                WaitForSingleObject( complete_event , INFINITE );
                status = ( NTSTATUS ) io_status.Status;
            }

            CloseHandle( complete_event );

            if ( bytes ) *bytes = ( DWORD ) io_status.Information;
            return nt_success( status );
        }

        bool init( ) {
            m_handle = CreateFileW(
                L"\\\\.\\evoke" ,
                GENERIC_READ | GENERIC_WRITE ,
                FILE_SHARE_READ | FILE_SHARE_WRITE ,
                nullptr ,
                OPEN_EXISTING ,
                FILE_ATTRIBUTE_NORMAL ,
                nullptr
            );

            m_kernel_available = ( m_handle != INVALID_HANDLE_VALUE );
            if ( !m_kernel_available ) {
                // Driver unavailable: kernel-only operations will fail.
                m_nt_device_io_control_file = nullptr;
                return true;
            }

            HMODULE ntdll = GetModuleHandleW( L"ntdll.dll" );
            if ( !ntdll )
                ntdll = LoadLibraryW( L"ntdll.dll" );

            if ( ntdll )
                m_nt_device_io_control_file = ( nt_device_io_control_file_t ) GetProcAddress( ntdll , "NtDeviceIoControlFile" );

            return true;
        }

        void attach( const wchar_t* procname ) {
            HANDLE h_snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS , 0 );
            if ( h_snapshot == INVALID_HANDLE_VALUE )
                return;

            PROCESSENTRY32W pe {};
            pe.dwSize = sizeof( pe );

            if ( Process32FirstW( h_snapshot , &pe ) )
            {
                do
                {
                    // case-insensitive wide compare
                    if ( _wcsicmp( procname , pe.szExeFile ) == 0 )
                    {
                        m_pid = pe.th32ProcessID;
                        break;
                    }
                } while ( Process32NextW( h_snapshot , &pe ) );
            }

            CloseHandle( h_snapshot );

            printf( "proc : %llx\n" , this->m_pid );
            if ( !this->m_pid ) return;

            get_base( this->m_base_addr );
            printf( "base address : %llx\n" , this->m_base_addr );
            if ( !this->m_base_addr ) return;

            get_dtb( this->m_dtb );
            printf( "dtb : %llx\n" , this->m_dtb );
            if ( !this->m_dtb ) return;
        }

        void fortnite( ) {
			attach( L"FortniteClient-Win64-Shipping.exe" );
        }

        bool attach_kernel( u32 pid ) {
            ioctl::attach_request req {};
            req.m_pid = pid;

            DWORD ret = 0;
            BOOL ok = ioctl_native( ioctl::attach , &req , sizeof( req ) , &req , sizeof( req ) , &ret );
            if ( !ok )
                return false;

            m_pid = pid;
            m_base_addr = req.m_base;
            m_dtb = req.m_dtb;
            return req.m_dtb != 0;
        }

        bool detach_kernel( u32 pid = 0 ) {
            if ( !m_kernel_available ) {
                m_base_addr = 0;
                m_dtb = 0;
                return true;
            }

            ioctl::detach_request req {};
            req.m_pid = pid ? pid : m_pid;

            DWORD ret = 0;
            BOOL ok = ioctl_native( ioctl::detach , &req , sizeof( req ) , nullptr , 0 , &ret );
            if ( ok && req.m_pid == m_pid ) {
                m_base_addr = 0;
                m_dtb = 0;
            }

            return !!ok;
        }

        bool resolve( u32 type , u64 argument , u64& out_value , u32 pid = 0 ) {
            if ( !m_kernel_available ) {
                out_value = 0;

                if ( type == ioctl::resolve_base ) {
                    u64 base = 0;
                    bool ok = get_base( base );
                    out_value = base;
                    return ok;
                }

                // DTB or other resolve kinds are kernel-only.
                return false;
            }

            ioctl::resolve_request req {};
            req.m_pid = pid ? pid : m_pid;
            req.m_type = type;
            req.m_argument = argument;

            DWORD ret = 0;
            BOOL ok = ioctl_native( ioctl::resolve , &req , sizeof( req ) , &req , sizeof( req ) , &ret );
            out_value = req.m_value;
            return ok && out_value;
        }

        bool get_base( u64& out_base ) {
            if ( m_kernel_available ) {
                ioctl::base_request req {};
                req.m_pid = m_pid;

                DWORD ret = 0;
                BOOL ok = ioctl_native( ioctl::get_base , &req , sizeof( req ) , &req , sizeof( req ) , &ret );

                out_base = req.m_base;
                return ok && out_base;
            }

            HANDLE mod_snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32 , m_pid );
            if ( mod_snapshot == INVALID_HANDLE_VALUE ) {
                out_base = 0;
                return false;
            }

            MODULEENTRY32W me {};
            me.dwSize = sizeof( me );

            bool ok = !!Module32FirstW( mod_snapshot , &me );
            out_base = ok ? ( u64 ) me.modBaseAddr : 0;
            CloseHandle( mod_snapshot );
            return ok && out_base;
        }

        bool get_peb( u64& out_peb ) {
            if ( !m_kernel_available ) {
                out_peb = 0;
                return false;
            }

            ioctl::peb_request req {};
            req.m_pid = m_pid;

            DWORD ret = 0;
            BOOL ok = ioctl_native( ioctl::get_peb , &req , sizeof( req ) , &req , sizeof( req ) , &ret );

            out_peb = req.m_peb;
            return ok && out_peb;
        }

        bool get_dtb( u64& out_dtb ) {
            if ( !m_kernel_available ) {
                out_dtb = 0;
                return false;
            }

            ioctl::pml4::dtb_invoke req {};
            req.pid = m_pid;

            DWORD ret = 0;
            BOOL ok = ioctl_native( ioctl::get_dtb , &req , sizeof( req ) , &req , sizeof( req ) , &ret );

            out_dtb = req.dtb;
            return ok && out_dtb;
        }

        bool read( u64 address , void* buffer , u64 size ) {
            if ( !m_kernel_available )
                return false;

            ioctl::read_request req {};
            req.m_pid = m_pid;
            req.m_address = address;
            req.m_buffer = ( u64 ) buffer;
            req.m_size = size;

            DWORD ret = 0;
            return ioctl_native( ioctl::read_memory , &req , sizeof( req ) , nullptr , 0 , &ret );
        }

        bool write( u64 address , const void* buffer , u64 size ) {
            if ( !m_kernel_available )
                return false;

            ioctl::write_request req {};
            req.m_pid = m_pid;
            req.m_address = address;
            req.m_buffer = ( u64 ) buffer;
            req.m_size = size;

            DWORD ret = 0;
            return ioctl_native( ioctl::write_memory , &req , sizeof( req ) , nullptr , 0 , &ret );
        }

        bool allocate_virtual( u64& inout_address , u64& inout_size , u32 alloc_type = MEM_COMMIT | MEM_RESERVE , u32 protection = PAGE_READWRITE , u32 pid = 0 ) {
            if ( !m_kernel_available )
                return false;

            ioctl::allocate_virtual_request req {};
            req.m_pid = pid ? pid : m_pid;
            req.m_address = inout_address;
            req.m_size = inout_size;
            req.m_alloc_type = alloc_type;
            req.m_protection = protection;

            DWORD ret = 0;
            BOOL ok = ioctl_native( ioctl::allocate_virtual , &req , sizeof( req ) , &req , sizeof( req ) , &ret );
            inout_address = req.m_address;
            inout_size = req.m_size;
            return !!ok && inout_address != 0;
        }

        bool protect_virtual( u64& inout_address , u64& inout_size , u32 protection , u32& out_old_protection , u32 pid = 0 ) {
            if ( !m_kernel_available )
                return false;

            ioctl::protect_virtual_request req {};
            req.m_pid = pid ? pid : m_pid;
            req.m_address = inout_address;
            req.m_size = inout_size;
            req.m_protection = protection;
            req.m_old_protection = 0;

            DWORD ret = 0;
            BOOL ok = ioctl_native( ioctl::protect_virtual , &req , sizeof( req ) , &req , sizeof( req ) , &ret );

            inout_address = req.m_address;
            inout_size = req.m_size;
            out_old_protection = req.m_old_protection;
            return !!ok;
        }

        bool unload( ) {
            if ( !m_kernel_available )
                return false;
            DWORD ret = 0;
            return ioctl_native( ioctl::unload_driver , nullptr , 0 , nullptr , 0 , &ret );
        }

        bool set_affinity( HWND hwnd , DWORD affinity ) {
            if ( !m_kernel_available )
                return false;

            ioctl::protect_window_request req {};
            req.m_window_handle = ( u64 ) hwnd;
            req.m_value = ( u32 ) affinity;

            DWORD ret = 0;
            return ioctl_native( ioctl::protect_window , &req , sizeof( req ) , nullptr , 0 , &ret );
        }

        bool call_eac( const char* ida_signature , const u64* args , u32 argc , u64& out_ret , u64* out_target = nullptr ) {
            out_ret = 0;
            if ( out_target )
                *out_target = 0;

            if ( !m_kernel_available )
                return false;

            if ( !ida_signature || argc > ioctl::eac_max_args )
                return false;

            ioctl::eac_call_request req {};
            size_t i = 0;
            for ( ; i + 1 < ioctl::eac_max_sig_len && ida_signature[ i ]; ++i )
                req.m_signature[ i ] = ida_signature[ i ];
            req.m_signature[ i ] = '\0';
            req.m_argc = argc;

            for ( u32 j = 0; j < argc; ++j )
                req.m_args[ j ] = args ? args[ j ] : 0;

            DWORD ret = 0;
            BOOL ok = ioctl_native( ioctl::eac_call , &req , sizeof( req ) , &req , sizeof( req ) , &ret );
            out_ret = req.m_return;
            if ( out_target )
                *out_target = req.m_target;

            return !!ok;
        }

        bool scan_eac( const char* ida_signature , u64& out_target ) {
            u64 out_ret = 0;
            out_target = 0;

            bool ok = call_eac( ida_signature , nullptr , 0 , out_ret , &out_target );
            return ok && out_target != 0;
        }

        bool call_eac( const char* ida_signature , u64& out_ret ) {
            return call_eac( ida_signature , nullptr , 0 , out_ret , nullptr );
        }

        template<typename... Args>
        bool call_eac( const char* ida_signature , u64& out_ret , Args... args ) {
            u64 packed[] = { ( u64 ) args... };
            return call_eac( ida_signature , packed , ( u32 ) sizeof...( args ) , out_ret , nullptr );
        }

        template<typename T>
        bool read( u64 address , T& out ) {
            return read( address , &out , sizeof( T ) );
        }

        template<typename T>
        T read( u64 address ) {
            T out {};
            read( address , out );
            return out;
        }

        template<typename T>
        bool write( u64 address , const T& value ) {
            return write( address , ( void* ) &value , sizeof( T ) );
        }
    }; // class c_driver

    inline c_driver* km = new c_driver( );

    namespace helpers {
        inline bool test_read_speed( ) {
            if ( !km->m_base_addr )
                return false;

            constexpr u64 test_range = 0x1000;
            constexpr u64 test_duration_ms = 1000; // 1 second

            printf( ( "Testing read speed over 0x%llx bytes for %llu ms\n" ) , test_range , test_duration_ms );

            u64 read_count = 0;

            auto start = std::chrono::high_resolution_clock::now( );

            while ( true ) {
                for ( u64 i = 0; i < test_range; i++ ) {
                    //km->read< std::uintptr_t >( km->m_base_addr + i );
					km->read( km->m_base_addr + i , &i , sizeof( i ) );
                    ++read_count;
                }

                auto now = std::chrono::high_resolution_clock::now( );
                auto elapsed = std::chrono::duration_cast< std::chrono::milliseconds >( now - start ).count( );

                if ( elapsed >= test_duration_ms )
                    break;
            }

            auto total_time = std::chrono::duration_cast< std::chrono::duration< double > >( std::chrono::high_resolution_clock::now( ) - start ).count( );

            double reads_per_sec = read_count / total_time;

            printf( ( "Read benchmark complete: %llu reads in %.3f sec (%.0f reads/sec)\n" ) , read_count , total_time , reads_per_sec );

            return true;
        }

        inline std::string read_string( u64 address , size_t max_len = 256 ) {
            std::string out {};
            out.reserve( max_len );

            char c = 0;
            for ( size_t i = 0; i < max_len; ++i ) {
                if ( !km->read( address + i , c ) || c == '\0' )
                    break;
                out.push_back( c );
            }

            return out;
        }

        inline std::wstring read_w_string( u64 address , size_t max_len = 256 ) {
            std::wstring out {};
            out.reserve( max_len );

            wchar_t c = 0;
            for ( size_t i = 0; i < max_len; ++i ) {
                if ( !km->read( address + ( i * sizeof( wchar_t ) ) , c ) || c == L'\0' )
                    break;
                out.push_back( c );
            }

            return out;
        }

        inline bool write_string( u64 address , const std::string& str , bool null_term = true ) {
            if ( str.empty( ) )
                return false;

            if ( !km->write( address , str.data( ) , str.size( ) ) )
                return false;

            if ( null_term ) {
                char zero = '\0';
                return km->write( address + str.size( ) , zero );
            }

            return true;
        }

        inline bool write_w_string( u64 address , const std::wstring& str , bool null_term = true ) {
            if ( str.empty( ) )
                return false;

            if ( !km->write( address , str.data( ) , str.size( ) * sizeof( wchar_t ) ) )
                return false;

            if ( null_term ) {
                wchar_t zero = L'\0';
                return km->write( address + ( str.size( ) * sizeof( wchar_t ) ) , zero );
            }

            return true;
        }

        template<typename T>
        inline bool read_array( u64 address , T* out , size_t count ) {
            return km->read( address , out , sizeof( T ) * count );
        }

        template<typename T>
        inline std::vector<T> read_vector( u64 address , size_t count ) {
            std::vector<T> out( count );
            read_array( address , out.data( ) , count );
            return out;
        }

        inline bool protect_window( HWND hwnd , DWORD affinity ) {
            return km->set_affinity( hwnd , affinity );
        }
    } // namespace helpers
} // namespace driver

