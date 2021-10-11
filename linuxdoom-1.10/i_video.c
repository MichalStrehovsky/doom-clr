#ifdef _WIN32
#include "doomtype.h"

#include <inttypes.h>

#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

#include "doomdef.h"

uint8_t app_palette[256 * 3];
uint8_t* app_screen;
thread_atomic_int_t app_running;

event_t event_queue[ 256 ] = {0};
int events_count = 0;
thread_mutex_t event_mutex;

thread_signal_t vblank_signal;

typedef struct app_sound_data_t {
    tsf* sound_font;
} app_sound_data_t;

void sound_callback( APP_S16* sample_pairs, int sample_pairs_count, void* user_data ) {
    app_sound_data_t* app_sound_data = (app_sound_data_t*) user_data;
    if( sample_pairs_count == SAMPLECOUNT * 4 ) {
        I_UpdateSound();
        for( int i = 0; i < SAMPLECOUNT; ++i ) {
            for( int j = 0; j < 8; ++j ) {
                sample_pairs[ i * 8 + j ] = mixbuffer[ i * 2 + ( j % 2 ) ];
            }
        }
    }
    render_music( sample_pairs, sample_pairs_count, app_sound_data->sound_font );
}


static void load_crt_frame_col( void* data, struct GIF_WHDR* whdr ) {
    APP_U32* pixels = (APP_U32*) data;
    for( int i = 0; i < 1024 * 1024; ++i ) {
        uint8_t v = whdr->bptr[ i ];
        pixels[ i ] = ( whdr->cpal[ v ].B << 16 ) |( whdr->cpal[ v ].G << 8 ) | ( whdr->cpal[ v ].R );
    }
}

static void load_crt_frame_alpha( void* data, struct GIF_WHDR* whdr ) {
    APP_U32* pixels = (APP_U32*) data;
    for( int i = 0; i < 1024 * 1024; ++i ) {
        uint8_t v = whdr->bptr[ i ];
        pixels[ i ] = pixels[ i ] | ( whdr->cpal[ v ].R << 24);
    }
}

static APP_U32* load_crt_frame( void ) {
    APP_U32* pixels = (APP_U32*) malloc( 1024 * 1024 * sizeof( APP_U32 ) );
    memset( pixels, 0, 1024 * 1024 * sizeof( APP_U32 ) );
    GIF_Load( crtframecol, (long)sizeof( crtframecol ), load_crt_frame_col, NULL, (void*)pixels, 0L );
    GIF_Load( crtframealpha, (long)sizeof( crtframealpha), load_crt_frame_alpha, NULL, (void*)pixels, 0L );
    return pixels;
}

uint32_t gradient[ 256 ] = {
    0xff060406,0xff060408,0xff07040b,0xff07040e,0xff080412,0xff070416,0xff08041a,0xff07051a,0xff080618,0xff07061c,0xff0a071a,0xff0a071c,0xff0a081a,0xff0a0a17,0xff0a081d,0xff0b081e,0xff0c081f,0xff0a0822,0xff0a0a1f,0xff0a0825,0xff0a072b,0xff0a0828,0xff08082b,0xff07082d,0xff080733,0xff070735,0xff080833,0xff070835,0xff0a0836,0xff0a0a34,0xff0b0a35,0xff0a0b33,0xff0c0c2f,0xff0c0c30,0xff0c0c32,0xff0b0b37,0xff0a0a3d,0xff0b0b3a,0xff0c0b3b,0xff0a0846,0xff0a074b,0xff0a0849,0xff090b43,0xff0a0b44,0xff080a4a,0xff090b48,0xff0a0b49,0xff090c46,0xff0b0c47,0xff0c0b4c,0xff0b0a52,0xff0c0a53,0xff0a0859,0xff0a0a57,0xff0b0b54,0xff080a5a,0xff0a0b58,0xff0b0b59,0xff0a0c57,0xff0c0c57,0xff0c0c59,0xff0b0a62,0xff0b0b60,0xff0a0a66,0xff0a0a68,0xff0b0b65,0xff0b0c63,0xff0a0c65,0xff0c0d62,0xff0d0d63,0xff0c0e61,0xff0c105b,0xff0b0f61,0xff0a0e67,0xff0a0f64,0xff0a1062,0xff0a1260,0xff0c1065,0xff0b0f6b,0xff0c0f6c,0xff0b1069,0xff0c106b,0xff0a106d,0xff090f73,0xff081072,0xff0a1073,0xff0b1271,0xff0d136f,0xff0c1371,0xff0d1372,0xff10146f,0xff10156c,0xff10166a,0xff0f1570,0xff0c1476,0xff0f1572,0xff0f1573,0xff0b1672,0xff0d1576,0xff0c147c,0xff0b1579,0xff0d1676,0xff0e157b,0xff0e157c,0xff0e157d,0xff0e157e,0xff0f1777,0xff0f1778,0xff10167d,0xff0e167f,0xff0d177d,0xff0f1682,0xff0e177f,0xff0c1685,0xff0f1781,0xff0e187f,0xff0d1a7d,0xff0e1881,0xff101882,0xff101883,0xff121a80,0xff0e1a82,0xff101a83,0xff101889,0xff10188a,0xff0e1a88,0xff0f1a89,0xff0f1b86,0xff0f1b88,0xff0f1b89,0xff101c86,0xff111d83,0xff141d84,0xff121d86,0xff101c8c,0xff141c8d,0xff131d8b,0xff121d8e,0xff121d8f,0xff0f1d92,0xff0f1e91,0xff0f1e93,0xff0f1f92,0xff0c1f96,0xff0d2095,0xff0d2098,0xff0d209c,0xff0c209e,0xff0d219c,0xff0c219f,0xff0c21a1,0xff0e239f,0xff0d249d,0xff0c259c,0xff0d269b,0xff0d269d,0xff0d269f,0xff10279c,0xff0d26a3,0xff0c27a2,0xff0d27a3,0xff0f27a4,0xff1028a2,0xff0f28a4,0xff0b28a8,0xff0d28a9,0xff0f28aa,0xff0d2aa9,0xff102ba7,0xff0d2baa,0xff0d2ca8,0xff102cab,0xff102daa,0xff0e2ea8,0xff102fa5,0xff0c2eac,0xff0c2eae,0xff102fab,0xff0e30ad,0xff1131ab,0xff142fb1,0xff1230b4,0xff1230b8,0xff1330bb,0xff0f30c0,0xff1131be,0xff1232bf,0xff0f34bf,0xff1234c3,0xff1236c4,0xff1539c0,0xff1739c5,0xff153cc4,0xff103ec8,0xff1340c8,0xff1142cb,0xff1345cb,0xff1346ce,0xff1149d0,0xff114bd2,0xff134dd2,0xff1550d2,0xff1552d2,0xff1554d2,0xff1156d5,0xff1558d6,0xff1b59d9,0xff185bdd,0xff145edf,0xff1660e1,0xff1462e4,0xff1865e2,0xff1967e4,0xff136be4,0xff166de5,0xff1a6fe9,0xff1872ec,0xff1776ea,0xff1679ee,0xff197cef,0xff1c7ff1,0xff1a82f2,0xff1a86f3,0xff1c8af4,0xff1f8df6,0xff2590f6,0xff1e95f5,0xff2098f7,0xff2b99f8,0xff2a9cf9,0xff309ff9,0xff39a0fa,0xff31a5fa,0xff36a9fc,0xff37adfc,0xff36b1fc,0xff3fb3fc,0xff3bb8fc,0xff37bcfc,0xff37bffc,0xff3dc2fc,0xff42c4fc,0xff3cc9fc,0xff36cdfc,0xff3bd1fc,0xff35d5fc,0xff35dafc,0xff30defc,0xff25e4fc,0xff25e8fc,0xff1fedfc,0xff17f1fc,0xff1af4fc,0xff1af7fc,0xff18fafc,0xff1afbfc,
};

int counter = 0;
void app_proc( app_t* app, void* user_data )
{
  	app_screenmode( app, APP_SCREENMODE_FULLSCREEN );
	//app_screenmode( app, APP_SCREENMODE_WINDOW );
	//app_window_size( app, 800, 600 ); 
	app_interpolation( app, APP_INTERPOLATION_NONE );

    uint8_t* screen_buffer_xbgr = (uint8_t*)malloc( SCREENWIDTH * SCREENHEIGHT * 4 );

    frametimer_t* frametimer = frametimer_create( 0);
    frametimer_lock_rate( frametimer, 60 );
    
    crtemu_pc_t* crtemu_pc = crtemu_pc_create( 0 );
    APP_U32* frame = load_crt_frame();
    crtemu_pc_frame( crtemu_pc, frame, 1024, 1024 );
    crtemu_pc_gradient( crtemu_pc, gradient );
    free( frame );

    APP_U32 empty = 0;
    app_pointer( app, 1, 1, empty, 0, 0 );

    app_sound_data_t app_sound_data;
    app_sound_data.sound_font = tsf_load_memory( soundfont, sizeof( soundfont ) );
    app_sound( app, SAMPLECOUNT * 4 * 2, sound_callback, &app_sound_data );
    while( thread_atomic_int_load(&app_running) && app_yield( app ) != APP_STATE_EXIT_REQUESTED )
    {
        if( app_screen )
        {
            for( int i = 0; i < SCREENWIDTH * SCREENHEIGHT; ++i )
            {
                uint8_t scr = app_screen[ i ];
                screen_buffer_xbgr[ i * 4 + 3] = 0xff;
                screen_buffer_xbgr[ i * 4 ] = app_palette[ scr * 3 ];
                screen_buffer_xbgr[ i * 4 + 1] = app_palette[ scr * 3 + 1 ];
                screen_buffer_xbgr[ i * 4 + 2] = app_palette[ scr * 3 + 2 ];
            }
        }

        frametimer_update( frametimer );
        ++counter;
		thread_signal_raise( &vblank_signal );

        crtemu_pc_present( crtemu_pc, time, screen_buffer_xbgr, SCREENWIDTH, SCREENHEIGHT, 0xffffff, 0x000000 );
        app_present( app, 0, 0, 0, 0xffffffff, 0x00000000);

    }
    app_sound( app, 0, NULL, NULL );
    tsf_close( app_sound_data.sound_font );
    free( screen_buffer_xbgr );
}

void app_proc_thread()
{
    thread_mutex_init( &mus_mutex );
    app_run( app_proc, NULL, NULL, NULL, NULL );
    thread_mutex_term( &mus_mutex );
}

void I_InitGraphics (void)
{
    thread_mutex_init( &event_mutex );
	thread_signal_init( &vblank_signal );

    app_screen = (uint8_t*)malloc( SCREENWIDTH * SCREENHEIGHT );
    thread_atomic_int_store( &app_running, 1 );

    thread_create( app_proc_thread, 0, "", THREAD_STACK_SIZE_DEFAULT );
}

void I_ShutdownGraphics(void)
{
    thread_atomic_int_store( &app_running, 0 );
    
    free( app_screen );
    app_screen = 0;

    thread_signal_term( &vblank_signal );
}

// Takes full 8 bit values.
void I_SetPalette (byte* palette)
{
    int i;
    int c;
	// set the X colormap entries
	for (i=0 ; i<256 ; i++)
	{
	c = gammatable[usegamma][*palette++];
	app_palette[i*3+0] = (c<<8) + c;
	c = gammatable[usegamma][*palette++];
	app_palette[i*3+1] = (c<<8) + c;
	c = gammatable[usegamma][*palette++];
	app_palette[i*3+2] = (c<<8) + c;
	}
}

void I_UpdateNoBlit (void)
{
}

void I_FinishUpdate (void)
{
    if( app_screen )
    {
        memcpy( app_screen, screens[ 0 ], SCREENWIDTH*SCREENHEIGHT );
    }
}

//
// Called by D_DoomLoop,
// called before processing each tic in a frame.
// Quick syncronous operations are performed here.
// Can call D_PostEvent.
void I_StartTic (void)
{
    static int prev[ 256 ] = { 0 };
    int keys[ 256 ];
    for( int i = 0; i < 256; ++i ) keys[ i ] = ( GetAsyncKeyState( i ) & 0x8000 ) != 0;

    for( int i = 0; i < 255; ++i ) 
    {
        int key = 0;
        
        switch( i )
        {
            case VK_RSHIFT: key = KEY_RSHIFT; break;
            case VK_RCONTROL: key = KEY_RCTRL; break;
            case VK_MENU: key = KEY_RALT; break;
            case VK_BACK: key = KEY_BACKSPACE; break;
            case VK_PAUSE: key = KEY_PAUSE; break;
            case VK_TAB: key = KEY_TAB; break;
            case VK_F1: key = KEY_F1; break;
            case VK_F2: key = KEY_F2; break;
            case VK_F3: key = KEY_F3; break;
            case VK_F4: key = KEY_F4; break;
            case VK_F5: key = KEY_F5; break;
            case VK_F6: key = KEY_F6; break;
            case VK_F7: key = KEY_F7; break;
            case VK_F8: key = KEY_F8; break;
            case VK_F9: key = KEY_F9; break;
            case VK_F10: key = KEY_F10; break;
            case VK_F11: key = KEY_F11; break;
            case VK_F12: key = KEY_F12; break;
            case VK_UP: key = KEY_UPARROW; break;
            case VK_DOWN: key = KEY_DOWNARROW; break;
            case VK_LEFT: key = KEY_LEFTARROW; break;
            case VK_RIGHT: key = KEY_RIGHTARROW; break;
            case VK_ESCAPE: key = KEY_ESCAPE; break;
            case VK_SHIFT: key = KEY_RSHIFT; break;
            case VK_CONTROL: key = KEY_RCTRL; break;
            case VK_RMENU: key = KEY_RALT; break;
            case VK_LMENU: key = KEY_LALT; break;
            case VK_RETURN: key = KEY_ENTER; break;
		    case VK_SPACE: key = ' '; break;
		    case VK_OEM_PLUS: key = KEY_EQUALS; break;
		    case VK_OEM_MINUS: key = KEY_MINUS; break;
            default:
                key = tolower( i );
        }

        if( keys[ i ] && !prev[ i ] )
            {
            event_t ev;
            ev.type = ev_keydown;
            ev.data1 = key;
            D_PostEvent( &ev );
            }
        else if( !keys[ i ] && prev[ i ] )
            {       
            event_t ev;
            ev.type = ev_keyup;
            ev.data1 = key;
            D_PostEvent( &ev );
            }
        prev[ i ] = keys[ i ];
	}

    RECT r;
    GetClientRect( GetDesktopWindow(), &r );
    r.bottom -= r.top;
    r.right -= r.left;
    POINT pos;
    GetCursorPos( &pos );
    pos.x -= r.right / 2;
    pos.y -= r.bottom / 2;

    event_t ev;
	ev.type = ev_mouse;
    ev.data1 = 0;
	ev.data2 = pos.x << 2;
	ev.data3 = -pos.y << 2;
    D_PostEvent( &ev );
    SetCursorPos(r.right / 2,r.bottom / 2);
}

// Wait for vertical retrace or pause a bit.
void I_WaitVBL(int count)
{
    int c = counter;
    while ( counter - c < count )
	    thread_signal_wait( &vblank_signal, 1000 );
}

void I_ReadScreen (byte* scr)
{
    memcpy( scr, screens[0], SCREENWIDTH * SCREENHEIGHT );
}

void I_BeginRead (void)
{

}

void I_EndRead (void)
{

}


#else
// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	DOOM graphics stuff for X11, UNIX.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_x.c,v 1.6 1997/02/03 22:45:10 b1 Exp $";

#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <X11/extensions/XShm.h>
// Had to dig up XShm.c for this one.
// It is in the libXext, but not in the XFree86 headers.
#ifdef LINUX
int XShmGetEventBase( Display* dpy ); // problems with g++?
#endif

#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <errnos.h>
#include <signal.h>

#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

#include "doomdef.h"


#define POINTER_WARP_COUNTDOWN	1

Display*	X_display=0;
Window		X_mainWindow;
Colormap	X_cmap;
Visual*		X_visual;
GC		X_gc;
XEvent		X_event;
int		X_screen;
XVisualInfo	X_visualinfo;
XImage*		image;
int		X_width;
int		X_height;

// MIT SHared Memory extension.
boolean		doShm;

XShmSegmentInfo	X_shminfo;
int		X_shmeventtype;

// Fake mouse handling.
// This cannot work properly w/o DGA.
// Needs an invisible mouse cursor at least.
boolean		grabMouse;
int		doPointerWarp = POINTER_WARP_COUNTDOWN;

// Blocky mode,
// replace each 320x200 pixel with multiply*multiply pixels.
// According to Dave Taylor, it still is a bonehead thing
// to use ....
static int	multiply=1;


//
//  Translates the key currently in X_event
//

int xlatekey(void)
{

    int rc;

    switch(rc = XKeycodeToKeysym(X_display, X_event.xkey.keycode, 0))
    {
      case XK_Left:	rc = KEY_LEFTARROW;	break;
      case XK_Right:	rc = KEY_RIGHTARROW;	break;
      case XK_Down:	rc = KEY_DOWNARROW;	break;
      case XK_Up:	rc = KEY_UPARROW;	break;
      case XK_Escape:	rc = KEY_ESCAPE;	break;
      case XK_Return:	rc = KEY_ENTER;		break;
      case XK_Tab:	rc = KEY_TAB;		break;
      case XK_F1:	rc = KEY_F1;		break;
      case XK_F2:	rc = KEY_F2;		break;
      case XK_F3:	rc = KEY_F3;		break;
      case XK_F4:	rc = KEY_F4;		break;
      case XK_F5:	rc = KEY_F5;		break;
      case XK_F6:	rc = KEY_F6;		break;
      case XK_F7:	rc = KEY_F7;		break;
      case XK_F8:	rc = KEY_F8;		break;
      case XK_F9:	rc = KEY_F9;		break;
      case XK_F10:	rc = KEY_F10;		break;
      case XK_F11:	rc = KEY_F11;		break;
      case XK_F12:	rc = KEY_F12;		break;
	
      case XK_BackSpace:
      case XK_Delete:	rc = KEY_BACKSPACE;	break;

      case XK_Pause:	rc = KEY_PAUSE;		break;

      case XK_KP_Equal:
      case XK_equal:	rc = KEY_EQUALS;	break;

      case XK_KP_Subtract:
      case XK_minus:	rc = KEY_MINUS;		break;

      case XK_Shift_L:
      case XK_Shift_R:
	rc = KEY_RSHIFT;
	break;
	
      case XK_Control_L:
      case XK_Control_R:
	rc = KEY_RCTRL;
	break;
	
      case XK_Alt_L:
      case XK_Meta_L:
      case XK_Alt_R:
      case XK_Meta_R:
	rc = KEY_RALT;
	break;
	
      default:
	if (rc >= XK_space && rc <= XK_asciitilde)
	    rc = rc - XK_space + ' ';
	if (rc >= 'A' && rc <= 'Z')
	    rc = rc - 'A' + 'a';
	break;
    }

    return rc;

}

void I_ShutdownGraphics(void)
{
  // Detach from X server
  if (!XShmDetach(X_display, &X_shminfo))
	    I_Error("XShmDetach() failed in I_ShutdownGraphics()");

  // Release shared memory.
  shmdt(X_shminfo.shmaddr);
  shmctl(X_shminfo.shmid, IPC_RMID, 0);

  // Paranoia.
  image->data = NULL;
}



//
// I_StartFrame
//
void I_StartFrame (void)
{
    // er?

}

static int	lastmousex = 0;
static int	lastmousey = 0;
boolean		mousemoved = false;
boolean		shmFinished;

void I_GetEvent(void)
{

    event_t event;

    // put event-grabbing stuff in here
    XNextEvent(X_display, &X_event);
    switch (X_event.type)
    {
      case KeyPress:
	event.type = ev_keydown;
	event.data1 = xlatekey();
	D_PostEvent(&event);
	// fprintf(stderr, "k");
	break;
      case KeyRelease:
	event.type = ev_keyup;
	event.data1 = xlatekey();
	D_PostEvent(&event);
	// fprintf(stderr, "ku");
	break;
      case ButtonPress:
	event.type = ev_mouse;
	event.data1 =
	    (X_event.xbutton.state & Button1Mask)
	    | (X_event.xbutton.state & Button2Mask ? 2 : 0)
	    | (X_event.xbutton.state & Button3Mask ? 4 : 0)
	    | (X_event.xbutton.button == Button1)
	    | (X_event.xbutton.button == Button2 ? 2 : 0)
	    | (X_event.xbutton.button == Button3 ? 4 : 0);
	event.data2 = event.data3 = 0;
	D_PostEvent(&event);
	// fprintf(stderr, "b");
	break;
      case ButtonRelease:
	event.type = ev_mouse;
	event.data1 =
	    (X_event.xbutton.state & Button1Mask)
	    | (X_event.xbutton.state & Button2Mask ? 2 : 0)
	    | (X_event.xbutton.state & Button3Mask ? 4 : 0);
	// suggest parentheses around arithmetic in operand of |
	event.data1 =
	    event.data1
	    ^ (X_event.xbutton.button == Button1 ? 1 : 0)
	    ^ (X_event.xbutton.button == Button2 ? 2 : 0)
	    ^ (X_event.xbutton.button == Button3 ? 4 : 0);
	event.data2 = event.data3 = 0;
	D_PostEvent(&event);
	// fprintf(stderr, "bu");
	break;
      case MotionNotify:
	event.type = ev_mouse;
	event.data1 =
	    (X_event.xmotion.state & Button1Mask)
	    | (X_event.xmotion.state & Button2Mask ? 2 : 0)
	    | (X_event.xmotion.state & Button3Mask ? 4 : 0);
	event.data2 = (X_event.xmotion.x - lastmousex) << 2;
	event.data3 = (lastmousey - X_event.xmotion.y) << 2;

	if (event.data2 || event.data3)
	{
	    lastmousex = X_event.xmotion.x;
	    lastmousey = X_event.xmotion.y;
	    if (X_event.xmotion.x != X_width/2 &&
		X_event.xmotion.y != X_height/2)
	    {
		D_PostEvent(&event);
		// fprintf(stderr, "m");
		mousemoved = false;
	    } else
	    {
		mousemoved = true;
	    }
	}
	break;
	
      case Expose:
      case ConfigureNotify:
	break;
	
      default:
	if (doShm && X_event.type == X_shmeventtype) shmFinished = true;
	break;
    }

}

Cursor
createnullcursor
( Display*	display,
  Window	root )
{
    Pixmap cursormask;
    XGCValues xgc;
    GC gc;
    XColor dummycolour;
    Cursor cursor;

    cursormask = XCreatePixmap(display, root, 1, 1, 1/*depth*/);
    xgc.function = GXclear;
    gc =  XCreateGC(display, cursormask, GCFunction, &xgc);
    XFillRectangle(display, cursormask, gc, 0, 0, 1, 1);
    dummycolour.pixel = 0;
    dummycolour.red = 0;
    dummycolour.flags = 04;
    cursor = XCreatePixmapCursor(display, cursormask, cursormask,
				 &dummycolour,&dummycolour, 0,0);
    XFreePixmap(display,cursormask);
    XFreeGC(display,gc);
    return cursor;
}

//
// I_StartTic
//
void I_StartTic (void)
{

    if (!X_display)
	return;

    while (XPending(X_display))
	I_GetEvent();

    // Warp the pointer back to the middle of the window
    //  or it will wander off - that is, the game will
    //  loose input focus within X11.
    if (grabMouse)
    {
	if (!--doPointerWarp)
	{
	    XWarpPointer( X_display,
			  None,
			  X_mainWindow,
			  0, 0,
			  0, 0,
			  X_width/2, X_height/2);

	    doPointerWarp = POINTER_WARP_COUNTDOWN;
	}
    }

    mousemoved = false;

}


//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
    // what is this?
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{

    static int	lasttic;
    int		tics;
    int		i;
    // UNUSED static unsigned char *bigscreen=0;

    // draws little dots on the bottom of the screen
    if (devparm)
    {

	i = I_GetTime();
	tics = i - lasttic;
	lasttic = i;
	if (tics > 20) tics = 20;

	for (i=0 ; i<tics*2 ; i+=2)
	    screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0xff;
	for ( ; i<20*2 ; i+=2)
	    screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0x0;
    
    }

    // scales the screen size before blitting it
    if (multiply == 2)
    {
	unsigned int *olineptrs[2];
	unsigned int *ilineptr;
	int x, y, i;
	unsigned int twoopixels;
	unsigned int twomoreopixels;
	unsigned int fouripixels;

	ilineptr = (unsigned int *) (screens[0]);
	for (i=0 ; i<2 ; i++)
	    olineptrs[i] = (unsigned int *) &image->data[i*X_width];

	y = SCREENHEIGHT;
	while (y--)
	{
	    x = SCREENWIDTH;
	    do
	    {
		fouripixels = *ilineptr++;
		twoopixels =	(fouripixels & 0xff000000)
		    |	((fouripixels>>8) & 0xffff00)
		    |	((fouripixels>>16) & 0xff);
		twomoreopixels =	((fouripixels<<16) & 0xff000000)
		    |	((fouripixels<<8) & 0xffff00)
		    |	(fouripixels & 0xff);
#ifdef __BIG_ENDIAN__
		*olineptrs[0]++ = twoopixels;
		*olineptrs[1]++ = twoopixels;
		*olineptrs[0]++ = twomoreopixels;
		*olineptrs[1]++ = twomoreopixels;
#else
		*olineptrs[0]++ = twomoreopixels;
		*olineptrs[1]++ = twomoreopixels;
		*olineptrs[0]++ = twoopixels;
		*olineptrs[1]++ = twoopixels;
#endif
	    } while (x-=4);
	    olineptrs[0] += X_width/4;
	    olineptrs[1] += X_width/4;
	}

    }
    else if (multiply == 3)
    {
	unsigned int *olineptrs[3];
	unsigned int *ilineptr;
	int x, y, i;
	unsigned int fouropixels[3];
	unsigned int fouripixels;

	ilineptr = (unsigned int *) (screens[0]);
	for (i=0 ; i<3 ; i++)
	    olineptrs[i] = (unsigned int *) &image->data[i*X_width];

	y = SCREENHEIGHT;
	while (y--)
	{
	    x = SCREENWIDTH;
	    do
	    {
		fouripixels = *ilineptr++;
		fouropixels[0] = (fouripixels & 0xff000000)
		    |	((fouripixels>>8) & 0xff0000)
		    |	((fouripixels>>16) & 0xffff);
		fouropixels[1] = ((fouripixels<<8) & 0xff000000)
		    |	(fouripixels & 0xffff00)
		    |	((fouripixels>>8) & 0xff);
		fouropixels[2] = ((fouripixels<<16) & 0xffff0000)
		    |	((fouripixels<<8) & 0xff00)
		    |	(fouripixels & 0xff);
#ifdef __BIG_ENDIAN__
		*olineptrs[0]++ = fouropixels[0];
		*olineptrs[1]++ = fouropixels[0];
		*olineptrs[2]++ = fouropixels[0];
		*olineptrs[0]++ = fouropixels[1];
		*olineptrs[1]++ = fouropixels[1];
		*olineptrs[2]++ = fouropixels[1];
		*olineptrs[0]++ = fouropixels[2];
		*olineptrs[1]++ = fouropixels[2];
		*olineptrs[2]++ = fouropixels[2];
#else
		*olineptrs[0]++ = fouropixels[2];
		*olineptrs[1]++ = fouropixels[2];
		*olineptrs[2]++ = fouropixels[2];
		*olineptrs[0]++ = fouropixels[1];
		*olineptrs[1]++ = fouropixels[1];
		*olineptrs[2]++ = fouropixels[1];
		*olineptrs[0]++ = fouropixels[0];
		*olineptrs[1]++ = fouropixels[0];
		*olineptrs[2]++ = fouropixels[0];
#endif
	    } while (x-=4);
	    olineptrs[0] += 2*X_width/4;
	    olineptrs[1] += 2*X_width/4;
	    olineptrs[2] += 2*X_width/4;
	}

    }
    else if (multiply == 4)
    {
	// Broken. Gotta fix this some day.
	void Expand4(unsigned *, double *);
  	Expand4 ((unsigned *)(screens[0]), (double *) (image->data));
    }

    if (doShm)
    {

	if (!XShmPutImage(	X_display,
				X_mainWindow,
				X_gc,
				image,
				0, 0,
				0, 0,
				X_width, X_height,
				True ))
	    I_Error("XShmPutImage() failed\n");

	// wait for it to finish and processes all input events
	shmFinished = false;
	do
	{
	    I_GetEvent();
	} while (!shmFinished);

    }
    else
    {

	// draw the image
	XPutImage(	X_display,
			X_mainWindow,
			X_gc,
			image,
			0, 0,
			0, 0,
			X_width, X_height );

	// sync up with server
	XSync(X_display, False);

    }

}


//
// I_ReadScreen
//
void I_ReadScreen (byte* scr)
{
    memcpy (scr, screens[0], SCREENWIDTH*SCREENHEIGHT);
}


//
// Palette stuff.
//
static XColor	colors[256];

void UploadNewPalette(Colormap cmap, byte *palette)
{

    register int	i;
    register int	c;
    static boolean	firstcall = true;

#ifdef __cplusplus
    if (X_visualinfo.c_class == PseudoColor && X_visualinfo.depth == 8)
#else
    if (X_visualinfo.class == PseudoColor && X_visualinfo.depth == 8)
#endif
	{
	    // initialize the colormap
	    if (firstcall)
	    {
		firstcall = false;
		for (i=0 ; i<256 ; i++)
		{
		    colors[i].pixel = i;
		    colors[i].flags = DoRed|DoGreen|DoBlue;
		}
	    }

	    // set the X colormap entries
	    for (i=0 ; i<256 ; i++)
	    {
		c = gammatable[usegamma][*palette++];
		colors[i].red = (c<<8) + c;
		c = gammatable[usegamma][*palette++];
		colors[i].green = (c<<8) + c;
		c = gammatable[usegamma][*palette++];
		colors[i].blue = (c<<8) + c;
	    }

	    // store the colors to the current colormap
	    XStoreColors(X_display, cmap, colors, 256);

	}
}

//
// I_SetPalette
//
void I_SetPalette (byte* palette)
{
    UploadNewPalette(X_cmap, palette);
}


//
// This function is probably redundant,
//  if XShmDetach works properly.
// ddt never detached the XShm memory,
//  thus there might have been stale
//  handles accumulating.
//
void grabsharedmemory(int size)
{

  int			key = ('d'<<24) | ('o'<<16) | ('o'<<8) | 'm';
  struct shmid_ds	shminfo;
  int			minsize = 320*200;
  int			id;
  int			rc;
  // UNUSED int done=0;
  int			pollution=5;
  
  // try to use what was here before
  do
  {
    id = shmget((key_t) key, minsize, 0777); // just get the id
    if (id != -1)
    {
      rc=shmctl(id, IPC_STAT, &shminfo); // get stats on it
      if (!rc) 
      {
	if (shminfo.shm_nattch)
	{
	  fprintf(stderr, "User %d appears to be running "
		  "DOOM.  Is that wise?\n", shminfo.shm_cpid);
	  key++;
	}
	else
	{
	  if (getuid() == shminfo.shm_perm.cuid)
	  {
	    rc = shmctl(id, IPC_RMID, 0);
	    if (!rc)
	      fprintf(stderr,
		      "Was able to kill my old shared memory\n");
	    else
	      I_Error("Was NOT able to kill my old shared memory");
	    
	    id = shmget((key_t)key, size, IPC_CREAT|0777);
	    if (id==-1)
	      I_Error("Could not get shared memory");
	    
	    rc=shmctl(id, IPC_STAT, &shminfo);
	    
	    break;
	    
	  }
	  if (size >= shminfo.shm_segsz)
	  {
	    fprintf(stderr,
		    "will use %d's stale shared memory\n",
		    shminfo.shm_cpid);
	    break;
	  }
	  else
	  {
	    fprintf(stderr,
		    "warning: can't use stale "
		    "shared memory belonging to id %d, "
		    "key=0x%x\n",
		    shminfo.shm_cpid, key);
	    key++;
	  }
	}
      }
      else
      {
	I_Error("could not get stats on key=%d", key);
      }
    }
    else
    {
      id = shmget((key_t)key, size, IPC_CREAT|0777);
      if (id==-1)
      {
	extern int errno;
	fprintf(stderr, "errno=%d\n", errno);
	I_Error("Could not get any shared memory");
      }
      break;
    }
  } while (--pollution);
  
  if (!pollution)
  {
    I_Error("Sorry, system too polluted with stale "
	    "shared memory segments.\n");
    }	
  
  X_shminfo.shmid = id;
  
  // attach to the shared memory segment
  image->data = X_shminfo.shmaddr = shmat(id, 0, 0);
  
  fprintf(stderr, "shared memory id=%d, addr=0x%x\n", id,
	  (int) (image->data));
}

void I_InitGraphics(void)
{

    char*		displayname;
    char*		d;
    int			n;
    int			pnum;
    int			x=0;
    int			y=0;
    
    // warning: char format, different type arg
    char		xsign=' ';
    char		ysign=' ';
    
    int			oktodraw;
    unsigned long	attribmask;
    XSetWindowAttributes attribs;
    XGCValues		xgcvalues;
    int			valuemask;
    static int		firsttime=1;

    if (!firsttime)
	return;
    firsttime = 0;

    signal(SIGINT, (void (*)(int)) I_Quit);

    if (M_CheckParm("-2"))
	multiply = 2;

    if (M_CheckParm("-3"))
	multiply = 3;

    if (M_CheckParm("-4"))
	multiply = 4;

    X_width = SCREENWIDTH * multiply;
    X_height = SCREENHEIGHT * multiply;

    // check for command-line display name
    if ( (pnum=M_CheckParm("-disp")) ) // suggest parentheses around assignment
	displayname = myargv[pnum+1];
    else
	displayname = 0;

    // check if the user wants to grab the mouse (quite unnice)
    grabMouse = !!M_CheckParm("-grabmouse");

    // check for command-line geometry
    if ( (pnum=M_CheckParm("-geom")) ) // suggest parentheses around assignment
    {
	// warning: char format, different type arg 3,5
	n = sscanf(myargv[pnum+1], "%c%d%c%d", &xsign, &x, &ysign, &y);
	
	if (n==2)
	    x = y = 0;
	else if (n==6)
	{
	    if (xsign == '-')
		x = -x;
	    if (ysign == '-')
		y = -y;
	}
	else
	    I_Error("bad -geom parameter");
    }

    // open the display
    X_display = XOpenDisplay(displayname);
    if (!X_display)
    {
	if (displayname)
	    I_Error("Could not open display [%s]", displayname);
	else
	    I_Error("Could not open display (DISPLAY=[%s])", getenv("DISPLAY"));
    }

    // use the default visual 
    X_screen = DefaultScreen(X_display);
    if (!XMatchVisualInfo(X_display, X_screen, 8, PseudoColor, &X_visualinfo))
	I_Error("xdoom currently only supports 256-color PseudoColor screens");
    X_visual = X_visualinfo.visual;

    // check for the MITSHM extension
    doShm = XShmQueryExtension(X_display);

    // even if it's available, make sure it's a local connection
    if (doShm)
    {
	if (!displayname) displayname = (char *) getenv("DISPLAY");
	if (displayname)
	{
	    d = displayname;
	    while (*d && (*d != ':')) d++;
	    if (*d) *d = 0;
	    if (!(!strcasecmp(displayname, "unix") || !*displayname)) doShm = false;
	}
    }

    fprintf(stderr, "Using MITSHM extension\n");

    // create the colormap
    X_cmap = XCreateColormap(X_display, RootWindow(X_display,
						   X_screen), X_visual, AllocAll);

    // setup attributes for main window
    attribmask = CWEventMask | CWColormap | CWBorderPixel;
    attribs.event_mask =
	KeyPressMask
	| KeyReleaseMask
	// | PointerMotionMask | ButtonPressMask | ButtonReleaseMask
	| ExposureMask;

    attribs.colormap = X_cmap;
    attribs.border_pixel = 0;

    // create the main window
    X_mainWindow = XCreateWindow(	X_display,
					RootWindow(X_display, X_screen),
					x, y,
					X_width, X_height,
					0, // borderwidth
					8, // depth
					InputOutput,
					X_visual,
					attribmask,
					&attribs );

    XDefineCursor(X_display, X_mainWindow,
		  createnullcursor( X_display, X_mainWindow ) );

    // create the GC
    valuemask = GCGraphicsExposures;
    xgcvalues.graphics_exposures = False;
    X_gc = XCreateGC(	X_display,
  			X_mainWindow,
  			valuemask,
  			&xgcvalues );

    // map the window
    XMapWindow(X_display, X_mainWindow);

    // wait until it is OK to draw
    oktodraw = 0;
    while (!oktodraw)
    {
	XNextEvent(X_display, &X_event);
	if (X_event.type == Expose
	    && !X_event.xexpose.count)
	{
	    oktodraw = 1;
	}
    }

    // grabs the pointer so it is restricted to this window
    if (grabMouse)
	XGrabPointer(X_display, X_mainWindow, True,
		     ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
		     GrabModeAsync, GrabModeAsync,
		     X_mainWindow, None, CurrentTime);

    if (doShm)
    {

	X_shmeventtype = XShmGetEventBase(X_display) + ShmCompletion;

	// create the image
	image = XShmCreateImage(	X_display,
					X_visual,
					8,
					ZPixmap,
					0,
					&X_shminfo,
					X_width,
					X_height );

	grabsharedmemory(image->bytes_per_line * image->height);


	// UNUSED
	// create the shared memory segment
	// X_shminfo.shmid = shmget (IPC_PRIVATE,
	// image->bytes_per_line * image->height, IPC_CREAT | 0777);
	// if (X_shminfo.shmid < 0)
	// {
	// perror("");
	// I_Error("shmget() failed in InitGraphics()");
	// }
	// fprintf(stderr, "shared memory id=%d\n", X_shminfo.shmid);
	// attach to the shared memory segment
	// image->data = X_shminfo.shmaddr = shmat(X_shminfo.shmid, 0, 0);
	

	if (!image->data)
	{
	    perror("");
	    I_Error("shmat() failed in InitGraphics()");
	}

	// get the X server to attach to it
	if (!XShmAttach(X_display, &X_shminfo))
	    I_Error("XShmAttach() failed in InitGraphics()");

    }
    else
    {
	image = XCreateImage(	X_display,
    				X_visual,
    				8,
    				ZPixmap,
    				0,
    				(char*)malloc(X_width * X_height),
    				X_width, X_height,
    				8,
    				X_width );

    }

    if (multiply == 1)
	screens[0] = (unsigned char *) (image->data);
    else
	screens[0] = (unsigned char *) malloc (SCREENWIDTH * SCREENHEIGHT);

}


unsigned	exptable[256];

void InitExpand (void)
{
    int		i;
	
    for (i=0 ; i<256 ; i++)
	exptable[i] = i | (i<<8) | (i<<16) | (i<<24);
}

double		exptable2[256*256];

void InitExpand2 (void)
{
    int		i;
    int		j;
    // UNUSED unsigned	iexp, jexp;
    double*	exp;
    union
    {
	double 		d;
	unsigned	u[2];
    } pixel;
	
    printf ("building exptable2...\n");
    exp = exptable2;
    for (i=0 ; i<256 ; i++)
    {
	pixel.u[0] = i | (i<<8) | (i<<16) | (i<<24);
	for (j=0 ; j<256 ; j++)
	{
	    pixel.u[1] = j | (j<<8) | (j<<16) | (j<<24);
	    *exp++ = pixel.d;
	}
    }
    printf ("done.\n");
}

int	inited;

void
Expand4
( unsigned*	lineptr,
  double*	xline )
{
    double	dpixel;
    unsigned	x;
    unsigned 	y;
    unsigned	fourpixels;
    unsigned	step;
    double*	exp;
	
    exp = exptable2;
    if (!inited)
    {
	inited = 1;
	InitExpand2 ();
    }
		
		
    step = 3*SCREENWIDTH/2;
	
    y = SCREENHEIGHT-1;
    do
    {
	x = SCREENWIDTH;

	do
	{
	    fourpixels = lineptr[0];
			
	    dpixel = *(double *)( (int)exp + ( (fourpixels&0xffff0000)>>13) );
	    xline[0] = dpixel;
	    xline[160] = dpixel;
	    xline[320] = dpixel;
	    xline[480] = dpixel;
			
	    dpixel = *(double *)( (int)exp + ( (fourpixels&0xffff)<<3 ) );
	    xline[1] = dpixel;
	    xline[161] = dpixel;
	    xline[321] = dpixel;
	    xline[481] = dpixel;

	    fourpixels = lineptr[1];
			
	    dpixel = *(double *)( (int)exp + ( (fourpixels&0xffff0000)>>13) );
	    xline[2] = dpixel;
	    xline[162] = dpixel;
	    xline[322] = dpixel;
	    xline[482] = dpixel;
			
	    dpixel = *(double *)( (int)exp + ( (fourpixels&0xffff)<<3 ) );
	    xline[3] = dpixel;
	    xline[163] = dpixel;
	    xline[323] = dpixel;
	    xline[483] = dpixel;

	    fourpixels = lineptr[2];
			
	    dpixel = *(double *)( (int)exp + ( (fourpixels&0xffff0000)>>13) );
	    xline[4] = dpixel;
	    xline[164] = dpixel;
	    xline[324] = dpixel;
	    xline[484] = dpixel;
			
	    dpixel = *(double *)( (int)exp + ( (fourpixels&0xffff)<<3 ) );
	    xline[5] = dpixel;
	    xline[165] = dpixel;
	    xline[325] = dpixel;
	    xline[485] = dpixel;

	    fourpixels = lineptr[3];
			
	    dpixel = *(double *)( (int)exp + ( (fourpixels&0xffff0000)>>13) );
	    xline[6] = dpixel;
	    xline[166] = dpixel;
	    xline[326] = dpixel;
	    xline[486] = dpixel;
			
	    dpixel = *(double *)( (int)exp + ( (fourpixels&0xffff)<<3 ) );
	    xline[7] = dpixel;
	    xline[167] = dpixel;
	    xline[327] = dpixel;
	    xline[487] = dpixel;

	    lineptr+=4;
	    xline+=8;
	} while (x-=16);
	xline += step;
    } while (y--);
}



#endif