namespace mouse {
    #define MAX_MOUSE_SPEED 100
    #define COORD_UNSPECIFIED (-2147483647)
    #define MOUSE_COORD_TO_ABS(coord, width_or_height) \
        (((65536LL * (coord)) / (width_or_height)) + ((coord) < 0 ? -1 : 1))

    enum send_mode_t {
        SM_EVENT = 0 ,
        SM_INPUT = 1 ,
        SM_PLAY = 2
    };

    static send_mode_t s_send_mode = SM_EVENT;

    static POINT s_send_input_cursor_pos = { COORD_UNSPECIFIED, COORD_UNSPECIFIED };

    static inline void do_mouse_delay( ) {
        if ( s_send_mode != SM_INPUT )
            Sleep( 1 );
    }

    static void do_incremental_mouse_move( LONG sx , LONG sy , LONG ex , LONG ey , int speed ) {
        int steps = max( 1 , speed * 2 );

        for ( int i = 1; i <= steps; ++i ) {
            float t = ( float ) i / ( float ) steps;

            LONG cx = sx + LONG( ( ex - sx ) * t );
            LONG cy = sy + LONG( ( ey - sy ) * t );

            mouse_event( MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE , cx , cy , 0 , 0 );

            do_mouse_delay( );
        }
    }

    static void mouse_event_wrapper( DWORD flags , DWORD data , DWORD x , DWORD y ) {
        mouse_event( flags , x == COORD_UNSPECIFIED ? 0 : x , y == COORD_UNSPECIFIED ? 0 : y , data , 0 );
    }

    void mouse_move( int& aX , int& aY , DWORD& aEventFlags , int aSpeed , bool aMoveOffset ) {
        if ( aX == COORD_UNSPECIFIED || aY == COORD_UNSPECIFIED )
            return;

        if ( s_send_mode == SM_PLAY ) {
            mouse_event_wrapper( MOUSEEVENTF_MOVE , 0 , aX , aY );

            do_mouse_delay( );

            if ( aMoveOffset ) {
                aX = COORD_UNSPECIFIED;
                aY = COORD_UNSPECIFIED;
            }
            return;
        }

        aEventFlags |= MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;

        POINT cursor {};
        if ( aMoveOffset ) {
            if ( s_send_mode == SM_INPUT ) {
                if ( s_send_input_cursor_pos.x == COORD_UNSPECIFIED )
                    GetCursorPos( &s_send_input_cursor_pos );

                aX += s_send_input_cursor_pos.x;
                aY += s_send_input_cursor_pos.y;
            }
            else {
                GetCursorPos( &cursor );
                aX += cursor.x;
                aY += cursor.y;
            }
        }

        if ( s_send_mode == SM_INPUT ) {
            s_send_input_cursor_pos.x = aX;
            s_send_input_cursor_pos.y = aY;
        }

        int screen_w = GetSystemMetrics( SM_CXSCREEN );
        int screen_h = GetSystemMetrics( SM_CYSCREEN );

        aX = MOUSE_COORD_TO_ABS( aX , screen_w );
        aY = MOUSE_COORD_TO_ABS( aY , screen_h );

        if ( aSpeed < 0 )
            aSpeed = 0;
        else if ( aSpeed > MAX_MOUSE_SPEED )
            aSpeed = MAX_MOUSE_SPEED;

        if ( aSpeed == 0 || s_send_mode == SM_INPUT ) {
            mouse_event_wrapper( MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE , 0 , aX , aY );
            do_mouse_delay( );
            return;
        }

        GetCursorPos( &cursor );

        do_incremental_mouse_move( MOUSE_COORD_TO_ABS( cursor.x , screen_w ) , MOUSE_COORD_TO_ABS( cursor.y , screen_h ) , aX , aY , aSpeed );
    }

    void wait_for_5_and_move( ) {
        while ( true ) {
            if ( GetAsyncKeyState( '5' ) & 0x8000 )
                break;
            Sleep( 1 );
        }

        DWORD flags = 0;
        int x = 10;
        int y = 10;

        mouse_move( x , y , flags , 0 , false );
    }
}