#define WINVER 0x0500                                  /* バージョン定義 Windows2000以上 */
#define _WIN32_WINNT WINVER

#include "ruby.h"
#ifndef RUBY_ST_H
#include "st.h"
#endif

#include <shlobj.h>
#include "dxruby.h"
#include "image.h"
#include "input.h"
#include "sound.h"
#include "font.h"
#ifdef DXRUBY15
#include "matrix.h"
#endif
#include "sprite.h"
#include "messagethread.h"

VALUE mDXRuby;       /* DXRubyモジュール     */
VALUE eDXRubyError;  /* 例外                 */
VALUE mWindow;       /* ウィンドウモジュール */
VALUE cRenderTarget; /* レンダーターゲットクラス */
VALUE cShaderCore;   /* シェーダコアクラス   */
VALUE cShader;   /* シェーダクラス       */

extern VALUE cImage;

#ifdef DXRUBY_USE_TYPEDDATA
extern rb_data_type_t Image_data_type;
extern rb_data_type_t Font_data_type;
#ifdef DXRUBY15
extern rb_data_type_t Matrix_data_type;
extern rb_data_type_t Vector_data_type;
#endif
#endif

/* グローバル変数たち */
HINSTANCE             g_hInstance   = NULL; /* アプリケーションインスタンス   */
HANDLE                g_hWnd        = NULL; /* ウィンドウハンドル             */
LPDIRECT3D9           g_pD3D        = NULL; /* Direct3Dインターフェイス       */
LPDIRECT3DDEVICE9     g_pD3DDevice  = NULL; /* Direct3DDeviceインターフェイス */
D3DPRESENT_PARAMETERS g_D3DPP;              /* D3DDeviceの設定                */
LPD3DXSPRITE          g_pD3DXSprite = NULL; /* D3DXSprite                     */

int g_iRefAll = 1; /* インターフェースの参照カウント */

/* フレーム調整用 */
static __int64 g_OneSecondCount       = 0;         /* 一秒間にカウンタが数える数         */
static int     g_isPerformanceCounter = 0;         /* パフォーマンスカウンタがあったら１ */
static __int64 g_OldTime              = 0;         /* 前回のフレームが終わった時間       */
static __int64 g_OneFrameCount        = 0;         /* １フレームの処理にかかった時間     */
static __int64 g_DrawEndTime          = 0;         /* 描画完了時間                       */
static __int64 g_StartTime            = 0;         /* Window.loopを最初に実行した時間    */
static __int64 g_RunningTime          = 0;         /* 実行経過時間(最初のWindow.loopから)*/
static int g_skip                     = 0;         /* スキップしたフレームは1になる      */
int g_sync                     = 0;         /* 垂直同期モード = 1                 */
int retry_flag = 0;
static HCURSOR mouse_cursor;
BYTE g_byMouseState_L_buf;
BYTE g_byMouseState_M_buf;
BYTE g_byMouseState_R_buf;

/* エンコーディング情報 */
rb_encoding *g_enc_sys;
rb_encoding *g_enc_utf16;
rb_encoding *g_enc_utf8;

/* システムエンコード */
char sys_encode[256];

/* ウィンドウ情報 */
struct DXRubyWindowInfo g_WindowInfo;

/* Picture系基底構造体 */
struct DXRubyPicture {
    void (*func)(void*);
    VALUE value;
    unsigned char blendflag; /* 半透明(000)、加算合成1(100)、加算合成2(101)、減算合成1(110)、減算合成2(111)のフラグ */
    char reserve1;
    char reserve2;
    char reserve3;
};

typedef struct tag_dx_TLVERTEX {
    float           x, y, z;
    D3DCOLOR        color;
    float           tu, tv;
}TLVERTX;

typedef struct tag_dx_TLVERTEX2 {
    float           x, y, z;
    D3DCOLOR        color;
}TLVERTX2;

/* デバイスロストで解放・復元するもの */
struct DXRubyLostList {
    void **pointer;
    int allocate_size;
    int count;
} g_RenderTargetList, g_ShaderCoreList;

/* シンボル */
VALUE symbol_blend   = Qundef;
VALUE symbol_angle   = Qundef;
VALUE symbol_alpha   = Qundef;
VALUE symbol_scalex  = Qundef;
VALUE symbol_scaley  = Qundef;
VALUE symbol_centerx = Qundef;
VALUE symbol_centery = Qundef;
VALUE symbol_scale_x  = Qundef;
VALUE symbol_scale_y  = Qundef;
VALUE symbol_center_x = Qundef;
VALUE symbol_center_y = Qundef;
VALUE symbol_z       = Qundef;
VALUE symbol_color   = Qundef;
VALUE symbol_add     = Qundef;
VALUE symbol_add2    = Qundef;
VALUE symbol_sub     = Qundef;
VALUE symbol_sub2    = Qundef;
VALUE symbol_none    = Qundef;
VALUE symbol_offset_sync = Qundef;
VALUE symbol_dividex = Qundef;
VALUE symbol_dividey = Qundef;
VALUE symbol_divide_x = Qundef;
VALUE symbol_divide_y = Qundef;
VALUE symbol_edge       = Qundef;
VALUE symbol_edge_color = Qundef;
VALUE symbol_edge_width = Qundef;
VALUE symbol_edge_level = Qundef;
VALUE symbol_shadow     = Qundef;
VALUE symbol_shadow_color = Qundef;
VALUE symbol_shadow_x = Qundef;
VALUE symbol_shadow_y = Qundef;
VALUE symbol_shadow_edge = Qundef;
VALUE symbol_shader  = Qundef;
VALUE symbol_int     = Qundef;
VALUE symbol_float   = Qundef;
VALUE symbol_texture = Qundef;
VALUE symbol_technique = Qundef;
VALUE symbol_discard = Qundef;
VALUE symbol_aa = Qundef;
VALUE symbol_call = Qundef;

/* プロトタイプ宣言 */
static void InitWindow( void );
static void InitDXGraphics( void );
LRESULT CALLBACK MainWndProc( HWND hWnd,UINT msg,UINT wParam,LONG lParam );
static __int64 GetSystemCounter( void );
static void Window_DirectXRelease( void );
static VALUE Window_update( VALUE );
static VALUE Window_sync( VALUE );
static VALUE Window_create( VALUE );

static VALUE RenderTarget_drawPixel( int argc, VALUE *argv, VALUE obj );
static VALUE RenderTarget_drawLine ( int argc, VALUE *argv, VALUE obj );
static VALUE RenderTarget_drawBox  ( int argc, VALUE *argv, VALUE obj );
static VALUE RenderTarget_drawBoxFill ( int argc, VALUE *argv, VALUE obj );
static VALUE RenderTarget_drawCircle ( int argc, VALUE *argv, VALUE obj );
static VALUE RenderTarget_drawCircleFill ( int argc, VALUE *argv, VALUE obj );
static VALUE RenderTarget_draw     ( int argc, VALUE *argv, VALUE obj );
static VALUE RenderTarget_drawAlpha( int argc, VALUE *argv, VALUE obj );
static VALUE RenderTarget_drawAdd  ( int argc, VALUE *argv, VALUE obj );
static VALUE RenderTarget_drawSub  ( int argc, VALUE *argv, VALUE obj );
static VALUE RenderTarget_drawShader( int argc, VALUE *argv, VALUE obj );
static VALUE RenderTarget_drawScale( int argc, VALUE *argv, VALUE obj );
static VALUE RenderTarget_drawRot  ( int argc, VALUE *argv, VALUE obj );
static VALUE RenderTarget_drawEx   ( int argc, VALUE *argv, VALUE obj );
static VALUE RenderTarget_drawFont ( int argc, VALUE *argv, VALUE obj );
static VALUE RenderTarget_drawFontEx ( int argc, VALUE *argv, VALUE obj );
static VALUE RenderTarget_drawMorph( int argc, VALUE *argv, VALUE obj );
static VALUE RenderTarget_drawTile ( int argc, VALUE *argv, VALUE obj );
//static VALUE RenderTarget_drawSprite( VALUE self, VALUE varg );
static VALUE RenderTarget_getMinFilter( VALUE self );
static VALUE RenderTarget_setMinFilter( VALUE self, VALUE filter );
static VALUE RenderTarget_getMagFilter( VALUE self );
static VALUE RenderTarget_setMagFilter( VALUE self, VALUE filter );
static void Window_setDefaultIcon( void );
static void Window_createCircleShader(void);
static void Window_createCircleFillShader(void);
static void CleanRenderTargetList( void );

/*********************************************************************
 * Windowモジュール
 * ウィンドウの管理・描画を行う。
 *********************************************************************/
/*--------------------------------------------------------------------
   画面クリア色取得
 ---------------------------------------------------------------------*/
static VALUE Window_get_bgcolor( VALUE obj )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );
    return rb_ary_new3( 3, INT2FIX( rt->r ), INT2FIX( rt->g ), INT2FIX( rt->b ) );
}


/*--------------------------------------------------------------------
   画面クリア色指定
 ---------------------------------------------------------------------*/
static VALUE Window_set_bgcolor( VALUE obj, VALUE array )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );

    Check_Type(array, T_ARRAY);

    if( RARRAY_LEN(array) == 4 )
    {
        rt->a = NUM2INT( rb_ary_entry(array, 0) );
        rt->r = NUM2INT( rb_ary_entry(array, 1) );
        rt->g = NUM2INT( rb_ary_entry(array, 2) );
        rt->b = NUM2INT( rb_ary_entry(array, 3) );
    }
    else
    {
        rt->a = 255;
        rt->r = NUM2INT( rb_ary_entry(array, 0) );
        rt->g = NUM2INT( rb_ary_entry(array, 1) );
        rt->b = NUM2INT( rb_ary_entry(array, 2) );
    }

    return array;
}

/*--------------------------------------------------------------------
   描画設定（通常描画）
 ---------------------------------------------------------------------*/
static VALUE Window_draw( int argc, VALUE *argv, VALUE obj )
{
    RenderTarget_draw( argc, argv, g_WindowInfo.render_target );
    return obj;
}

/*--------------------------------------------------------------------
   描画設定（点描画）
 ---------------------------------------------------------------------*/
static VALUE Window_drawPixel( int argc, VALUE *argv, VALUE obj )
{
    RenderTarget_drawPixel( argc, argv, g_WindowInfo.render_target );
    return obj;
}

/*--------------------------------------------------------------------
   描画設定（線描画）
 ---------------------------------------------------------------------*/
static VALUE Window_drawLine( int argc, VALUE *argv, VALUE obj )
{
    RenderTarget_drawLine( argc, argv, g_WindowInfo.render_target );
    return obj;
}

/*--------------------------------------------------------------------
   描画設定（四角形描画）
 ---------------------------------------------------------------------*/
static VALUE Window_drawBox( int argc, VALUE *argv, VALUE obj )
{
    RenderTarget_drawBox( argc, argv, g_WindowInfo.render_target );
    return obj;
}

/*--------------------------------------------------------------------
   描画設定（塗り潰し四角形描画）
 ---------------------------------------------------------------------*/
static VALUE Window_drawBoxFill( int argc, VALUE *argv, VALUE obj )
{
    RenderTarget_drawBoxFill( argc, argv, g_WindowInfo.render_target );
    return obj;
}

/*--------------------------------------------------------------------
   描画設定（塗り潰し円描画）
 ---------------------------------------------------------------------*/
static VALUE Window_drawCircleFill( int argc, VALUE *argv, VALUE obj )
{
    RenderTarget_drawCircleFill( argc, argv, g_WindowInfo.render_target );
    return obj;
}

/*--------------------------------------------------------------------
   描画設定（円描画）
 ---------------------------------------------------------------------*/
static VALUE Window_drawCircle( int argc, VALUE *argv, VALUE obj )
{
    RenderTarget_drawCircle( argc, argv, g_WindowInfo.render_target );
    return obj;
}

/*--------------------------------------------------------------------
   描画設定（半透明描画）
 ---------------------------------------------------------------------*/
static VALUE Window_drawAlpha( int argc, VALUE *argv, VALUE obj )
{
    RenderTarget_drawAlpha( argc, argv, g_WindowInfo.render_target );
    return obj;
}


/*--------------------------------------------------------------------
   描画設定（加算合成描画）
 ---------------------------------------------------------------------*/
static VALUE Window_drawAdd( int argc, VALUE *argv, VALUE obj )
{
    RenderTarget_drawAdd( argc, argv, g_WindowInfo.render_target );
    return obj;
}


/*--------------------------------------------------------------------
   描画設定（減算合成描画）
 ---------------------------------------------------------------------*/
static VALUE Window_drawSub( int argc, VALUE *argv, VALUE obj )
{
    RenderTarget_drawSub( argc, argv, g_WindowInfo.render_target );
    return obj;
}


/*--------------------------------------------------------------------
   描画設定（シェーダ描画）
 ---------------------------------------------------------------------*/
static VALUE Window_drawShader( int argc, VALUE *argv, VALUE obj )
{
    RenderTarget_drawShader( argc, argv, g_WindowInfo.render_target );
    return obj;
}


/*--------------------------------------------------------------------
   描画設定（拡大縮小描画）
 ---------------------------------------------------------------------*/
static VALUE Window_drawScale( int argc, VALUE *argv, VALUE obj )
{
    RenderTarget_drawScale( argc, argv, g_WindowInfo.render_target );
    return obj;
}

/*--------------------------------------------------------------------
   描画設定（回転描画）
 ---------------------------------------------------------------------*/
static VALUE Window_drawRot( int argc, VALUE *argv, VALUE obj )
{
    RenderTarget_drawRot( argc, argv, g_WindowInfo.render_target );
    return obj;
}

/*--------------------------------------------------------------------
   描画設定（フルオプション）
 ---------------------------------------------------------------------*/
static VALUE Window_drawEx( int argc, VALUE *argv, VALUE obj )
{
    RenderTarget_drawEx( argc, argv, g_WindowInfo.render_target );
    return obj;
}


/*--------------------------------------------------------------------
   フォント描画
 ---------------------------------------------------------------------*/
static VALUE Window_drawFont( int argc, VALUE *argv, VALUE obj )
{
    RenderTarget_drawFont( argc, argv, g_WindowInfo.render_target );
    return obj;
}

/*--------------------------------------------------------------------
   高品質フォント描画
 ---------------------------------------------------------------------*/
static VALUE Window_drawFontEx( int argc, VALUE *argv, VALUE obj )
{
    RenderTarget_drawFontEx( argc, argv, g_WindowInfo.render_target );
    return obj;
}


/*--------------------------------------------------------------------
   描画設定（4点指定）
 ---------------------------------------------------------------------*/
static VALUE Window_drawMorph( int argc, VALUE *argv, VALUE obj )
{
    RenderTarget_drawMorph( argc, argv, g_WindowInfo.render_target );
    return obj;
}


/*--------------------------------------------------------------------
   マップ描画
 ---------------------------------------------------------------------*/
static VALUE Window_drawTile( int argc, VALUE *argv, VALUE obj )
{
    RenderTarget_drawTile( argc, argv, g_WindowInfo.render_target );
    return obj;
}


///*--------------------------------------------------------------------
//   Sprite描画
// ---------------------------------------------------------------------*/
//static VALUE Window_drawSprite( VALUE obj, VALUE varg )
//{
//    RenderTarget_drawSprite( g_WindowInfo.render_target, varg );
//    return Qnil;
//}


static VALUE Window_create( VALUE klass )
{
    if( !g_WindowInfo.created )
    {
        WindowCreateMessage();
        g_WindowInfo.requestclose = 0;

        /* フレーム調整処理初期化 */
        g_StartTime = GetSystemCounter();
        g_RunningTime = 0;
    }

    return Qnil;
}

int Window_autocall_foreach( VALUE key, VALUE value, VALUE obj )
{
    rb_funcall( value, SYM2ID( symbol_call ), 0 );
    return ST_CONTINUE;
}

/*--------------------------------------------------------------------
   Windows.loop
 ---------------------------------------------------------------------*/
static VALUE Window_loop( int argc, VALUE *argv, VALUE obj )
{
    VALUE vclose_cancel;
    rb_scan_args( argc, argv, "01", &vclose_cancel );

    if( !g_WindowInfo.created )
    {
        Window_create( obj );
    }

    /* 終了条件が無い */
    while( 1 )
    {
        /* 自動的に閉じる場合はxボタンで閉じる */
        if( g_WindowInfo.requestclose == 1 )
        {
            if( !RTEST( vclose_cancel ) )
            {
                g_WindowInfo.requestclose = 0;
                ShowWindow( g_hWnd, SW_HIDE );
                g_WindowInfo.created = 0;
                break;
            }
        }

        /* メッセージが無い時 */
        /* 入力状態更新 */
        inputupdate_internal();

        /* before_call処理 */
        rb_hash_foreach( g_WindowInfo.before_call, Window_autocall_foreach, obj );

        /* ブロック実行 */
        rb_yield( obj );

        /* ブロック終了時にウィンドウが閉じられていたら終了する */
        if( !g_WindowInfo.created )
        {
            break;
        }

        /* after_call処理 */
        rb_hash_foreach( g_WindowInfo.after_call, Window_autocall_foreach, obj );

        /* fps調整 */
        Window_sync( obj );

        /* 描画 */
        Window_update( Qnil );
    }

    return Qnil;
}

static VALUE Window_close( VALUE klass )
{
    if( g_WindowInfo.created )
    {
        ShowWindow( g_hWnd, SW_HIDE );
        g_WindowInfo.created = 0;
    }

    return Qnil;
}

static VALUE Window_get_created( VALUE klass )
{
    return g_WindowInfo.created ? Qtrue : Qfalse;
}

static VALUE Window_get_closed( VALUE klass )
{
    return g_WindowInfo.created ? Qfalse : Qtrue;
}

/*--------------------------------------------------------------------
   画面更新
 ---------------------------------------------------------------------*/
static VALUE Window_update( VALUE obj )
{
    HRESULT hr;
    int i;
    int ret = -1;

    if( obj != Qnil )
    {
        rb_hash_foreach( g_WindowInfo.after_call, Window_autocall_foreach, obj );
    }

    if( g_sync == 0 ) // 非同期モード
    {
        if( g_skip == 0 )
        {
            RenderTarget_update( g_WindowInfo.render_target );
            hr = g_pD3DDevice->lpVtbl->Present( g_pD3DDevice, NULL, NULL, NULL, NULL );
            if( hr == D3DERR_DEVICELOST )
            {
                ret = ResetMessage();
            }
        }
    }
    else // 同期モード
    {
        if( g_skip == 0 )
        {
            RenderTarget_update( g_WindowInfo.render_target );

            hr = g_pD3DDevice->lpVtbl->Present( g_pD3DDevice, NULL, NULL, NULL, NULL );
            if( hr == D3DERR_DEVICELOST )
            {
                ret = ResetMessage();
            }

            g_OneFrameCount = GetSystemCounter() - g_OldTime;

            if( GetSystemCounter() > g_OldTime + g_OneSecondCount / g_WindowInfo.fps * 1.5 ) // フレーム内に収まらなかった
            {
                // 次フレームはスキップ
                g_skip = 1;
            }

            g_OldTime = GetSystemCounter();
        }
        else
        {
            // スキップしたら次は普通に。
            g_skip = 0;
            g_OldTime += g_OneSecondCount / g_WindowInfo.fps;
        }
    }

    // デバイスロストから復帰した
    if( ret == 0 )
    {
        rb_gc_start();
        for( i = 0; i < g_RenderTargetList.count; i++ )
        {
            if( g_RenderTargetList.pointer[i] )
            {
                struct DXRubyRenderTarget *rt = (struct DXRubyRenderTarget *)g_RenderTargetList.pointer[i];
#ifdef DXRUBY15
                // 再生成Procが設定されている場合
                if( rt->vregenerate_proc != Qnil )
                {
                    rb_funcall( rt->vregenerate_proc, SYM2ID( symbol_call ), 0 );
                    rt->clearflag = 0;
                }
#endif
            }
        }
    }

    // Windowに関連付けられた内部生成Imageの破棄
    {
        struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );
        rt->PictureCount = 0;
        rt->PictureSize = 0;
        rt->PictureDecideCount = 0;
        rt->PictureDecideSize = 0;
        for( i = 0; i < RARRAY_LEN(g_WindowInfo.image_array); i++ )
        {
            Image_dispose( RARRAY_AREF(g_WindowInfo.image_array, i) );
        }
        rb_ary_clear( g_WindowInfo.image_array );
    }

    CleanRenderTargetList();
    g_RunningTime = GetSystemCounter() - g_StartTime;

    g_WindowInfo.input_updated = 0;
    return Qnil;
}


/*--------------------------------------------------------------------
  （内部関数）フレーム調整
 ---------------------------------------------------------------------*/
static VALUE Window_sync( VALUE obj )
{
    __int64 NowTime;
    __int64 WaitTime;
    static __int64 BeforeSecond = 0;
    static int fps = 0;
    static int mod = 0;
    static int skip_count = 0;

    NowTime = GetSystemCounter();

    /* 非同期モード時 */
    if( g_sync == 0 )
    {
        g_OneFrameCount = NowTime - g_OldTime;
        if ( g_WindowInfo.fps > 0 ) /* fps指定がnil or 0の時はWait処理しない */
        {
            __int64 SleepTime;

            WaitTime = g_OneSecondCount / g_WindowInfo.fps;

            /* 誤差補正 */
            mod += g_OneSecondCount % g_WindowInfo.fps;
            if( mod >= g_WindowInfo.fps )
            {
                mod -= g_WindowInfo.fps;
                WaitTime += 1;
            }

            /* もう既に前回から１フレーム分の時間が経っていたら */
            if( g_OldTime + WaitTime < NowTime && g_skip == 0 )
            {
                /* コマ落ち制御しない場合 */
                if( g_WindowInfo.frameskip == Qfalse )
                {
                    fps++;
                    g_OldTime = NowTime;
                }
                else /* する場合 */
                {
                    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );
                    /* 今回はウェイトも描画もしない */
                    g_skip = 1;
                    rt->PictureCount = 0;
                    rt->PictureSize = 0;
                    rt->PictureDecideCount = 0;
                    rt->PictureDecideSize = 0;
                    g_OldTime = g_OldTime + WaitTime;
                }
            }
            else
            {
                /* 前回描画を飛ばしたのに今回も間に合ってない場合 */
                if( g_OldTime + WaitTime < NowTime && g_skip == 1 )
                {
                    /* 諦めて処理落ち */
                    g_OldTime = NowTime;
                }
                else
                {
                    __int64 TempTime;
                    /* おおまかな時間をSleepで待つ */
                    while( (WaitTime - (GetSystemCounter() - g_OldTime)) * 1000 / g_OneSecondCount > 2 )
                    {
//                        Sleep(1);
                        rb_thread_wait_for(rb_time_interval(rb_float_new(1.0 / 1000.0)));
                    }

                    /* ループで厳密に処理をする */
                    for ( ; ; )
                    {
                        TempTime = GetSystemCounter();
                        if( g_OldTime +  WaitTime < TempTime )
                        {
                            break;
                        }
                    }
                    g_OldTime = g_OldTime +  WaitTime;
                }
                g_skip = 0;
                fps++;
            }
        }
        else
        {
            g_OldTime = NowTime;
            g_skip = 0;
            fps++;
        }
    }
    else // 同期モード
    {
        if( g_skip == 0 )
        {
            fps++;
        }
    }

    /* FPS値設定 */
    if( (NowTime - BeforeSecond) >= g_OneSecondCount )
    {
        BeforeSecond = NowTime;
        g_WindowInfo.fpscheck = fps;
        fps = 0;
    }

    return Qnil;
}

/*--------------------------------------------------------------------
  （内部関数）フレーム調整用カウンタ値取得
 ---------------------------------------------------------------------*/
static __int64 GetSystemCounter( void )
{
    __int64 time;

    if( g_isPerformanceCounter == 1 )
    {
        QueryPerformanceCounter( (LARGE_INTEGER *)&time );
        return time;
    }
    else
    {
        return timeGetTime();
    }
}


/*--------------------------------------------------------------------
   終了時に実行する
 ---------------------------------------------------------------------*/
static VALUE Window_shutdown( VALUE obj )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );

    /* アイコンリソース解放 */
    if( g_WindowInfo.hIcon != 0 )
    {
        DestroyIcon(g_WindowInfo.hIcon);
    }

    /* マウス状態復元 */
    if( !g_WindowInfo.enablemouse )
    {
        ShowCursorMessage();
    }

    /* リスト解放 */
    free( rt->PictureList );
    free( rt->PictureStruct );
    rt->PictureCount = 0;
#ifdef DXRUBY15
    rt->vregenerate_proc = Qnil;
#endif
    ExitMessageThread();

    /* マウスカーソルを戻す */
    SetCursor( mouse_cursor );

    /* フレーム調整の後始末 */
    timeEndPeriod( 1 );

    /* Imageの終了処理 */
    finalize_dxruby_Image();

    return obj;
}


/*--------------------------------------------------------------------
    経過時間取得
 ---------------------------------------------------------------------*/
static VALUE Window_running_time( VALUE klass )
{
    if( !g_WindowInfo.created )
    {
        rb_raise( eDXRubyError, "It can not be executed before window creation - Window_running_time" );
    }

    return rb_float_new( g_RunningTime * 1000.0 / g_OneSecondCount );
}


/*--------------------------------------------------------------------
   ウィンドウのサイズを変更する。
 ---------------------------------------------------------------------*/
static VALUE Window_resize( VALUE klass, VALUE vwidth, VALUE vheight )
{
    g_WindowInfo.width = NUM2INT( vwidth );
    g_WindowInfo.height = NUM2INT( vheight );
    if( g_WindowInfo.created )
    {
        g_WindowInfo.created = 0;
        Window_create( klass );
    }
    return Qnil;
}


/*--------------------------------------------------------------------
   ウィンドウのモード（ウィンドウ/全画面）を設定する。
 ---------------------------------------------------------------------*/
static VALUE Window_setwindowed( VALUE klass, VALUE vwindowed )
{
    if( g_WindowInfo.windowed != RTEST(vwindowed) )
    {
        g_WindowInfo.windowed = RTEST(vwindowed);

        if( g_WindowInfo.created )
        {
            g_WindowInfo.created = 0;
            Window_create( klass );
        }
    }

    return vwindowed;
}


/*--------------------------------------------------------------------
   ウィンドウのモード（ウィンドウ/全画面）を設定する。
 ---------------------------------------------------------------------*/
static VALUE Window_setfullscreen( VALUE klass, VALUE vfullscreen )
{
    if( g_WindowInfo.windowed != !RTEST(vfullscreen) )
    {
        g_WindowInfo.windowed = !RTEST(vfullscreen);

        if( g_WindowInfo.created )
        {
            g_WindowInfo.created = 0;
            Window_create( klass );
        }
    }

    return vfullscreen;
}


/*--------------------------------------------------------------------
   ウィンドウのモード（ウィンドウ/全画面）を取得する。
 ---------------------------------------------------------------------*/
static VALUE Window_getwindowed( VALUE klass )
{
    return g_WindowInfo.windowed ? Qtrue : Qfalse;
}


/*--------------------------------------------------------------------
   ウィンドウのモード（ウィンドウ/全画面）を取得する。
 ---------------------------------------------------------------------*/
static VALUE Window_getfullscreen( VALUE klass )
{
    return g_WindowInfo.windowed ? Qfalse : Qtrue;
}


/*--------------------------------------------------------------------
   ウィンドウの位置（x座標）を設定する。
 ---------------------------------------------------------------------*/
static VALUE Window_setx( VALUE klass, VALUE x )
{
    if( g_WindowInfo.created )
    {
        rb_raise( eDXRubyError, "It is not possible to change the setting after window creation - Window_setx" );
    }

    g_WindowInfo.x = NUM2INT( x );
    return x;
}


/*--------------------------------------------------------------------
   ウィンドウの位置（y座標）を設定する。
 ---------------------------------------------------------------------*/
static VALUE Window_sety( VALUE klass , VALUE y )
{
    if( g_WindowInfo.created )
    {
        rb_raise( eDXRubyError, "It is not possible to change the setting after window creation - Window_sety" );
    }

    g_WindowInfo.y = NUM2INT( y );
    return y;
}


/*--------------------------------------------------------------------
   ウィンドウのサイズ（幅）を設定する。
 ---------------------------------------------------------------------*/
static VALUE Window_setwidth( VALUE klass, VALUE vwidth )
{
    int width;

    if( g_WindowInfo.created )
    {
        rb_raise( eDXRubyError, "It is not possible to change the setting after window creation - Window_setwidth" );
    }

    width = NUM2INT( vwidth );
    if( width < 0 )
    {
        width = 0;
    }

    g_WindowInfo.width = width;
    return vwidth;
}


/*--------------------------------------------------------------------
   ウィンドウのサイズ（高さ）を設定する。
 ---------------------------------------------------------------------*/
static VALUE Window_setheight( VALUE klass , VALUE vheight)
{
    int height;

    if( g_WindowInfo.created )
    {
        rb_raise( eDXRubyError, "It is not possible to change the setting after window creation - Window_setheight" );
    }

    height = NUM2INT( vheight );
    if( height < 0 )
    {
        height = 0;
    }

    g_WindowInfo.height = height;
    return vheight;
}


/*--------------------------------------------------------------------
   ウィンドウの位置（x座標）を返す。
 ---------------------------------------------------------------------*/
static VALUE Window_x( VALUE klass )
{
    return INT2FIX( g_WindowInfo.x );
}


/*--------------------------------------------------------------------
   ウィンドウの位置（y座標）を返す。
 ---------------------------------------------------------------------*/
static VALUE Window_y( VALUE klass )
{
    return INT2FIX( g_WindowInfo.y );
}


/*--------------------------------------------------------------------
   ウィンドウのサイズ（幅）を返す。
 ---------------------------------------------------------------------*/
static VALUE Window_width( VALUE klass )
{
    return INT2FIX( g_WindowInfo.width );
}


/*--------------------------------------------------------------------
   ウィンドウのサイズ（高さ）を返す。
 ---------------------------------------------------------------------*/
static VALUE Window_height( VALUE klass )
{
    return INT2FIX( g_WindowInfo.height );
}


/*--------------------------------------------------------------------
   ウィンドウの描画開始位置xを返す。
 ---------------------------------------------------------------------*/
static VALUE Window_getOx( VALUE self )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );
    return INT2FIX( rt->ox );
}


/*--------------------------------------------------------------------
   ウィンドウの描画開始位置yを返す。
 ---------------------------------------------------------------------*/
static VALUE Window_getOy( VALUE self )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );
    return INT2FIX( rt->oy );
}


/*--------------------------------------------------------------------
   ウィンドウの描画開始位置xを設定する。
 ---------------------------------------------------------------------*/
static VALUE Window_setOx( VALUE self, VALUE vox )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );
    rt->ox = NUM2INT( vox );
    return vox;
}


/*--------------------------------------------------------------------
   ウィンドウの描画開始位置yを設定する。
 ---------------------------------------------------------------------*/
static VALUE Window_setOy( VALUE self, VALUE voy )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );
    rt->oy = NUM2INT( voy );
    return voy;
}


/*--------------------------------------------------------------------
  ウィンドウタイトル取得
 ---------------------------------------------------------------------*/
static VALUE Window_getCaption( VALUE klass )
{
    char buf[256];
    VALUE vstr;
    VALUE venc = rb_enc_default_internal();

    GetWindowText( g_hWnd, buf, 256 );

    vstr = rb_enc_associate( rb_str_new2( buf ), g_enc_sys );

    if( RTEST(venc) )
    {
        vstr = rb_str_export_to_enc( vstr, rb_default_internal_encoding() );
    }

    return vstr;
}


/*--------------------------------------------------------------------
  ウィンドウタイトル設定
 ---------------------------------------------------------------------*/
static VALUE Window_setCaption( VALUE klass, VALUE vcaption )
{
    Check_Type(vcaption, T_STRING);

    if( rb_enc_get_index( vcaption ) != 0 )
    {
        VALUE vstr = rb_str_export_to_enc( vcaption, g_enc_utf16 );
        int len = RSTRING_LEN( vstr );
        char *buf = alloca( len + 2 );
        buf[len] = buf[len + 1] = 0;
        memcpy( buf, RSTRING_PTR( vstr ), len );
        SetWindowTextW( g_hWnd, (LPWSTR)buf );
    }
    else
    {
        SetWindowText( g_hWnd, RSTRING_PTR( vcaption ) );
    }

    return vcaption;
}


/*--------------------------------------------------------------------
  ウィンドウのサイズ倍率を取得する
 ---------------------------------------------------------------------*/
static VALUE Window_getScale( VALUE klass )
{
    return rb_float_new( g_WindowInfo.scale );
}


/*--------------------------------------------------------------------
  ウィンドウのサイズ倍率を設定する
 ---------------------------------------------------------------------*/
static VALUE Window_setScale( VALUE klass, VALUE vscale )
{
    g_WindowInfo.scale = NUM2FLOAT( vscale );

    return vscale;
}


/*--------------------------------------------------------------------
  fps値を設定する
 ---------------------------------------------------------------------*/
static VALUE Window_getrealfps( VALUE obj )
{
    return INT2FIX( g_WindowInfo.fpscheck );
}


/*--------------------------------------------------------------------
  fps値を取得する
 ---------------------------------------------------------------------*/
static VALUE Window_getfps( VALUE obj )
{
    return INT2FIX( g_WindowInfo.fpscheck );
}


/*--------------------------------------------------------------------
  fps値を設定する
 ---------------------------------------------------------------------*/
static VALUE Window_setfps( VALUE obj, VALUE vfps )
{
    g_WindowInfo.fps = vfps == Qnil ? 0 : NUM2INT( vfps );

    return vfps;
}


/*--------------------------------------------------------------------
   縮小フィルタ取得
 ---------------------------------------------------------------------*/
static VALUE Window_getMinFilter( VALUE obj )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );
    return INT2FIX( rt->minfilter );
}


/*--------------------------------------------------------------------
   縮小フィルタ設定
 ---------------------------------------------------------------------*/
static VALUE Window_setMinFilter( VALUE obj, VALUE vminfilter )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );
    rt->minfilter = FIX2INT( vminfilter );
    return vminfilter;
}


/*--------------------------------------------------------------------
   拡大フィルタ取得
 ---------------------------------------------------------------------*/
static VALUE Window_getMagFilter( VALUE obj )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );
    return INT2FIX( rt->magfilter );
}


/*--------------------------------------------------------------------
   拡大フィルタ設定
 ---------------------------------------------------------------------*/
static VALUE Window_setMagFilter( VALUE obj, VALUE vmagfilter )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );
    rt->magfilter = FIX2INT( vmagfilter );
    return vmagfilter;
}


/*--------------------------------------------------------------------
   描画予約確定
 ---------------------------------------------------------------------*/
static VALUE Window_decide( VALUE obj )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );
    rt->PictureDecideCount = rt->PictureCount;
    rt->PictureDecideSize = rt->PictureSize;
    return Qnil;
}


/*--------------------------------------------------------------------
   描画予約破棄
 ---------------------------------------------------------------------*/
static VALUE Window_discard( VALUE obj )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );
    rt->PictureCount = rt->PictureDecideCount;
    rt->PictureSize = rt->PictureDecideSize;
    return Qnil;
}


/*--------------------------------------------------------------------
  1フレームの処理負荷を％で取得する
 ---------------------------------------------------------------------*/
static VALUE Window_getload( VALUE obj )
{
    if( g_WindowInfo.fps == 0 )
    {
        return INT2FIX( 0 );
    }
    return rb_float_new( (double) ( (double)g_OneFrameCount * 100.0 / ((double)g_OneSecondCount / (double)g_WindowInfo.fps )) );
}


/*--------------------------------------------------------------------
  ウィンドウハンドルを取得する
 ---------------------------------------------------------------------*/
static VALUE Window_gethWnd( VALUE obj )
{
    return INT2NUM( (int)g_hWnd );
}


/*--------------------------------------------------------------------
  フレームスキップon/offを取得する
 ---------------------------------------------------------------------*/
static VALUE Window_getframeskip( VALUE obj )
{
    return g_WindowInfo.frameskip;
}


/*--------------------------------------------------------------------
  フレームスキップon/offを設定する
 ---------------------------------------------------------------------*/
static VALUE Window_setframeskip( VALUE obj, VALUE vskip )
{
    g_WindowInfo.frameskip = (vskip == Qnil || vskip == Qfalse) ? Qfalse : Qtrue;

    return vskip;
}


/*--------------------------------------------------------------------
  ウィンドウのアクティブ状態を取得
 ---------------------------------------------------------------------*/
static VALUE Window_get_active( VALUE obj )
{
    return ( g_WindowInfo.active != 0 ? Qtrue : Qfalse );
}


/*--------------------------------------------------------------------
  ウィンドウアイコンを設定する
 ---------------------------------------------------------------------*/
static VALUE Window_loadIcon( VALUE obj, VALUE vfilename )
{
    VALUE vsjisstr;
    Check_Type( vfilename, T_STRING );

    if( rb_enc_get_index( vfilename ) != 0 )
    {
        vsjisstr = rb_str_export_to_enc( vfilename, g_enc_sys );
    }
    else
    {
        vsjisstr = vfilename;
    }

    /* アイコンリソース解放 */
    if( g_WindowInfo.hIcon != 0 )
    {
        DestroyIcon(g_WindowInfo.hIcon);
    }

    g_WindowInfo.hIcon = LoadImage( 0, RSTRING_PTR(vsjisstr), IMAGE_ICON, 16, 16, LR_DEFAULTSIZE|LR_LOADFROMFILE );
    if( g_WindowInfo.hIcon == NULL )
    {
        rb_raise( eDXRubyError, "failed to load the icon image file - Window_loadIcon" );
    }

    SendMessage(g_hWnd, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)g_WindowInfo.hIcon);

    return Qnil;
}


/*--------------------------------------------------------------------
  ファイルオープンダイアログを表示する
 ---------------------------------------------------------------------*/
static VALUE Window_openDialog(VALUE obj, VALUE vfilter, VALUE vtitle)
{
    OPENFILENAME OFN; 
    char buf[MAX_PATH*2];
    VALUE filter;
    VALUE vsjisstr;
    buf[0] = 0;

    Check_Type(vfilter, T_ARRAY);
    Check_Type(vtitle, T_STRING);

    ZeroMemory(&OFN,sizeof(OPENFILENAME));
    OFN.lStructSize = sizeof(OPENFILENAME); 
    OFN.hwndOwner = g_hWnd;

    {
        int i;
        VALUE base_array = rb_ary_new();
        for( i = 0; i < RARRAY_LEN( vfilter ); i++ )
        {
            VALUE child_array = rb_ary_new();
            Check_Type(RARRAY_PTR( vfilter )[i], T_ARRAY);
            rb_ary_push( base_array, child_array );
            if( rb_enc_get_index( rb_ary_entry( RARRAY_PTR( vfilter )[i], 0) ) != 0 )
            {
                vsjisstr = rb_str_export_to_enc( rb_ary_entry( RARRAY_PTR( vfilter )[i], 0 ), g_enc_sys );
            }
            else
            {
                vsjisstr = rb_ary_entry( RARRAY_PTR( vfilter )[i], 0);
            }
            rb_ary_push( child_array, vsjisstr );
            if( rb_enc_get_index( rb_ary_entry( RARRAY_PTR( vfilter )[i], 1) ) != 0 )
            {
                vsjisstr = rb_str_export_to_enc( rb_ary_entry( RARRAY_PTR( vfilter )[i], 1 ), g_enc_sys );
            }
            else
            {
                vsjisstr = rb_ary_entry( RARRAY_PTR( vfilter )[i], 1);
            }
            rb_ary_push( child_array, vsjisstr );
        }
        filter = rb_ary_join( base_array, rb_str_new( "\0", 1 ) );
        filter = rb_str_cat( filter, "\0", 1 );
        OFN.lpstrFilter = RSTRING_PTR( filter );
    }

    OFN.lpstrFile = buf;
    OFN.nMaxFile = MAX_PATH*2;
    OFN.Flags = OFN_FILEMUSTEXIST;

    if( rb_enc_get_index( vtitle ) != 0 )
    {
        vsjisstr = rb_str_export_to_enc( vtitle, g_enc_sys );
    }
    else
    {
        vsjisstr = vtitle;
    }

    OFN.lpstrTitle = RSTRING_PTR(vsjisstr);
    OFN.lpstrDefExt = 0;
    if( !GetOpenFileName(&OFN) )
    {
        return Qnil;
    }

    {
        VALUE venc = rb_enc_default_internal();
        VALUE vstr = rb_str_new2( buf );
        rb_enc_associate( vstr, g_enc_sys );

        if( RTEST(venc) )
        {
            vstr = rb_str_export_to_enc( vstr, rb_default_internal_encoding() );
        }

        return vstr;
    }
}


/*--------------------------------------------------------------------
  ファイルセーブダイアログを表示する
 ---------------------------------------------------------------------*/
static VALUE Window_saveDialog(VALUE obj, VALUE vfilter, VALUE vtitle)
{
    OPENFILENAME OFN; 
    char buf[MAX_PATH*2];
    VALUE filter;
    VALUE vsjisstr;
    buf[0] = 0;

    Check_Type(vfilter, T_ARRAY);
    Check_Type(vtitle, T_STRING);

    ZeroMemory(&OFN,sizeof(OPENFILENAME));
    OFN.lStructSize = sizeof(OPENFILENAME); 
    OFN.hwndOwner = g_hWnd;

    {
        int i;
        VALUE base_array = rb_ary_new();
        for( i = 0; i < RARRAY_LEN( vfilter ); i++ )
        {
            VALUE child_array = rb_ary_new();
            Check_Type(RARRAY_PTR( vfilter )[i], T_ARRAY);
            rb_ary_push( base_array, child_array );
            if( rb_enc_get_index( rb_ary_entry( RARRAY_PTR( vfilter )[i], 0) ) != 0 )
            {
                vsjisstr = rb_str_export_to_enc( rb_ary_entry( RARRAY_PTR( vfilter )[i], 0 ), g_enc_sys );
            }
            else
            {
                vsjisstr = rb_ary_entry( RARRAY_PTR( vfilter )[i], 0);
            }
            rb_ary_push( child_array, vsjisstr );
            if( rb_enc_get_index( rb_ary_entry( RARRAY_PTR( vfilter )[i], 1) ) != 0 )
            {
                vsjisstr = rb_str_export_to_enc( rb_ary_entry( RARRAY_PTR( vfilter )[i], 1 ), g_enc_sys );
            }
            else
            {
                vsjisstr = rb_ary_entry( RARRAY_PTR( vfilter )[i], 1);
            }
            rb_ary_push( child_array, vsjisstr );
        }
        filter = rb_ary_join( base_array, rb_str_new( "\0", 1 ) );
        filter = rb_str_cat( filter, "\0", 1 );
        OFN.lpstrFilter = RSTRING_PTR( filter );
    }

    OFN.lpstrFile = buf;
    OFN.nMaxFile = MAX_PATH*2;
    OFN.Flags = OFN_CREATEPROMPT | OFN_OVERWRITEPROMPT;

    if( rb_enc_get_index( vtitle ) != 0 )
    {
        vsjisstr = rb_str_export_to_enc( vtitle, g_enc_sys );
    }
    else
    {
    vsjisstr = vtitle;
    }

    OFN.lpstrTitle = RSTRING_PTR(vsjisstr);
    OFN.lpstrDefExt = 0;
    if( !GetSaveFileName(&OFN) )
    {
        return Qnil;
    }

    {
        VALUE venc = rb_enc_default_internal();
        VALUE vstr = rb_str_new2( buf );
        rb_enc_associate( vstr, g_enc_sys );

        if( RTEST(venc) )
        {
            vstr = rb_str_export_to_enc( vstr, rb_default_internal_encoding() );
        }
        return vstr;
    }
}


/*--------------------------------------------------------------------
  フォルダ選択ダイアログを表示する
 ---------------------------------------------------------------------*/
int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    if(uMsg==BFFM_INITIALIZED){
        SendMessage(hwnd, BFFM_SETSELECTION, (WPARAM)TRUE, lpData);
    }
    return 0;
}

static VALUE Window_folderDialog( int argc, VALUE *argv, VALUE obj )
{
    BROWSEINFO bi;
    ITEMIDLIST *idl;
    LPMALLOC pMalloc;
    char szTmp[MAX_PATH];
    VALUE vtitle, vpath;

    SHGetMalloc( &pMalloc );

    rb_scan_args( argc, argv, "02", &vtitle, &vpath );


    bi.hwndOwner = g_hWnd;
    bi.pidlRoot = NULL;
    bi.pszDisplayName = szTmp;

    if( vtitle != Qnil )
    {
        Check_Type( vtitle, T_STRING );
        if( rb_enc_get_index( vtitle ) != 0 )
        {
            vtitle = rb_str_export_to_enc( vtitle, g_enc_sys );
        }
        bi.lpszTitle = RSTRING_PTR( vtitle );
    }
    else
    {
        bi.lpszTitle = "";
    }

    if( vpath != Qnil )
    {
        Check_Type( vpath, T_STRING );
        if( rb_enc_get_index( vpath ) != 0 )
        {
            vpath = rb_str_export_to_enc( vpath, g_enc_sys );
        }
        bi.lpfn=&BrowseCallbackProc;
        bi.lParam = (LPARAM)RSTRING_PTR( vpath );
    }
    else
    {
        bi.lpfn=0;
        bi.lParam = 0;
    }

    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_DONTGOBELOWDOMAIN | BIF_NEWDIALOGSTYLE;
    bi.iImage = 0;
    //ダイアログを表示
    idl = SHBrowseForFolder( &bi );
    if( idl == NULL )
    {
        return Qnil;
    }

    SHGetPathFromIDList( idl, szTmp );

    //PIDLを解放する
    pMalloc->lpVtbl->Free( pMalloc, idl );
    pMalloc->lpVtbl->Release( pMalloc );

    {
        VALUE venc = rb_enc_default_internal();
        VALUE vstr = rb_str_new2( szTmp );
        rb_enc_associate( vstr, g_enc_sys );

        if( RTEST(venc) )
        {
            vstr = rb_str_export_to_enc( vstr, rb_default_internal_encoding() );
        }
        return vstr;
    }
}


/*--------------------------------------------------------------------
   スクリーンショットを撮る
 ---------------------------------------------------------------------*/
static VALUE Window_getScreenShot( int argc, VALUE *argv, VALUE obj )
{
	HRESULT hr;
	D3DDISPLAYMODE dmode;
	LPDIRECT3DSURFACE9 pSurface;
	RECT rect;
    VALUE vfilename, vformat, vsjisstr, vdowncase;
    char ext[5][6] = {".jpeg", ".jpg", ".png", ".bmp", ".dds"};
    char len[5] = {4, 3, 3, 3, 3};
    int format[5] = {FORMAT_JPG, FORMAT_JPG, FORMAT_PNG, FORMAT_BMP, FORMAT_DDS};
    int i, j, k, f;
    int dispx;
    int dispy;

    rb_scan_args( argc, argv, "11", &vfilename, &vformat );

    Check_Type( vfilename, T_STRING );

    /* 現在のディスプレイのフォーマットなどを取得 */
	hr = g_pD3D->lpVtbl->GetAdapterDisplayMode(g_pD3D, D3DADAPTER_DEFAULT, &dmode);
	if( FAILED( hr ) )
	{
        rb_raise( eDXRubyError, "Failure to capture - GetAdapterDisplayMode" );
    }

	/* キャプチャ用サーフェス作成 */
	hr = g_pD3DDevice->lpVtbl->CreateOffscreenPlainSurface(g_pD3DDevice,
			dmode.Width,
			dmode.Height,
			D3DFMT_A8R8G8B8,
			D3DPOOL_SCRATCH, &pSurface, NULL);
	if( FAILED( hr ) )
	{
        rb_raise( eDXRubyError, "Failure to capture - CreateOffscreenPlainSurface" );
    }
	/* キャプチャ */
	hr = g_pD3DDevice->lpVtbl->GetFrontBufferData(g_pD3DDevice, 0, pSurface);
	if( FAILED( hr ) )
	{
		RELEASE(pSurface);
        rb_raise( eDXRubyError, "Failure to capture - GetFrontBufferData" );
	}

    /* サーフェスの保存 */
    if( g_D3DPP.Windowed )
    {
        POINT p = { 0, 0 };
        /* ウィンドウ左上の設定 */
        ClientToScreen(g_hWnd  , &p);
        if( p.x < 0 )
        {
            p.x = 0;
        }
        if( p.y < 0 )
        {
            p.y = 0;
        }
        rect.left = p.x; rect.top = p.y;
        /* ウィンドウ右下の設定 */
        p.x = (LONG)(g_D3DPP.BackBufferWidth * g_WindowInfo.scale);
        p.y = (LONG)(g_D3DPP.BackBufferHeight * g_WindowInfo.scale);
        ClientToScreen(g_hWnd, &p);
        dispx = GetSystemMetrics(SM_CXSCREEN);
        dispy = GetSystemMetrics(SM_CYSCREEN);
        if( p.x >= dispx )
        {
            p.x = dispx - 1;
        }
        if( p.y >= dispy )
        {
            p.y = dispy - 1;
        }
        rect.right = p.x; rect.bottom = p.y;
    }
    if( rb_enc_get_index( vfilename ) != 0 )
    {
        vsjisstr = rb_str_export_to_enc( vfilename, rb_default_internal_encoding() );
    }
    else
    {
        vsjisstr = vfilename;
    }

    vdowncase = rb_funcall( vsjisstr, rb_intern("downcase"), 0, Qnil );
    if( vformat == Qnil )
    {
        for( i = 0; i < 5; i++ )
        {
            for( j = RSTRING_LEN(vdowncase) - 1, k = len[i]; k >= 0 && j >= 0; k--, j-- )
            {
                if( RSTRING_PTR(vdowncase)[j] != ext[i][k] )
                {
                    break;
                }
            }

            if( ext[i][k + 1] == '.' && RSTRING_PTR(vdowncase)[j + 1] == '.' )
            {
                f = format[i];
                break;
            }
        }
        if( i == 5 )
        {
            f = FORMAT_PNG;
        }
    }
    else
    {
        f = FIX2INT( vformat );
    }

    hr = D3DXSaveSurfaceToFile(
            RSTRING_PTR( vsjisstr ),                            /* 保存ファイル名 */
            f,                                                  /* ファイルフォーマット */
            pSurface,                                           /* 保存するサーフェス */
            NULL,                                               /* パレット */
            g_D3DPP.Windowed ? &rect : NULL);                   /* 保存領域 */
    RELEASE(pSurface);
	if( FAILED( hr ) )
	{
        rb_raise( eDXRubyError, "Failure to capture - D3DXSaveSurfaceToFile" );
    }

	return obj;
}


///*--------------------------------------------------------------------
//   WindowのバックバッファをImageオブジェクトにして返す
// ---------------------------------------------------------------------*/
//static VALUE Window_to_image( VALUE klass )
//{
//    VALUE vimage;
//    struct DXRubyImage *image;
//    LPDIRECT3DTEXTURE9 pD3DTexture;
//    LPDIRECT3DSURFACE9 pBackBufferSurface;
//    VALUE ary[2];
//    IDirect3DSurface9 *surface;
//    D3DLOCKED_RECT srctrect;
//    D3DLOCKED_RECT dsttrect;
//    int i, j;
//    RECT srect;
//    RECT drect;
//    HRESULT hr;
//    int *psrc;
//    int *pdst;
//    D3DSURFACE_DESC desc;
//
//    vimage = Image_allocate( cImage );
//    ary[0] = INT2FIX( g_WindowInfo.width );
//    ary[1] = INT2FIX( g_WindowInfo.height );
//    Image_initialize( 2, ary, vimage );
//    image = DXRUBY_GET_STRUCT( Image, vimage );
//
//    DXRUBY_RETRY_START;
//    /* テクスチャオブジェクトを作成する */
//    hr = D3DXCreateTexture( g_pD3DDevice, (UINT)g_WindowInfo.width, (UINT)g_WindowInfo.height,
//                                      1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
//                                      &pD3DTexture);
//    DXRUBY_RETRY_END;
//    if( FAILED( hr ) ) rb_raise( eDXRubyError, "テクスチャの作成に失敗しました - Window_to_image" );
//
//    /* テクスチャのサーフェイスを取得 */
//    hr = pD3DTexture->lpVtbl->GetSurfaceLevel( pD3DTexture, 0, &surface );
//    if( FAILED( hr ) ) rb_raise( eDXRubyError, "サーフェイスの作成に失敗しました - Window_to_image" );
//
//    /* レンダーターゲットのイメージ取得 */
//    hr = g_pD3DDevice->lpVtbl->GetBackBuffer( g_pD3DDevice, 0, 0, D3DBACKBUFFER_TYPE_MONO , &surface );
//    if( FAILED( hr ) ) rb_raise( eDXRubyError, "イメージの取得に失敗しました - Window_to_image" );
//
//    /* イメージのコピー */
//    srect.left = 0;
//    srect.top = 0;
//    srect.right = g_WindowInfo.width;
//    srect.bottom = g_WindowInfo.height;
//    drect.left = 0;
//    drect.top = 0;
//    drect.right = g_WindowInfo.width;
//    drect.bottom = g_WindowInfo.height;
//
//    hr = pD3DTexture->lpVtbl->LockRect( pD3DTexture, 0, &srctrect, &srect, D3DLOCK_READONLY );
//    hr = image->texture->pD3DTexture->lpVtbl->LockRect( image->texture->pD3DTexture, 0, &dsttrect, &drect, 0 );
//
//    for( i = 0; i < g_WindowInfo.height; i++)
//    {
//        psrc = (int*)((char *)srctrect.pBits + i * srctrect.Pitch);
//        pdst = (int*)((char *)dsttrect.pBits + i * dsttrect.Pitch);
//        for( j = 0; j < g_WindowInfo.width; j++)
//        {
//            *(pdst++) = *(psrc++);
//        }
//    }
//
//    pD3DTexture->lpVtbl->UnlockRect( pD3DTexture, 0 );
//    image->texture->pD3DTexture->lpVtbl->UnlockRect( image->texture->pD3DTexture, 0 );
//
//    RELEASE( surface );
//    RELEASE( pD3DTexture );
//
//    return vimage;
//}


/*--------------------------------------------------------------------
   使用可能なスクリーンサイズを取得する
 ---------------------------------------------------------------------*/
static VALUE Window_getScreenModes( VALUE obj )
{
    D3DDISPLAYMODE d3ddm;
    int count , max;
    volatile VALUE ary, temp;

    ary = rb_ary_new();
    max = g_pD3D->lpVtbl->GetAdapterModeCount( g_pD3D, D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8 );

    for( count = 0 ; count < max ; count++ )
    {
        temp = rb_ary_new();
        g_pD3D->lpVtbl->EnumAdapterModes( g_pD3D, D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8 , count , &d3ddm);
        rb_ary_push( temp, INT2NUM(d3ddm.Width) );
        rb_ary_push( temp, INT2NUM(d3ddm.Height) );
        rb_ary_push( temp, INT2NUM(d3ddm.RefreshRate) );
        rb_ary_push( ary, temp );
    }

    rb_funcall( ary, rb_intern("uniq!"), 0 );

    return ary;
}


/*--------------------------------------------------------------------
   現在のスクリーンサイズを取得する
 ---------------------------------------------------------------------*/
static VALUE Window_getCurrentMode( VALUE obj )
{
    D3DDISPLAYMODE d3ddm;
    int count , max;
    volatile VALUE ary;

    ary = rb_ary_new();
    g_pD3D->lpVtbl->GetAdapterDisplayMode( g_pD3D, D3DADAPTER_DEFAULT , &d3ddm );

    rb_ary_push( ary, INT2NUM(d3ddm.Width) );
    rb_ary_push( ary, INT2NUM(d3ddm.Height) );
    rb_ary_push( ary, INT2NUM(d3ddm.RefreshRate) );

    return ary;
}


/*--------------------------------------------------------------------
   before_callハッシュを返す
 ---------------------------------------------------------------------*/
static VALUE Window_before_call( VALUE obj )
{
    return g_WindowInfo.before_call;
}

/*--------------------------------------------------------------------
   after_callハッシュを返す
 ---------------------------------------------------------------------*/
static VALUE Window_after_call( VALUE obj )
{
    return g_WindowInfo.after_call;
}


#ifdef DXRUBY15
/*--------------------------------------------------------------------
   デバイスロストのような動作を再現するテストメソッド
 ---------------------------------------------------------------------*/
static VALUE Window_test_device_lost( VALUE obj )
{
    int i;

    for( i = 0; i < g_RenderTargetList.count; i++ )
    {
        struct DXRubyRenderTarget *rt = (struct DXRubyRenderTarget *)g_RenderTargetList.pointer[i];
        g_pD3DDevice->lpVtbl->SetRenderTarget( g_pD3DDevice, 0, rt->surface );
        g_pD3DDevice->lpVtbl->Clear( g_pD3DDevice, 0, NULL, D3DCLEAR_TARGET,
                                     D3DCOLOR_ARGB( 255, 0, 0, 0 ), 1.0f, 0 );

        // 再生成Procが設定されている場合
        if( rt->vregenerate_proc != Qnil )
        {
            rb_funcall( rt->vregenerate_proc, SYM2ID( symbol_call ), 0 );
        }
    }

    return Qnil;
}
#endif


/*********************************************************************
 * ShaderCoreクラス
 *
 * シェーダプログラムを管理する。
 *********************************************************************/

static void AddShaderCoreList( struct DXRubyShaderCore *core )
{
    if( g_ShaderCoreList.allocate_size <= g_ShaderCoreList.count )
    {
        g_ShaderCoreList.allocate_size = g_ShaderCoreList.allocate_size * 3 / 2; /* 1.5倍にする */
        g_ShaderCoreList.pointer = realloc( g_ShaderCoreList.pointer, sizeof( void* ) * g_ShaderCoreList.allocate_size );
    }

    g_ShaderCoreList.pointer[g_ShaderCoreList.count] = core;
    g_ShaderCoreList.count++;
}
static void DeleteShaderCoreList( struct DXRubyShaderCore *rt )
{
    int i;

    for( i = 0; i < g_ShaderCoreList.count; i++ )
    {
        if( g_ShaderCoreList.pointer[i] == rt )
        {
            break;
        }
    }
    if( i == g_ShaderCoreList.count )
    {
        rb_raise( eDXRubyError, "Internal error - DeleteShaderCoreList" );
    }

    i++;
    for( ; i < g_ShaderCoreList.count; i++ )
    {
        g_ShaderCoreList.pointer[i - 1] = g_ShaderCoreList.pointer[i];
    }

    g_ShaderCoreList.count--;
}

/*--------------------------------------------------------------------
   参照されなくなったときにGCから呼ばれる関数
 ---------------------------------------------------------------------*/
static void ShaderCore_free( struct DXRubyShaderCore *core)
{
    RELEASE( core->pD3DXEffect );
    DeleteShaderCoreList( core );
    core->vtype = Qnil;
}
void ShaderCore_release( struct DXRubyShaderCore *core )
{
    if( core->pD3DXEffect )
    {
        ShaderCore_free( core );
    }
    free( core );
}

/*--------------------------------------------------------------------
   GCから呼ばれるマーク関数
 ---------------------------------------------------------------------*/
static void ShaderCore_mark( struct DXRubyShaderCore* core )
{
    rb_gc_mark( core->vtype );
}

#ifdef DXRUBY_USE_TYPEDDATA
const rb_data_type_t ShaderCore_data_type = {
    "ShaderCore",
    {
    ShaderCore_mark,
    ShaderCore_release,
    0,
    },
    NULL, NULL
};
#endif

/*--------------------------------------------------------------------
   ShaderCoreクラスのdispose。
 ---------------------------------------------------------------------*/
static VALUE ShaderCore_dispose( VALUE self )
{
    struct DXRubyShaderCore *core = DXRUBY_GET_STRUCT( ShaderCore, self );
    DXRUBY_CHECK_DISPOSE( core, pD3DXEffect );
    ShaderCore_free( core );
    return Qnil;
}

/*--------------------------------------------------------------------
   ShaderCoreクラスのdisposed?。
 ---------------------------------------------------------------------*/
static VALUE ShaderCore_check_disposed( VALUE self )
{
    if( DXRUBY_GET_STRUCT( ShaderCore, self )->pD3DXEffect == NULL )
    {
        return Qtrue;
    }

    return Qfalse;
}

/*--------------------------------------------------------------------
   ShaderCoreクラスのallocate。メモリを確保する為にinitialize前に呼ばれる。
 ---------------------------------------------------------------------*/
static VALUE ShaderCore_allocate( VALUE klass )
{
    VALUE obj;
    struct DXRubyShaderCore *core;

    /* DXRubyShaderCoreのメモリ取得＆ShaderCoreオブジェクト生成 */
    core = malloc( sizeof( struct DXRubyShaderCore ) );
    if( core == NULL ) rb_raise( eDXRubyError, "Out of memory - ShaderCore_allocate" );
#ifdef DXRUBY_USE_TYPEDDATA
    obj = TypedData_Wrap_Struct( klass, &ShaderCore_data_type, core );
#else
    obj = Data_Wrap_Struct( klass, ShaderCore_mark, ShaderCore_release, core );
#endif
    /* とりあえず各オブジェクトはNULLにしておく */
    core->pD3DXEffect = NULL;
    core->vtype = Qnil;

    return obj;
}

/*--------------------------------------------------------------------
   ShaderCoreクラスのgetParam。
 ---------------------------------------------------------------------*/
static VALUE ShaderCore_getParam( VALUE self )
{
    struct DXRubyShaderCore *core = DXRUBY_GET_STRUCT( ShaderCore, self );
    DXRUBY_CHECK_DISPOSE( core, pD3DXEffect );
    return core->vtype;
}


/*--------------------------------------------------------------------
   ShaderCoreクラスのInitialize
 ---------------------------------------------------------------------*/
 static VALUE ShaderCore_initialize( int argc, VALUE *argv, VALUE self )
{
    struct DXRubyShaderCore *core = DXRUBY_GET_STRUCT( ShaderCore, self );
    LPD3DXBUFFER pErr=NULL;
    VALUE vhlsl, vparam;

    rb_scan_args( argc, argv, "11", &vhlsl, &vparam );

    Check_Type( vhlsl, T_STRING );

    if( vparam == Qnil )
    {
        vparam = rb_hash_new();
    }
    else
    {
        Check_Type( vparam, T_HASH );
    }

    if( FAILED( D3DXCreateEffect(
        g_pD3DDevice, RSTRING_PTR( vhlsl ), RSTRING_LEN( vhlsl ), NULL, NULL,
        0 , NULL, &core->pD3DXEffect, &pErr )))
    {
        // シェーダの読み込みの失敗
//        rb_raise( eDXRubyError, pErr->lpVtbl->GetBufferPointer( pErr ) );
        rb_raise( eDXRubyError, pErr ? pErr->lpVtbl->GetBufferPointer( pErr ) : "D3DXCreateEffect failed");
    }
    RELEASE( pErr );

    core->vtype = vparam;
    rb_hash_aset( vparam, symbol_technique, symbol_technique );

    AddShaderCoreList( core );
    return self;
}


/*********************************************************************
 * Shaderクラス
 *
 * ShaderCoreオブジェクトとシェーダパラメータを関連付けて管理する。
 *********************************************************************/

/*--------------------------------------------------------------------
   参照されなくなったときにGCから呼ばれる関数
 ---------------------------------------------------------------------*/
void Shader_release( struct DXRubyShader *shader )
{
    free( shader );
}

/*--------------------------------------------------------------------
   GCから呼ばれるマーク関数
 ---------------------------------------------------------------------*/
static void Shader_mark( struct DXRubyShader *shader )
{
    rb_gc_mark( shader->vcore );
    rb_gc_mark( shader->vparam );
    rb_gc_mark( shader->vname );
}

#ifdef DXRUBY_USE_TYPEDDATA
const rb_data_type_t Shader_data_type = {
    "Shader",
    {
    Shader_mark,
    Shader_release,
    0,
    },
    NULL, NULL
};
#endif

/*--------------------------------------------------------------------
   Shaderクラスのallocate。メモリを確保する為にinitialize前に呼ばれる。
 ---------------------------------------------------------------------*/
static VALUE Shader_allocate( VALUE klass )
{
    VALUE obj;
    struct DXRubyShader *shader;

    /* DXRubyShaderのメモリ取得＆ShaderCoreオブジェクト生成 */
    shader = malloc( sizeof( struct DXRubyShader ) );
    if( shader == NULL ) rb_raise( eDXRubyError, "Out of memory - Shader_allocate" );
#ifdef DXRUBY_USE_TYPEDDATA
    obj = TypedData_Wrap_Struct( klass, &Shader_data_type, shader );
#else
    obj = Data_Wrap_Struct( klass, Shader_mark, Shader_release, shader );
#endif

    /* とりあえず各オブジェクトはNULLにしておく */
    shader->vcore = Qnil;
    shader->vparam = Qnil;
    shader->vname = Qnil;

    return obj;
}

/* 動的生成したGetter */
static VALUE Shader_ref( VALUE self )
{
    struct DXRubyShader *shader = DXRUBY_GET_STRUCT( Shader, self );
    return hash_lookup( shader->vparam, ID2SYM( rb_frame_this_func() ) );
}

/* 動的生成したSetter */
static VALUE Shader_set( VALUE self, VALUE val )
{
    struct DXRubyShader *shader = DXRUBY_GET_STRUCT( Shader, self );
    rb_hash_aset( shader->vparam, hash_lookup( shader->vname,  ID2SYM( rb_frame_this_func() ) ), val );
    return val;
}

/*--------------------------------------------------------------------
   ShaderクラスのInitialize
 ---------------------------------------------------------------------*/
static int Shader_foreach( VALUE key, VALUE value, VALUE self )
{
    struct DXRubyShader *shader = DXRUBY_GET_STRUCT( Shader, self );
    if (key == Qundef) return ST_CONTINUE;
//    rb_hash_aset( shader->vparam, key, Qnil );
    if( TYPE(key) == T_SYMBOL )
    {
        rb_hash_aset( shader->vname, ID2SYM(rb_id_attrset(SYM2ID(key))), key );
        rb_define_singleton_method( self, rb_id2name( SYM2ID( key ) ), Shader_ref, 0 );
        rb_define_singleton_method( self, rb_id2name( rb_id_attrset( SYM2ID( key ) ) ), Shader_set, 1 );
    }
    else if( TYPE(key) == T_STRING )
    {
        rb_hash_aset( shader->vname, ID2SYM(rb_id_attrset(rb_intern( RSTRING_PTR(key) ))), ID2SYM(rb_intern( RSTRING_PTR(key) )) );
        rb_define_singleton_method( self, rb_id2name( rb_intern( RSTRING_PTR(key) ) ), Shader_ref, 0 );
        rb_define_singleton_method( self, rb_id2name( rb_id_attrset( rb_intern( RSTRING_PTR(key) ) ) ), Shader_set, 1 );
    }
    return ST_CONTINUE;
}

static VALUE Shader_initialize( int argc, VALUE *argv, VALUE self )
{
    struct DXRubyShaderCore *core;
    struct DXRubyShader *shader;
    VALUE vcore, vtech;

    rb_scan_args( argc, argv, "11", &vcore, &vtech );

    DXRUBY_CHECK_TYPE( ShaderCore, vcore );
    core = DXRUBY_GET_STRUCT( ShaderCore, vcore );
    DXRUBY_CHECK_DISPOSE( core, pD3DXEffect );
    
    shader = DXRUBY_GET_STRUCT( Shader, self );

    shader->vcore = vcore;
    shader->vparam = rb_hash_new();
    shader->vname = rb_hash_new();

    rb_hash_foreach( core->vtype, Shader_foreach, self );

    rb_hash_aset( shader->vparam, symbol_technique, vtech );

    return self;
}

/* technique名取得 */
static VALUE Shader_getTechnique( VALUE self )
{
    struct DXRubyShader *shader = DXRUBY_GET_STRUCT( Shader, self );
    return hash_lookup( shader->vparam, symbol_technique );
}

/* technique名設定 */
static VALUE Shader_setTechnique( VALUE self, VALUE vtech )
{
    struct DXRubyShader *shader = DXRUBY_GET_STRUCT( Shader, self );
    Check_Type( vtech, T_STRING );
    rb_hash_aset( shader->vparam, symbol_technique, vtech );
    return vtech;
}


/*********************************************************************
 * RenderTargetクラス
 *
 * レンダーターゲットになれるImageクラス。
 * 直接編集機能は無いが、draw系メソッドでハードウェア描画ができる。
 *********************************************************************/

static void AddRenderTargetList( struct DXRubyRenderTarget *rt )
{
    if( g_RenderTargetList.allocate_size <= g_RenderTargetList.count )
    {
        g_RenderTargetList.allocate_size = g_RenderTargetList.allocate_size * 3 / 2; /* 1.5倍にする */
        g_RenderTargetList.pointer = realloc( g_RenderTargetList.pointer, sizeof( void* ) * g_RenderTargetList.allocate_size );
    }

    g_RenderTargetList.pointer[g_RenderTargetList.count] = rt;
    g_RenderTargetList.count++;
}

static void DeleteRenderTargetList( struct DXRubyRenderTarget *rt )
{
    int i;

    for( i = 0; i < g_RenderTargetList.count; i++ )
    {
        if( g_RenderTargetList.pointer[i] == rt )
        {
            g_RenderTargetList.pointer[i] = NULL;
            break;
        }
    }
}

static void CleanRenderTargetList( void )
{
    int i, j;

    for( i = 0, j = 0; j < g_RenderTargetList.count; )
    {
        if( g_RenderTargetList.pointer[j] == NULL )
        {
            j++;
        }
        else
        {
            g_RenderTargetList.pointer[i] = g_RenderTargetList.pointer[j];
            i++;
            j++;
        }
    }

    g_RenderTargetList.count = i;
}


/*--------------------------------------------------------------------
   参照されなくなったときにGCから呼ばれる関数
 ---------------------------------------------------------------------*/
static void RenderTarget_free( struct DXRubyRenderTarget *rt )
{
    RELEASE( rt->surface );
    if( rt->texture != NULL )
    {
        RELEASE( rt->texture->pD3DTexture );
        free( rt->texture );
        rt->texture = NULL;
    };
    free( rt->PictureList );
    free( rt->PictureStruct );
    DeleteRenderTargetList( rt );
    rt->PictureCount = 0;
    rt->PictureSize = 0;
    rt->PictureDecideCount = 0;
    rt->PictureDecideSize = 0;
#ifdef DXRUBY15
    rt->vregenerate_proc = Qnil;
#endif
}

void RenderTarget_release( struct DXRubyRenderTarget *rt )
{
    /* テクスチャオブジェクトの開放 */
    if( rt->texture )
    {
        RenderTarget_free( rt );
    }
    free( rt );
    rt = NULL;

    g_iRefAll--;
    if( g_iRefAll == 0 )
    {
        CoUninitialize();
    }
}

/*--------------------------------------------------------------------
   RenderTargetクラスのmark
 ---------------------------------------------------------------------*/
static void RenderTarget_mark( struct DXRubyRenderTarget *rt )
{
    int i;

    for( i = 0; i < rt->PictureCount; i++ )
    {
        rb_gc_mark( rt->PictureList[i].picture->value );
    }

#ifdef DXRUBY15
    rb_gc_mark( rt->vregenerate_proc );
#endif
}

#ifdef DXRUBY_USE_TYPEDDATA
const rb_data_type_t RenderTarget_data_type = {
    "RenderTarget",
    {
    RenderTarget_mark,
    RenderTarget_release,
    0,
    },
    NULL, NULL
};
#endif

/*--------------------------------------------------------------------
   RenderTargetクラスのdispose。
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_dispose( VALUE self )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
    DXRUBY_CHECK_DISPOSE( rt, surface );
    RenderTarget_free( rt );
    return Qnil;
}

/*--------------------------------------------------------------------
   RenderTargetクラスのdisposed?。
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_check_disposed( VALUE self )
{
    if( DXRUBY_GET_STRUCT( RenderTarget, self )->surface == NULL )
    {
        return Qtrue;
    }

    return Qfalse;
}

/*--------------------------------------------------------------------
 RenderTargetだった場合に描画予約があればupdateする(内部用)
 ---------------------------------------------------------------------*/
static void RenderTerget_auto_update( VALUE vrt )
{
    if( DXRUBY_CHECK( RenderTarget, vrt ) )
    {
        struct DXRubyRenderTarget *src_rt = DXRUBY_GET_STRUCT( RenderTarget, vrt );

        if( src_rt->clearflag == 0 && src_rt->PictureCount == 0 )
        {
            g_pD3DDevice->lpVtbl->SetRenderTarget( g_pD3DDevice, 0, src_rt->surface );
            g_pD3DDevice->lpVtbl->Clear( g_pD3DDevice, 0, NULL, D3DCLEAR_TARGET,
                                         D3DCOLOR_ARGB( src_rt->a, src_rt->r, src_rt->g, src_rt->b ), 1.0f, 0 );
            src_rt->clearflag = 1;
        }
        else if( src_rt->PictureCount > 0 )
        {
            RenderTarget_update( vrt );
        }
    }
}

/*--------------------------------------------------------------------
   RenderTargetクラスのallocate。メモリを確保する為にinitialize前に呼ばれる。
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_allocate( VALUE klass )
{
    VALUE obj;
    struct DXRubyRenderTarget *rt;

    /* DXRubyRenderTargetのメモリ取得＆RenderTargetオブジェクト生成 */
    rt = malloc(sizeof(struct DXRubyRenderTarget));
    if( rt == NULL ) rb_raise( eDXRubyError, "メモリの取得に失敗しました - RenderTarget_allocate" );
#ifdef DXRUBY_USE_TYPEDDATA
    obj = TypedData_Wrap_Struct( klass, &RenderTarget_data_type, rt );
#else
    obj = Data_Wrap_Struct(klass, RenderTarget_mark, RenderTarget_release, rt);
#endif
    /* とりあえずテクスチャオブジェクトはNULLにしておく */
    rt->texture = NULL;
    rt->surface = NULL;

    /* ピクチャ構造体の初期値設定 */
    rt->PictureCount = 0;
    rt->PictureAllocateCount = 128;
    rt->PictureSize = 0;
    rt->PictureAllocateSize = 128*32;
    rt->PictureList = malloc( rt->PictureAllocateCount * sizeof(struct DXRubyPictureList) );
    rt->PictureStruct = malloc( rt->PictureAllocateSize );
    rt->PictureDecideCount = 0;
    rt->PictureDecideSize = 0;

    rt->minfilter = D3DTEXF_LINEAR;
    rt->magfilter = D3DTEXF_LINEAR;

    rt->a = 0;
    rt->r = 0;
    rt->g = 0;
    rt->b = 0;
#ifdef DXRUBY15
    rt->vregenerate_proc = Qnil;
#endif
    rt->clearflag = 0;
    rt->ox = 0;
    rt->oy = 0;

//    rt->lockcount = 0;

    return obj;
}


/*--------------------------------------------------------------------
   RenderTargetクラスのInitialize
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_initialize( int argc, VALUE *argv, VALUE self )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
    struct DXRubyTexture *texture;
    HRESULT hr;
    D3DSURFACE_DESC desc;
    VALUE vwidth, vheight, vary;
    int width, height;

    g_iRefAll++;

    rb_scan_args( argc, argv, "21", &vwidth, &vheight, &vary );

    width = NUM2INT( vwidth );
    height = NUM2INT( vheight );

    if( width <= 0 || height <= 0 )
    {
        rb_raise( eDXRubyError, "Argument error(width<=0 or height<=0) - RenderTarget_initialize" );
    }

    /* テクスチャメモリ取得 */
    texture = (struct  DXRubyTexture *)malloc( sizeof( struct DXRubyTexture ) );

    if( texture == NULL )
    {
        rb_raise( eDXRubyError, "Out of memory - RenderTarget_initialize" );
    }

    DXRUBY_RETRY_START;
    /* テクスチャオブジェクトを作成する */
    hr = D3DXCreateTexture( g_pD3DDevice, width, height,
                                      1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                      &texture->pD3DTexture);
    DXRUBY_RETRY_END;
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Create texture failed - RenderTarget_initialize" );
    }

    texture->pD3DTexture->lpVtbl->GetSurfaceLevel( texture->pD3DTexture, 0, &rt->surface );
    texture->pD3DTexture->lpVtbl->GetLevelDesc( texture->pD3DTexture, 0, &desc );
    texture->width = (float)desc.Width;
    texture->height = (float)desc.Height;

    rt->texture = texture;
    rt->x = 0;
    rt->y = 0;
    rt->width = width;
    rt->height = height;
    if( vary != Qnil )
    {
        Check_Type( vary, T_ARRAY );
        if( RARRAY_LEN( vary ) == 4 )
        {
            rt->a = NUM2INT( rb_ary_entry(vary, 0) );
            rt->r = NUM2INT( rb_ary_entry(vary, 1) );
            rt->g = NUM2INT( rb_ary_entry(vary, 2) );
            rt->b = NUM2INT( rb_ary_entry(vary, 3) );
        }
        else
        {
            rt->a = 255;
            rt->r = NUM2INT( rb_ary_entry(vary, 0) );
            rt->g = NUM2INT( rb_ary_entry(vary, 1) );
            rt->b = NUM2INT( rb_ary_entry(vary, 2) );
        }
    }

    AddRenderTargetList( rt );

//    g_pD3DDevice->lpVtbl->SetRenderTarget( g_pD3DDevice, 0, rt->surface );
//    g_pD3DDevice->lpVtbl->Clear( g_pD3DDevice, 0, NULL, D3DCLEAR_TARGET,
//                                 D3DCOLOR_ARGB( rt->a, rt->r, rt->g, rt->b ), 1.0f, 0 );

    return self;
}


/*--------------------------------------------------------------------
   RenderTargetクラスのResize
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_resize( VALUE self, VALUE vwidth, VALUE vheight )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
    struct DXRubyTexture *texture;
    HRESULT hr;
    D3DSURFACE_DESC desc;
    int width, height;

    width = NUM2INT( vwidth );
    height = NUM2INT( vheight );

    if( width <= 0 || height <= 0 )
    {
        rb_raise( eDXRubyError, "Argument error(width<=0 or height<=0) - RenderTarget_resize" );
    }

    texture = rt->texture;
    RELEASE( rt->surface );
    RELEASE( texture->pD3DTexture );

    DXRUBY_RETRY_START;
    /* テクスチャオブジェクトを作成する */
    hr = D3DXCreateTexture( g_pD3DDevice, width, height,
                                      1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                      &texture->pD3DTexture);
    DXRUBY_RETRY_END;
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Create texture failed - RenderTarget_resize" );
    }

    texture->pD3DTexture->lpVtbl->GetSurfaceLevel( texture->pD3DTexture, 0, &rt->surface );
    texture->pD3DTexture->lpVtbl->GetLevelDesc( texture->pD3DTexture, 0, &desc );
    texture->width = (float)desc.Width;
    texture->height = (float)desc.Height;

    rt->texture = texture;
    rt->x = 0;
    rt->y = 0;
    rt->width = width;
    rt->height = height;
    rt->clearflag = 0;
    rt->PictureCount = 0;
    rt->PictureSize = 0;
    rt->PictureDecideCount = 0;
    rt->PictureDecideSize = 0;

    return self;
}


/*--------------------------------------------------------------------
   RenderTargetクラスのImageオブジェクト化
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_to_image( VALUE self )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
    VALUE vimage;
    struct DXRubyImage *image;
    LPDIRECT3DTEXTURE9 pD3DTexture;
    VALUE ary[2];
    IDirect3DSurface9 *surface;
    D3DLOCKED_RECT srctrect;
    D3DLOCKED_RECT dsttrect;
    int i, j;
    RECT srect;
    RECT drect;
    HRESULT hr;
    int *psrc;
    int *pdst;
    D3DSURFACE_DESC desc;

    DXRUBY_CHECK_DISPOSE( rt, surface );

    /* 描画予約があればupdateする */
    if( rt->clearflag == 0 && rt->PictureCount == 0 )
    {
        g_pD3DDevice->lpVtbl->SetRenderTarget( g_pD3DDevice, 0, rt->surface );
        g_pD3DDevice->lpVtbl->Clear( g_pD3DDevice, 0, NULL, D3DCLEAR_TARGET,
                                     D3DCOLOR_ARGB( rt->a, rt->r, rt->g, rt->b ), 1.0f, 0 );
        rt->clearflag = 1;
    }
    else if( rt->PictureCount > 0 )
    {
        RenderTarget_update( self );
    }

    vimage = Image_allocate( cImage );
    ary[0] = INT2FIX( rt->width );
    ary[1] = INT2FIX( rt->height );
    Image_initialize( 2, ary, vimage );
    image = DXRUBY_GET_STRUCT( Image, vimage );

    DXRUBY_RETRY_START;
    /* テクスチャオブジェクトを作成する */
    hr = D3DXCreateTexture( g_pD3DDevice, (UINT)rt->texture->width, (UINT)rt->texture->height,
                                      1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                                      &pD3DTexture);
    DXRUBY_RETRY_END;
    if( FAILED( hr ) ) rb_raise( eDXRubyError, "Create texture failed - RenderTarget_to_image" );

    /* テクスチャのサーフェイスを取得 */
    hr = pD3DTexture->lpVtbl->GetSurfaceLevel( pD3DTexture, 0, &surface );
    if( FAILED( hr ) ) rb_raise( eDXRubyError, "Get surface failed - RenderTarget_to_image" );

    /* レンダーターゲットのイメージ取得 */
    hr = g_pD3DDevice->lpVtbl->GetRenderTargetData( g_pD3DDevice, rt->surface, surface );
    if( FAILED( hr ) ) rb_raise( eDXRubyError, "Get image data failed - RenderTarget_to_image" );

    /* イメージのコピー */
    srect.left = rt->x;
    srect.top = rt->y;
    srect.right = rt->width;
    srect.bottom = rt->height;
    drect.left = 0;
    drect.top = 0;
    drect.right = rt->width;
    drect.bottom = rt->height;

    hr = pD3DTexture->lpVtbl->LockRect( pD3DTexture, 0, &srctrect, &srect, D3DLOCK_READONLY );
    hr = image->texture->pD3DTexture->lpVtbl->LockRect( image->texture->pD3DTexture, 0, &dsttrect, &drect, 0 );

    for( i = 0; i < rt->height; i++)
    {
        psrc = (int*)((char *)srctrect.pBits + i * srctrect.Pitch);
        pdst = (int*)((char *)dsttrect.pBits + i * dsttrect.Pitch);
        for( j = 0; j < rt->width; j++)
        {
            *(pdst++) = *(psrc++);
        }
    }

    pD3DTexture->lpVtbl->UnlockRect( pD3DTexture, 0 );
    image->texture->pD3DTexture->lpVtbl->UnlockRect( image->texture->pD3DTexture, 0 );

    RELEASE( surface );
    RELEASE( pD3DTexture );

    return vimage;
}


/*--------------------------------------------------------------------
   レンダーターゲットのサイズ（幅）を返す。
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_getWidth( VALUE self )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
    DXRUBY_CHECK_DISPOSE( rt, surface );
    return INT2FIX( rt->width );
}


/*--------------------------------------------------------------------
   レンダーターゲットのサイズ（高さ）を返す。
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_getHeight( VALUE self )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
    DXRUBY_CHECK_DISPOSE( rt, surface );
    return INT2FIX( rt->height );
}


/*--------------------------------------------------------------------
   レンダーターゲットの描画開始位置xを返す。
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_getOx( VALUE self )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
    DXRUBY_CHECK_DISPOSE( rt, surface );
    return INT2FIX( rt->ox );
}


/*--------------------------------------------------------------------
   レンダーターゲットの描画開始位置yを返す。
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_getOy( VALUE self )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
    DXRUBY_CHECK_DISPOSE( rt, surface );
    return INT2FIX( rt->oy );
}


/*--------------------------------------------------------------------
   レンダーターゲットの描画開始位置xを設定する。
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_setOx( VALUE self, VALUE vox )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
    DXRUBY_CHECK_DISPOSE( rt, surface );
    rt->ox = NUM2INT( vox );
    return vox;
}


/*--------------------------------------------------------------------
   レンダーターゲットの描画開始位置yを設定する。
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_setOy( VALUE self, VALUE voy )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
    DXRUBY_CHECK_DISPOSE( rt, surface );
    rt->oy = NUM2INT( voy );
    return voy;
}


/*--------------------------------------------------------------------
   ピクチャリストのメモリ確保
 ---------------------------------------------------------------------*/
void *RenderTarget_AllocPictureList( struct DXRubyRenderTarget *rt, int size )
{
    void* result = rt->PictureStruct + rt->PictureSize;
    int i;

    rt->PictureSize += size;

    if( rt->PictureSize > rt->PictureAllocateSize )
    {
        char *temp = rt->PictureStruct;
        rt->PictureAllocateSize = rt->PictureAllocateSize * 3 / 2; /* 1.5倍にする */
        rt->PictureStruct = realloc( rt->PictureStruct, rt->PictureAllocateSize );
        if( rt->PictureStruct == NULL ) rb_raise(eDXRubyError, "Out of memory - RenderTarget_draw");
        for( i = 0; i < rt->PictureCount; i++)
        {
            rt->PictureList[i].picture = (struct DXRubyPicture *)((char *)rt->PictureList[i].picture + ((char*)rt->PictureStruct - temp));
        }
        rt->PictureDecideSize += (char*)rt->PictureStruct - temp;
        result = rt->PictureStruct + rt->PictureSize - size;
    }

    if( rt->PictureCount >= rt->PictureAllocateCount )
    {
        rt->PictureAllocateCount = rt->PictureAllocateCount * 3 / 2; /* 1.5倍にする */
        rt->PictureList = realloc( rt->PictureList, rt->PictureAllocateCount * sizeof(struct DXRubyPictureList) );
        if( rt->PictureList == NULL ) rb_raise(eDXRubyError, "Out of memory - RenderTarget_draw");
    }

    return result;
}


/* マージソート */
void merge( struct DXRubyPictureList *list, struct DXRubyPictureList *temp, int left, int mid, int right )
{
    int left_end, num_elements, tmp_pos;

    left_end = mid - 1;
    tmp_pos = left;
    num_elements = right - left + 1;

    while ((left <= left_end) && (mid <= right))
    {
        if (list[left].z <= list[mid].z)
            temp[tmp_pos++] = list[left++];
        else
            temp[tmp_pos++] = list[mid++];
    }

    while (left <= left_end)
    {
        temp[tmp_pos++] = list[left++];
    }
    while (mid <= right)
    {
        temp[tmp_pos++] = list[mid++];
    }

    while (num_elements--)
    {
        list[right] = temp[right];
        right--;
    }
}


void m_sort( struct DXRubyPictureList *list, struct DXRubyPictureList *temp, int left, int right )
{
    int mid;

    if( right > left )
    {
        mid = (right + left) / 2;
        m_sort( list, temp, left, mid );
        m_sort( list, temp, mid+1, right );

        merge( list, temp, left, mid+1, right );
    }
}


void RenderTarget_SortPictureList( struct DXRubyRenderTarget *rt )
{
    struct DXRubyPictureList *temp;
    int i;

    for( i = 0; i < rt->PictureCount; i++ )
    {
        if( rt->PictureList[i].z != 0.0f )
        {
            temp = malloc( sizeof(struct DXRubyPictureList)* rt->PictureCount );
            m_sort( rt->PictureList, temp, 0, rt->PictureCount - 1 );
            free(temp);
            break;
        }
    }
}


/*--------------------------------------------------------------------
   画面クリア色取得
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_get_bgcolor( VALUE self )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
    DXRUBY_CHECK_DISPOSE( rt, surface );
    return rb_ary_new3( 4, INT2FIX( rt->a ), INT2FIX( rt->r ), INT2FIX( rt->g ), INT2FIX( rt->b ) );
}


/*--------------------------------------------------------------------
   画面クリア色指定
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_set_bgcolor( VALUE self, VALUE array )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
    DXRUBY_CHECK_DISPOSE( rt, surface );

    Check_Type( array, T_ARRAY );

    if( RARRAY_LEN( array ) == 4 )
    {
        rt->a = NUM2INT( rb_ary_entry(array, 0) );
        rt->r = NUM2INT( rb_ary_entry(array, 1) );
        rt->g = NUM2INT( rb_ary_entry(array, 2) );
        rt->b = NUM2INT( rb_ary_entry(array, 3) );
    }
    else
    {
        rt->a = 255;
        rt->r = NUM2INT( rb_ary_entry(array, 0) );
        rt->g = NUM2INT( rb_ary_entry(array, 1) );
        rt->b = NUM2INT( rb_ary_entry(array, 2) );
    }

    return array;
}

void RenderTarget_drawLine_func( struct DXRubyPicture_drawLine *picture )
{
    TLVERTX2 VertexDataTbl[2];

    /* 頂点１と２のx */
    if( picture->x1 == picture->x2 )
    {
        VertexDataTbl[0].x = (float)picture->x1;
        VertexDataTbl[1].x = (float)picture->x2 + 0.5f;
    }
    else if( picture->x1 > picture->x2 )
    {
        VertexDataTbl[0].x = (float)picture->x1 + 1.0f;
        VertexDataTbl[1].x = (float)picture->x2;
    }
    else
    {
        VertexDataTbl[0].x = (float)picture->x1;
        VertexDataTbl[1].x = (float)picture->x2 + 1.0f;
    }

    /* 頂点１と２のy */
    if( picture->y1 == picture->y2 )
    {
        VertexDataTbl[0].y = (float)picture->y1;
        VertexDataTbl[1].y = (float)picture->y2 + 0.5f;
    }
    else if( picture->y1 > picture->y2 )
    {
        VertexDataTbl[0].y = (float)picture->y1 + 1.0f;
        VertexDataTbl[1].y = (float)picture->y2;
    }
    else
    {
        VertexDataTbl[0].y = (float)picture->y1;
        VertexDataTbl[1].y = (float)picture->y2 + 1.0f;
    }

    /* 頂点色 */
    VertexDataTbl[0].color = VertexDataTbl[1].color = picture->col;
    /* Ｚ座標 */
    VertexDataTbl[0].z = VertexDataTbl[1].z = picture->z;

    /* テクスチャをセット */
    g_pD3DDevice->lpVtbl->SetTexture(g_pD3DDevice, 0, (IDirect3DBaseTexture9*)NULL);

    /* デバイスに使用する頂点フォーマットをセット */
    g_pD3DDevice->lpVtbl->SetFVF( g_pD3DDevice, D3DFVF_XYZ | D3DFVF_DIFFUSE );

    /* 描画 */
    g_pD3DDevice->lpVtbl->DrawPrimitiveUP(g_pD3DDevice, D3DPT_LINELIST , 1, VertexDataTbl, sizeof(TLVERTX2));
}

/*--------------------------------------------------------------------
   描画設定（点描画）
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_drawPixel( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyPicture_drawLine *picture;
    float z;
    int col;
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, obj );
    DXRUBY_CHECK_DISPOSE( rt, surface );
//    DXRUBY_CHECK_IMAGE_LOCK( rt );

    if( argc < 3 || argc > 4 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 3, 4 );

    Check_Type( argv[2], T_ARRAY );
    col = array2color( argv[2] );

    picture = (struct DXRubyPicture_drawLine *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_drawLine ) );

    /* DXRubyPictureオブジェクト設定 */
    picture->func = RenderTarget_drawLine_func;
    picture->x1 = NUM2INT( argv[0] ) - rt->ox;
    picture->y1 = NUM2INT( argv[1] ) - rt->oy;
    picture->x2 = NUM2INT( argv[0] ) - rt->ox;
    picture->y2 = NUM2INT( argv[1] ) - rt->oy;
    picture->value = Qnil;
    picture->alpha = 0xff;
    picture->blendflag = 0;
    picture->col = col;

    /* リストデータに追加 */
    rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
    z = argc < 6 || argv[3] == Qnil ? 0.0f : NUM2FLOAT( argv[3] );
    rt->PictureList[rt->PictureCount].z = z;
    picture->z = z;
    rt->PictureCount++;

    return obj;
}

/*--------------------------------------------------------------------
   描画設定（線描画）
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_drawLine( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyPicture_drawLine *picture;
    float z;
    int col;
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, obj );
    DXRUBY_CHECK_DISPOSE( rt, surface );
//    DXRUBY_CHECK_IMAGE_LOCK( rt );

    if( argc < 5 || argc > 6 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 5, 6 );

    Check_Type( argv[4], T_ARRAY );
    col = array2color( argv[4] );

    picture = (struct DXRubyPicture_drawLine *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_drawLine ) );

    /* DXRubyPictureオブジェクト設定 */
    picture->func = RenderTarget_drawLine_func;
    picture->x1 = NUM2INT( argv[0] ) - rt->ox;
    picture->y1 = NUM2INT( argv[1] ) - rt->oy;
    picture->x2 = NUM2INT( argv[2] ) - rt->ox;
    picture->y2 = NUM2INT( argv[3] ) - rt->oy;
    picture->value = Qnil;
    picture->alpha = 0xff;
    picture->blendflag = 0;
    picture->col = col;

    /* リストデータに追加 */
    rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
    z = argc < 6 || argv[5] == Qnil ? 0.0f : NUM2FLOAT( argv[5] );
    rt->PictureList[rt->PictureCount].z = z;
    picture->z = z;
    rt->PictureCount++;

    return obj;
}

/*--------------------------------------------------------------------
   描画設定（Box描画）
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_drawBox( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyPicture_drawLine *picture;
    float z;
    int col;
    int x1, y1, x2, y2;
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, obj );
    DXRUBY_CHECK_DISPOSE( rt, surface );

    if( argc < 5 || argc > 6 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 5, 6 );

    Check_Type( argv[4], T_ARRAY );
    col = array2color( argv[4] );

    x1 = NUM2INT( argv[0] );
    y1 = NUM2INT( argv[1] );
    x2 = NUM2INT( argv[2] );
    y2 = NUM2INT( argv[3] );

    if( x1 == x2 || y1 == y2 ) /* 横もしくは縦の線か、点の場合 */
    {
        picture = (struct DXRubyPicture_drawLine *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_drawLine ));

        /* DXRubyPictureオブジェクト設定 */
        picture->func = RenderTarget_drawLine_func;
        picture->x1 = x1 - rt->ox;
        picture->y1 = y1 - rt->oy;
        picture->x2 = x2 - rt->ox;
        picture->y2 = y2 - rt->oy;
        picture->value = Qnil;
        picture->alpha = 0xff;
        picture->blendflag = 0;
        picture->col = col;

        /* リストデータに追加 */
        rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
        z = argc < 6 || argv[5] == Qnil ? 0.0f : NUM2FLOAT( argv[5] );
        rt->PictureList[rt->PictureCount].z = z;
        picture->z = z;
        rt->PictureCount++;
    }
    else
    {
        if( x1 > x2 )
        {
            int temp = x1;
            x1 = x2;
            x2 = temp;
        }
        if( y1 > y2 )
        {
            int temp = y1;
            y1 = y2;
            y2 = temp;
        }

        /* DXRubyPictureオブジェクト設定 */
        picture = (struct DXRubyPicture_drawLine *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_drawLine ) );
        picture->func = RenderTarget_drawLine_func;
        picture->x1 = x1 - rt->ox + 1;
        picture->y1 = y1 - rt->oy;
        picture->x2 = x2 - rt->ox;
        picture->y2 = y1 - rt->oy;
        picture->value = Qnil;
        picture->alpha = 0xff;
        picture->blendflag = 0;
        picture->col = col;

        /* リストデータに追加 */
        rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
        z = argc < 6 || argv[5] == Qnil ? 0.0f : NUM2FLOAT( argv[5] );
        rt->PictureList[rt->PictureCount].z = z;
        picture->z = z;
        rt->PictureCount++;

        /* DXRubyPictureオブジェクト設定 */
        picture = (struct DXRubyPicture_drawLine *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_drawLine ) );
        picture->func = RenderTarget_drawLine_func;
        picture->x1 = x1 - rt->ox;
        picture->y1 = y2 - rt->oy;
        picture->x2 = x2 - rt->ox - 1;
        picture->y2 = y2 - rt->oy;
        picture->value = Qnil;
        picture->alpha = 0xff;
        picture->blendflag = 0;
        picture->col = col;

        /* リストデータに追加 */
        rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
        z = argc < 6 || argv[5] == Qnil ? 0.0f : NUM2FLOAT( argv[5] );
        rt->PictureList[rt->PictureCount].z = z;
        picture->z = z;
        rt->PictureCount++;

        /* DXRubyPictureオブジェクト設定 */
        picture = (struct DXRubyPicture_drawLine *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_drawLine ) );
        picture->func = RenderTarget_drawLine_func;
        picture->x1 = x1 - rt->ox;
        picture->y1 = y1 - rt->oy;
        picture->x2 = x1 - rt->ox;
        picture->y2 = y2 - rt->oy - 1;
        picture->value = Qnil;
        picture->alpha = 0xff;
        picture->blendflag = 0;
        picture->col = col;

        /* リストデータに追加 */
        rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
        z = argc < 6 || argv[5] == Qnil ? 0.0f : NUM2FLOAT( argv[5] );
        rt->PictureList[rt->PictureCount].z = z;
        picture->z = z;
        rt->PictureCount++;

        /* DXRubyPictureオブジェクト設定 */
        picture = (struct DXRubyPicture_drawLine *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_drawLine ) );
        picture->func = RenderTarget_drawLine_func;
        picture->x1 = x2 - rt->ox;
        picture->y1 = y1 - rt->oy + 1;
        picture->x2 = x2 - rt->ox;
        picture->y2 = y2 - rt->oy;
        picture->value = Qnil;
        picture->alpha = 0xff;
        picture->blendflag = 0;
        picture->col = col;

        /* リストデータに追加 */
        rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
        z = argc < 6 || argv[5] == Qnil ? 0.0f : NUM2FLOAT( argv[5] );
        rt->PictureList[rt->PictureCount].z = z;
        picture->z = z;
        rt->PictureCount++;
    }

    return obj;
}


void RenderTarget_drawBoxFill_func( struct DXRubyPicture_drawLine *picture )
{
    TLVERTX2 VertexDataTbl[6];

    if( picture->x1 == picture->x2 )
    {
        VertexDataTbl[0].x = 
        VertexDataTbl[2].x = 
        VertexDataTbl[5].x = (float)picture->x1;
        VertexDataTbl[1].x = 
        VertexDataTbl[3].x = 
        VertexDataTbl[4].x = (float)picture->x2;
    }
    else if( picture->x1 > picture->x2 )
    {
        VertexDataTbl[0].x = 
        VertexDataTbl[2].x = 
        VertexDataTbl[5].x = (float)picture->x1 + 0.5f;
        VertexDataTbl[1].x = 
        VertexDataTbl[3].x = 
        VertexDataTbl[4].x = (float)picture->x2;
    }
    else
    {
        VertexDataTbl[0].x = 
        VertexDataTbl[2].x = 
        VertexDataTbl[5].x = (float)picture->x1;
        VertexDataTbl[1].x = 
        VertexDataTbl[3].x = 
        VertexDataTbl[4].x = (float)picture->x2 + 0.5f;
    }

    if( picture->y1 == picture->y2 )
    {
        VertexDataTbl[0].y = 
        VertexDataTbl[1].y = 
        VertexDataTbl[3].y = (float)picture->y1;
        VertexDataTbl[2].y = 
        VertexDataTbl[4].y = 
        VertexDataTbl[5].y = (float)picture->y2;
    }
    else if( picture->y1 > picture->y2 )
    {
        VertexDataTbl[0].y = 
        VertexDataTbl[1].y = 
        VertexDataTbl[3].y = (float)picture->y1 + 0.5f;
        VertexDataTbl[2].y = 
        VertexDataTbl[4].y = 
        VertexDataTbl[5].y = (float)picture->y2;
    }
    else
    {
        VertexDataTbl[0].y = 
        VertexDataTbl[1].y = 
        VertexDataTbl[3].y = (float)picture->y1;
        VertexDataTbl[2].y = 
        VertexDataTbl[4].y = 
        VertexDataTbl[5].y = (float)picture->y2 + 0.5f;
    }

    /* 頂点色 */
    VertexDataTbl[0].color = VertexDataTbl[1].color =
    VertexDataTbl[2].color = VertexDataTbl[3].color =
    VertexDataTbl[4].color = VertexDataTbl[5].color = picture->col;
    /* Ｚ座標 */
    VertexDataTbl[0].z  = VertexDataTbl[1].z =
    VertexDataTbl[2].z  = VertexDataTbl[3].z =
    VertexDataTbl[4].z  = VertexDataTbl[5].z = 0.0f;

    /* テクスチャをセット */
    g_pD3DDevice->lpVtbl->SetTexture(g_pD3DDevice, 0, (IDirect3DBaseTexture9*)NULL);

    /* デバイスに使用する頂点フォーマットをセット */
    g_pD3DDevice->lpVtbl->SetFVF( g_pD3DDevice, D3DFVF_XYZ | D3DFVF_DIFFUSE );

    /* 描画 */
    g_pD3DDevice->lpVtbl->DrawPrimitiveUP(g_pD3DDevice, D3DPT_TRIANGLELIST , 2, VertexDataTbl, sizeof(TLVERTX2));
}

/*--------------------------------------------------------------------
   描画設定（BoxFill描画）
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_drawBoxFill( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyPicture_drawLine *picture;
    float z;
    int col;
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, obj );
    DXRUBY_CHECK_DISPOSE( rt, surface );

    if( argc < 5 || argc > 6 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 5, 6 );

    Check_Type( argv[4], T_ARRAY );
    col = array2color( argv[4] );

    picture = (struct DXRubyPicture_drawLine *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_drawLine ) );

    /* DXRubyPictureオブジェクト設定 */
    picture->func = RenderTarget_drawBoxFill_func;
    picture->x1 = NUM2INT( argv[0] ) - rt->ox;
    picture->y1 = NUM2INT( argv[1] ) - rt->oy;
    picture->x2 = NUM2INT( argv[2] ) - rt->ox;
    picture->y2 = NUM2INT( argv[3] ) - rt->oy;
    picture->value = Qnil;
    picture->alpha = 0xff;
    picture->blendflag = 0;
    picture->col = col;

    /* リストデータに追加 */
    rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
    z = argc < 6 || argv[5] == Qnil ? 0.0f : NUM2FLOAT( argv[5] );
    rt->PictureList[rt->PictureCount].z = z;
    picture->z = z;
    rt->PictureCount++;

    return obj;
}


void RenderTarget_drawCircle_func( struct DXRubyPicture_drawCircle *picture )
{
    TLVERTX VertexDataTbl[6];
    float basex = (float)picture->x - (float)picture->r;
    float basey = (float)picture->y - (float)picture->r;
    float r2 = (float)picture->r * 2;
    int i;
    UINT pass;

    /* 頂点１ */
    VertexDataTbl[0].x = basex-0.5f;
    VertexDataTbl[0].y = basey-0.5f;
    /* 頂点２ */
    VertexDataTbl[1].x = VertexDataTbl[3].x = basex + r2-0.5f;
    VertexDataTbl[1].y = VertexDataTbl[3].y = basey-0.5f;
    /* 頂点３ */
    VertexDataTbl[4].x = basex + r2-0.5f;
    VertexDataTbl[4].y = basey + r2-0.5f;
    /* 頂点４ */
    VertexDataTbl[2].x = VertexDataTbl[5].x = basex-0.5f;
    VertexDataTbl[2].y = VertexDataTbl[5].y = basey + r2-0.5f;
    /* 頂点色 */
    VertexDataTbl[0].color = VertexDataTbl[1].color =
    VertexDataTbl[2].color = VertexDataTbl[3].color =
    VertexDataTbl[4].color = VertexDataTbl[5].color = D3DCOLOR_ARGB(picture->alpha,255,255,255);
    /* Ｚ座標 */
    VertexDataTbl[0].z  = VertexDataTbl[1].z =
    VertexDataTbl[2].z  = VertexDataTbl[3].z =
    VertexDataTbl[4].z  = VertexDataTbl[5].z = 0.0f;
    /* テクスチャ座標 */
    VertexDataTbl[0].tu = VertexDataTbl[5].tu = VertexDataTbl[2].tu = 0.0f;
    VertexDataTbl[0].tv = VertexDataTbl[1].tv = VertexDataTbl[3].tv = 0.0f;
    VertexDataTbl[1].tu = VertexDataTbl[3].tu = VertexDataTbl[4].tu = 1.0f;
    VertexDataTbl[4].tv = VertexDataTbl[5].tv = VertexDataTbl[2].tv = 1.0f;

    /* テクスチャをセット */
    g_pD3DDevice->lpVtbl->SetTexture(g_pD3DDevice, 0, (IDirect3DBaseTexture9*)NULL);

    /* デバイスに使用する頂点フォーマットをセット */
    g_pD3DDevice->lpVtbl->SetFVF(g_pD3DDevice, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);

    /* 色をセット */
    {
        D3DXHANDLE h;
        float *temp;

        h = g_WindowInfo.pD3DXEffectCircleShader->lpVtbl->GetParameterByName( g_WindowInfo.pD3DXEffectCircleShader, NULL, "color" );
        temp = alloca( sizeof(float) * 4 );
        temp[0] = (float)((picture->col >> 16) & 0xff)/255.0f;
        temp[1] = (float)((picture->col >> 8 ) & 0xff)/255.0f;
        temp[2] = (float)((picture->col      ) & 0xff)/255.0f;
        temp[3] = (float)((picture->col >> 24) & 0xff)/255.0f;
        g_WindowInfo.pD3DXEffectCircleShader->lpVtbl->SetFloatArray( g_WindowInfo.pD3DXEffectCircleShader, h, temp, 4 );
    }

    /* 円周を認識する幅をセット */
    {
        D3DXHANDLE h;
        h = g_WindowInfo.pD3DXEffectCircleShader->lpVtbl->GetParameterByName( g_WindowInfo.pD3DXEffectCircleShader, NULL, "p" );
        g_WindowInfo.pD3DXEffectCircleShader->lpVtbl->SetFloat( g_WindowInfo.pD3DXEffectCircleShader, h, 1.0f / r2 );
//        g_WindowInfo.pD3DXEffectCircleShader->lpVtbl->SetFloatArray( g_WindowInfo.pD3DXEffectCircleShader, h, temp, 4 );
    }

    g_WindowInfo.pD3DXEffectCircleShader->lpVtbl->Begin( g_WindowInfo.pD3DXEffectCircleShader, &pass, 0 );
    for( i = 0; i < pass; i++ )
    {
        /* 描画 */
        g_WindowInfo.pD3DXEffectCircleShader->lpVtbl->BeginPass( g_WindowInfo.pD3DXEffectCircleShader, i );
        g_pD3DDevice->lpVtbl->DrawPrimitiveUP( g_pD3DDevice, D3DPT_TRIANGLELIST, 2, VertexDataTbl, sizeof(TLVERTX) );
        g_WindowInfo.pD3DXEffectCircleShader->lpVtbl->EndPass( g_WindowInfo.pD3DXEffectCircleShader );
    }
    g_WindowInfo.pD3DXEffectCircleShader->lpVtbl->End( g_WindowInfo.pD3DXEffectCircleShader );
}

/*--------------------------------------------------------------------
   描画設定（Circle描画）
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_drawCircle( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyPicture_drawCircle *picture;
    float z;
    int col;
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, obj );
    DXRUBY_CHECK_DISPOSE( rt, surface );

    if( argc < 4 || argc > 5 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 4, 5 );

    Check_Type( argv[3], T_ARRAY );
    col = array2color( argv[3] );

    picture = (struct DXRubyPicture_drawCircle *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_drawCircle ) );

    /* DXRubyPictureオブジェクト設定 */
    picture->func = RenderTarget_drawCircle_func;
    picture->x = NUM2INT( argv[0] ) - rt->ox;
    picture->y = NUM2INT( argv[1] ) - rt->oy;
    picture->r = NUM2INT( argv[2] );
    picture->value = Qnil;
    picture->alpha = 0xff;
    picture->blendflag = 0;
    picture->col = col;

    /* リストデータに追加 */
    rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
    z = argc < 5 || argv[4] == Qnil ? 0.0f : NUM2FLOAT( argv[4] );
    rt->PictureList[rt->PictureCount].z = z;
    picture->z = z;
    rt->PictureCount++;

    return obj;
}


void RenderTarget_drawCircleFill_func( struct DXRubyPicture_drawCircle *picture )
{
    TLVERTX VertexDataTbl[6];
    float basex = (float)picture->x - (float)picture->r;
    float basey = (float)picture->y - (float)picture->r;
    float r2 = (float)picture->r * 2;
    int i;
    UINT pass;

    /* 頂点１ */
    VertexDataTbl[0].x = basex-0.5f;
    VertexDataTbl[0].y = basey-0.5f;
    /* 頂点２ */
    VertexDataTbl[1].x = VertexDataTbl[3].x = basex + r2-0.5f;
    VertexDataTbl[1].y = VertexDataTbl[3].y = basey-0.5f;
    /* 頂点３ */
    VertexDataTbl[4].x = basex + r2-0.5f;
    VertexDataTbl[4].y = basey + r2-0.5f;
    /* 頂点４ */
    VertexDataTbl[2].x = VertexDataTbl[5].x = basex-0.5f;
    VertexDataTbl[2].y = VertexDataTbl[5].y = basey + r2-0.5f;
    /* 頂点色 */
    VertexDataTbl[0].color = VertexDataTbl[1].color =
    VertexDataTbl[2].color = VertexDataTbl[3].color =
    VertexDataTbl[4].color = VertexDataTbl[5].color = D3DCOLOR_ARGB(picture->alpha,255,255,255);
    /* Ｚ座標 */
    VertexDataTbl[0].z  = VertexDataTbl[1].z =
    VertexDataTbl[2].z  = VertexDataTbl[3].z =
    VertexDataTbl[4].z  = VertexDataTbl[5].z = 0.0f;
    /* テクスチャ座標 */
    VertexDataTbl[0].tu = VertexDataTbl[5].tu = VertexDataTbl[2].tu = 0.0f;
    VertexDataTbl[0].tv = VertexDataTbl[1].tv = VertexDataTbl[3].tv = 0.0f;
    VertexDataTbl[1].tu = VertexDataTbl[3].tu = VertexDataTbl[4].tu = 1.0f;
    VertexDataTbl[4].tv = VertexDataTbl[5].tv = VertexDataTbl[2].tv = 1.0f;

    /* テクスチャをセット */
    g_pD3DDevice->lpVtbl->SetTexture(g_pD3DDevice, 0, (IDirect3DBaseTexture9*)NULL);

    /* デバイスに使用する頂点フォーマットをセット */
    g_pD3DDevice->lpVtbl->SetFVF(g_pD3DDevice, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);

    /* 色をセット */
    {
        D3DXHANDLE h;
        float *temp;

        h = g_WindowInfo.pD3DXEffectCircleFillShader->lpVtbl->GetParameterByName( g_WindowInfo.pD3DXEffectCircleFillShader, NULL, "color" );
        temp = alloca( sizeof(float) * 4 );
        temp[0] = (float)((picture->col >> 16) & 0xff)/255.0f;
        temp[1] = (float)((picture->col >> 8 ) & 0xff)/255.0f;
        temp[2] = (float)((picture->col      ) & 0xff)/255.0f;
        temp[3] = (float)((picture->col >> 24) & 0xff)/255.0f;
        g_WindowInfo.pD3DXEffectCircleFillShader->lpVtbl->SetFloatArray( g_WindowInfo.pD3DXEffectCircleFillShader, h, temp, 4 );
    }

    g_WindowInfo.pD3DXEffectCircleFillShader->lpVtbl->Begin( g_WindowInfo.pD3DXEffectCircleFillShader, &pass, 0 );
    for( i = 0; i < pass; i++ )
    {
        /* 描画 */
        g_WindowInfo.pD3DXEffectCircleFillShader->lpVtbl->BeginPass( g_WindowInfo.pD3DXEffectCircleFillShader, i );
        g_pD3DDevice->lpVtbl->DrawPrimitiveUP( g_pD3DDevice, D3DPT_TRIANGLELIST, 2, VertexDataTbl, sizeof(TLVERTX) );
        g_WindowInfo.pD3DXEffectCircleFillShader->lpVtbl->EndPass( g_WindowInfo.pD3DXEffectCircleFillShader );
    }
    g_WindowInfo.pD3DXEffectCircleFillShader->lpVtbl->End( g_WindowInfo.pD3DXEffectCircleFillShader );
}

/*--------------------------------------------------------------------
   描画設定（CircleFill描画）
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_drawCircleFill( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyPicture_drawCircle *picture;
    float z;
    int col;
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, obj );
    DXRUBY_CHECK_DISPOSE( rt, surface );

    if( argc < 4 || argc > 5 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 4, 5 );

    Check_Type( argv[3], T_ARRAY );
    col = array2color( argv[3] );

    picture = (struct DXRubyPicture_drawCircle *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_drawCircle ) );

    /* DXRubyPictureオブジェクト設定 */
    picture->func = RenderTarget_drawCircleFill_func;
    picture->x = NUM2INT( argv[0] ) - rt->ox;
    picture->y = NUM2INT( argv[1] ) - rt->oy;
    picture->r = NUM2INT( argv[2] );
    picture->value = Qnil;
    picture->alpha = 0xff;
    picture->blendflag = 0;
    picture->col = col;

    /* リストデータに追加 */
    rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
    z = argc < 5 || argv[4] == Qnil ? 0.0f : NUM2FLOAT( argv[4] );
    rt->PictureList[rt->PictureCount].z = z;
    picture->z = z;
    rt->PictureCount++;

    return obj;
}


void RenderTarget_draw_func( struct DXRubyPicture_draw *picture )
{
    TLVERTX VertexDataTbl[6];
    struct DXRubyImage *image = DXRUBY_GET_STRUCT( Image, picture->value );
    float basex = picture->x - 0.5f;
    float basey = picture->y - 0.5f;
    float width = (float)image->width;
    float height = (float)image->height;
    float tu1;
    float tu2;
    float tv1;
    float tv2;

    DXRUBY_CHECK_DISPOSE( image, texture );
    tu1 = image->x / image->texture->width;
    tu2 = (image->x + width) / image->texture->width;
    tv1 = image->y / image->texture->height;
    tv2 = (image->y + height) / image->texture->height;

    /* 頂点１ */
    VertexDataTbl[0].x = basex;
    VertexDataTbl[0].y = basey;
    /* 頂点２ */
    VertexDataTbl[1].x = VertexDataTbl[3].x = basex + width;
    VertexDataTbl[1].y = VertexDataTbl[3].y = basey;
    /* 頂点３ */
    VertexDataTbl[4].x = basex + width;
    VertexDataTbl[4].y = basey + height;
    /* 頂点４ */
    VertexDataTbl[2].x = VertexDataTbl[5].x = basex;
    VertexDataTbl[2].y = VertexDataTbl[5].y = basey + height;
    /* 頂点色 */
    VertexDataTbl[0].color = VertexDataTbl[1].color =
    VertexDataTbl[2].color = VertexDataTbl[3].color =
    VertexDataTbl[4].color = VertexDataTbl[5].color = D3DCOLOR_ARGB(picture->alpha,255,255,255);
    /* Ｚ座標 */
    VertexDataTbl[0].z  = VertexDataTbl[1].z =
    VertexDataTbl[2].z  = VertexDataTbl[3].z =
    VertexDataTbl[4].z  = VertexDataTbl[5].z = 0.0f;
    /* テクスチャ座標 */
    VertexDataTbl[0].tu = VertexDataTbl[5].tu = VertexDataTbl[2].tu = tu1;
    VertexDataTbl[0].tv = VertexDataTbl[1].tv = VertexDataTbl[3].tv = tv1;
    VertexDataTbl[1].tu = VertexDataTbl[3].tu = VertexDataTbl[4].tu = tu2;
    VertexDataTbl[4].tv = VertexDataTbl[5].tv = VertexDataTbl[2].tv = tv2;

    /* テクスチャをセット */
    g_pD3DDevice->lpVtbl->SetTexture(g_pD3DDevice, 0, (IDirect3DBaseTexture9*)image->texture->pD3DTexture);

    /* デバイスに使用する頂点フォーマットをセット */
    g_pD3DDevice->lpVtbl->SetFVF(g_pD3DDevice, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);

    /* 描画 */
    g_pD3DDevice->lpVtbl->DrawPrimitiveUP(g_pD3DDevice, D3DPT_TRIANGLELIST, 2, VertexDataTbl, sizeof(TLVERTX));

    /* ロックカウントを減らす */
//    image->lockcount--;
}

/*--------------------------------------------------------------------
   描画設定（通常描画）
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_draw( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyImage *image;
    struct DXRubyPicture_draw *picture;
    float z;
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, obj );
    DXRUBY_CHECK_DISPOSE( rt, surface );
//    DXRUBY_CHECK_IMAGE_LOCK( rt );

    if( argc < 3 || argc > 4 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 3, 4 );

    /* 引数のイメージオブジェクトから中身を取り出す */
    DXRUBY_CHECK_IMAGE( argv[2] );
    image = DXRUBY_GET_STRUCT( Image, argv[2] );
    DXRUBY_CHECK_DISPOSE( image, texture );

    picture = (struct DXRubyPicture_draw *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_draw ) );

    /* DXRubyPictureオブジェクト設定 */
    picture->func = RenderTarget_draw_func;
    picture->x = NUM2INT( argv[0] ) - rt->ox;
    picture->y = NUM2INT( argv[1] ) - rt->oy;
    picture->value = argv[2];
    picture->alpha = 0xff;
    picture->blendflag = 0;

    /* リストデータに追加 */
    rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
    z = argc < 4 || argv[3] == Qnil ? 0.0f : NUM2FLOAT( argv[3] );
    rt->PictureList[rt->PictureCount].z = z;
    picture->z = z;
    rt->PictureCount++;

    /* RenderTargetだった場合に描画予約があればupdateする */
    RenderTerget_auto_update( argv[2] );

    /* 使われたimageのロック */
//    image->lockcount++;

    return obj;
}

/*--------------------------------------------------------------------
   描画設定（半透明描画）
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_drawAlpha( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyImage *image;
    struct DXRubyPicture_draw *picture;
    float z;
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, obj );
    DXRUBY_CHECK_DISPOSE( rt, surface );
//    DXRUBY_CHECK_IMAGE_LOCK( rt );

    if( argc < 4 || argc > 5 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 4, 5 );

    /* 引数のイメージオブジェクトから中身を取り出す */
    DXRUBY_CHECK_IMAGE( argv[2] );
    image = DXRUBY_GET_STRUCT( Image, argv[2] );
    DXRUBY_CHECK_DISPOSE( image, texture );

    picture = (struct DXRubyPicture_draw *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_draw ) );

    /* DXRubyPictureオブジェクト設定 */
    picture->func = RenderTarget_draw_func;
    picture->x = NUM2INT( argv[0] ) - rt->ox;
    picture->y = NUM2INT( argv[1] ) - rt->oy;
    picture->value = argv[2];
    picture->alpha = NUM2INT( argv[3] );
    picture->blendflag = 0;

    /* リストデータに追加 */
    rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
    z = argc < 5 || argv[4] == Qnil ? 0.0f : NUM2FLOAT( argv[4] );
    rt->PictureList[rt->PictureCount].z = z;
    picture->z = z;
    rt->PictureCount++;

    /* RenderTargetだった場合に描画予約があればupdateする */
    RenderTerget_auto_update( argv[2] );

    /* 使われたimageのロック */
//    image->lockcount++;

    return obj;
}


/*--------------------------------------------------------------------
   描画設定（加算合成描画）
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_drawAdd( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyImage *image;
    struct DXRubyPicture_draw *picture;
    float z;
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, obj );
    DXRUBY_CHECK_DISPOSE( rt, surface );
//    DXRUBY_CHECK_IMAGE_LOCK( rt );

    if( argc < 3 || argc > 4 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 3, 4 );

    /* 引数のイメージオブジェクトから中身を取り出す */
    DXRUBY_CHECK_IMAGE( argv[2] );
    image = DXRUBY_GET_STRUCT( Image, argv[2] );
    DXRUBY_CHECK_DISPOSE( image, texture );

    picture = (struct DXRubyPicture_draw *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_draw ) );

    /* DXRubyPictureオブジェクト設定 */
    picture->func = RenderTarget_draw_func;
    picture->x = NUM2INT( argv[0] ) - rt->ox;
    picture->y = NUM2INT( argv[1] ) - rt->oy;
    picture->value = argv[2];
    picture->alpha = 0xff;
    picture->blendflag = 4;

    /* リストデータに追加 */
    rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
    z = argc < 4 || argv[3] == Qnil ? 0.0f : NUM2FLOAT( argv[3] );
    rt->PictureList[rt->PictureCount].z = z;
    picture->z = z;
    rt->PictureCount++;

    /* RenderTargetだった場合に描画予約があればupdateする */
    RenderTerget_auto_update( argv[2] );

    /* 使われたimageのロック */
//    image->lockcount++;

    return obj;
}


/*--------------------------------------------------------------------
   描画設定（減算合成描画）
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_drawSub( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyImage *image;
    struct DXRubyPicture_draw *picture;
    float z;
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, obj );
    DXRUBY_CHECK_DISPOSE( rt, surface );
//    DXRUBY_CHECK_IMAGE_LOCK( rt );

    if( argc < 3 || argc > 4 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 3, 4);

    /* 引数のイメージオブジェクトから中身を取り出す */
    DXRUBY_CHECK_IMAGE( argv[2] );
    image = DXRUBY_GET_STRUCT( Image, argv[2] );
    DXRUBY_CHECK_DISPOSE( image, texture );

    picture = (struct DXRubyPicture_draw *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_draw ) );

    /* DXRubyPictureオブジェクト設定 */
    picture->func = RenderTarget_draw_func;
    picture->x = NUM2INT( argv[0] ) - rt->ox;
    picture->y = NUM2INT( argv[1] ) - rt->oy;
    picture->value = argv[2];
    picture->alpha = 0xff;
    picture->blendflag = 6;

    /* リストデータに追加 */
    rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
    z = argc < 4 || argv[3] == Qnil ? 0.0f : NUM2FLOAT( argv[3] );
    rt->PictureList[rt->PictureCount].z = z;
    picture->z = z;
    rt->PictureCount++;

    /* RenderTargetだった場合に描画予約があればupdateする */
    RenderTerget_auto_update( argv[2] );

    /* 使われたimageのロック */
//    image->lockcount++;

    return obj;
}


static int Window_drawShader_func_foreach( VALUE key, VALUE value, VALUE obj ) /* key、valueはvparam、objはShaderCore */
{
    struct DXRubyShaderCore *core = DXRUBY_GET_STRUCT( ShaderCore, obj );
    const char *str;
    D3DXHANDLE h;
    int i;
    VALUE vtype;

    if ( key == Qundef ) return ST_CONTINUE;
    if ( TYPE( key ) != T_SYMBOL ) return ST_CONTINUE;

    str = rb_id2name( SYM2ID( key ) );
    vtype = hash_lookup( core->vtype, key );
    if( vtype == Qnil )
    {
        vtype = hash_lookup( core->vtype, rb_str_new2(str) );
    }
    if( vtype == symbol_float )
    {
        float *temp;
        h = core->pD3DXEffect->lpVtbl->GetParameterByName( core->pD3DXEffect, NULL, str );
        if( TYPE( value ) == T_ARRAY )
        {
            temp = alloca( sizeof(float) * RARRAY_LEN( value ) );
            for( i = 0; i < RARRAY_LEN( value ); i++ )
            {
                temp[i] = NUM2FLOAT( RARRAY_PTR( value )[i] );
            }
            core->pD3DXEffect->lpVtbl->SetFloatArray( core->pD3DXEffect, h, temp, i );
        }
#ifdef DXRUBY15
        else if( DXRUBY_CHECK( Matrix, value ) )
        {
            core->pD3DXEffect->lpVtbl->SetMatrix( core->pD3DXEffect, h, (D3DXMATRIX*)&DXRUBY_GET_STRUCT( Matrix, value )->m );
        }
        else if( DXRUBY_CHECK( Vector, value ) )
        {
            core->pD3DXEffect->lpVtbl->SetVector( core->pD3DXEffect, h, (D3DXVECTOR4*)&DXRUBY_GET_STRUCT( Vector, value )->v );
        }
#endif
        else 
        {
            core->pD3DXEffect->lpVtbl->SetFloat( core->pD3DXEffect, h, NUM2FLOAT( value ) );
        }
    }
    else if( vtype == symbol_texture )
    {
        if ( DXRUBY_CHECK( Image, value ) || DXRUBY_CHECK( RenderTarget, value ) )
        {
            DXRUBY_CHECK_DISPOSE( DXRUBY_GET_STRUCT( Image, value ), texture );
            core->pD3DXEffect->lpVtbl->SetTexture( core->pD3DXEffect, str ,
                                                     (IDirect3DBaseTexture9*)(DXRUBY_GET_STRUCT( Image, value )->texture->pD3DTexture) );
            /* ロックカウントを減らす */
//            DXRUBY_GET_STRUCT( Image, value )->lockcount--;

        }
        else
        {
            rb_raise( eDXRubyError, "Argument error(texture) - Window_draw" );
        }
    }
    else if( vtype == symbol_int )
    {
        int *temp;
        h = core->pD3DXEffect->lpVtbl->GetParameterByName( core->pD3DXEffect, NULL, str );
        if( TYPE( value ) == T_ARRAY )
        {
            temp = alloca( sizeof(int) * RARRAY_LEN( value ) );
            for( i = 0; i < RARRAY_LEN( value ); i++ )
            {
                temp[i] = NUM2INT( RARRAY_PTR( value )[i] );
            }
            core->pD3DXEffect->lpVtbl->SetIntArray( core->pD3DXEffect, h, temp, i );
        }
        else
        {
            core->pD3DXEffect->lpVtbl->SetInt( core->pD3DXEffect, h, NUM2INT( value ) );
        }
    }
    else if( vtype == symbol_technique )
    {
        if( TYPE( value ) == T_STRING )
        {
            h = core->pD3DXEffect->lpVtbl->GetTechniqueByName( core->pD3DXEffect, RSTRING_PTR( value ) );
            core->pD3DXEffect->lpVtbl->SetTechnique( core->pD3DXEffect, h );
        } else if( TYPE( value ) == T_SYMBOL )
        {
            h = core->pD3DXEffect->lpVtbl->GetTechniqueByName( core->pD3DXEffect, rb_id2name( SYM2ID( value ) ) );
            core->pD3DXEffect->lpVtbl->SetTechnique( core->pD3DXEffect, h );
        }
        
    }
    else
    {
        rb_raise( eDXRubyError, "Unknown parameter type of shader object - Window_draw" );
    }

    return ST_CONTINUE;
}

void RenderTarget_drawShader_func( struct DXRubyPicture_draw *picture )
{
    TLVERTX VertexDataTbl[6];
    struct DXRubyImage *image;
    struct DXRubyShaderCore *core;
    float basex = (float)picture->x;
    float basey = (float)picture->y;
    float width;
    float height;
    float tu1;
    float tu2;
    float tv1;
    float tv2;
    int i;
    UINT pass;

    image = DXRUBY_GET_STRUCT( Image, RARRAY_PTR( picture->value )[0] );
    core = DXRUBY_GET_STRUCT( ShaderCore, RARRAY_PTR( picture->value )[1] );
    DXRUBY_CHECK_DISPOSE( image, texture );
    DXRUBY_CHECK_DISPOSE( core, pD3DXEffect );

    width = (float)image->width;
    height = (float)image->height;
    tu1 = image->x / image->texture->width;
    tu2 = (image->x + width) / image->texture->width;
    tv1 = image->y / image->texture->height;
    tv2 = (image->y + height) / image->texture->height;

    /* 頂点１ */
    VertexDataTbl[0].x = basex-0.5f;
    VertexDataTbl[0].y = basey-0.5f;
    /* 頂点２ */
    VertexDataTbl[1].x = VertexDataTbl[3].x = basex + width-0.5f;
    VertexDataTbl[1].y = VertexDataTbl[3].y = basey-0.5f;
    /* 頂点３ */
    VertexDataTbl[4].x = basex + width-0.5f;
    VertexDataTbl[4].y = basey + height-0.5f;
    /* 頂点４ */
    VertexDataTbl[2].x = VertexDataTbl[5].x = basex-0.5f;
    VertexDataTbl[2].y = VertexDataTbl[5].y = basey + height-0.5f;
    /* 頂点色 */
    VertexDataTbl[0].color = VertexDataTbl[1].color =
    VertexDataTbl[2].color = VertexDataTbl[3].color =
    VertexDataTbl[4].color = VertexDataTbl[5].color = D3DCOLOR_ARGB(picture->alpha,255,255,255);
    /* Ｚ座標 */
    VertexDataTbl[0].z  = VertexDataTbl[1].z =
    VertexDataTbl[2].z  = VertexDataTbl[3].z =
    VertexDataTbl[4].z  = VertexDataTbl[5].z = 0.0f;
    /* テクスチャ座標 */
    VertexDataTbl[0].tu = VertexDataTbl[5].tu = VertexDataTbl[2].tu = tu1;
    VertexDataTbl[0].tv = VertexDataTbl[1].tv = VertexDataTbl[3].tv = tv1;
    VertexDataTbl[1].tu = VertexDataTbl[3].tu = VertexDataTbl[4].tu = tu2;
    VertexDataTbl[4].tv = VertexDataTbl[5].tv = VertexDataTbl[2].tv = tv2;

    /* デバイスに使用する頂点フォーマットをセット */
    g_pD3DDevice->lpVtbl->SetFVF(g_pD3DDevice, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);

    rb_hash_foreach( RARRAY_PTR( picture->value )[2], Window_drawShader_func_foreach, RARRAY_PTR( picture->value )[1]);

    DXRUBY_CHECK_DISPOSE( DXRUBY_GET_STRUCT( Image, RARRAY_PTR( picture->value )[0] ), texture );
    core->pD3DXEffect->lpVtbl->SetTexture( core->pD3DXEffect, "tex0",
                                             (IDirect3DBaseTexture9*)(DXRUBY_GET_STRUCT( Image, RARRAY_PTR( picture->value )[0] )->texture->pD3DTexture) );

    core->pD3DXEffect->lpVtbl->Begin( core->pD3DXEffect, &pass, 0 );
    for( i = 0; i < pass; i++ )
    {
        /* 描画 */
        core->pD3DXEffect->lpVtbl->BeginPass( core->pD3DXEffect, i );
        g_pD3DDevice->lpVtbl->DrawPrimitiveUP( g_pD3DDevice, D3DPT_TRIANGLELIST, 2, VertexDataTbl, sizeof(TLVERTX) );
        core->pD3DXEffect->lpVtbl->EndPass( core->pD3DXEffect );
    }
    core->pD3DXEffect->lpVtbl->End( core->pD3DXEffect );

    /* ロックカウントを減らす */
//    image->lockcount--;
}

/*--------------------------------------------------------------------
   描画設定（シェーダ描画）
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_drawShader( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyPicture_draw *picture;
    struct DXRubyShaderCore *core;
    struct DXRubyShader *shader;
    float z;
    volatile VALUE temp;
    int i;
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, obj );

    if( argc < 4 || argc > 5 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 4, 5 );

    DXRUBY_CHECK_DISPOSE( rt, surface );
//    DXRUBY_CHECK_IMAGE_LOCK( rt );

    picture = (struct DXRubyPicture_draw *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_draw ) );

    DXRUBY_CHECK_TYPE( Shader, argv[3] );
    shader = DXRUBY_GET_STRUCT( Shader, argv[3] );
    core = DXRUBY_GET_STRUCT( ShaderCore, shader->vcore );
    DXRUBY_CHECK_DISPOSE( core, pD3DXEffect );

    /* 引数のイメージオブジェクトから中身を取り出す */
    DXRUBY_CHECK_IMAGE( argv[2] );
    DXRUBY_CHECK_DISPOSE( DXRUBY_GET_STRUCT( Image, argv[2] ), texture );

    temp = rb_ary_new3( 3, argv[2], shader->vcore, rb_obj_dup( shader->vparam ) );
    picture->value = temp;

    /* Shader内のImageオブジェクトをロックする */
//    rb_hash_foreach( RARRAY_PTR( picture->value )[2], Window_drawShader_func_foreach_lock, RARRAY_PTR( picture->value )[1]);

    /* DXRubyPictureオブジェクト設定 */
    picture->func = RenderTarget_drawShader_func;
    picture->x = NUM2INT( argv[0] ) - rt->ox;
    picture->y = NUM2INT( argv[1] ) - rt->oy;
    picture->alpha = 0xff;
    picture->blendflag = 0;

    /* リストデータに追加 */
    rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
    z = argc < 5 || argv[4] == Qnil ? 0.0f : NUM2FLOAT( argv[4] );
    rt->PictureList[rt->PictureCount].z = z;
    picture->z = z;
    rt->PictureCount++;

    /* RenderTargetだった場合に描画予約があればupdateする */
    RenderTerget_auto_update( argv[2] );

    /* 使われたimageのロック */
//    DXRUBY_GET_STRUCT( Image, argv[2] )->lockcount++;

    return obj;
}


void RenderTarget_drawEx_func( struct DXRubyPicture_drawEx *picture )
{
    TLVERTX VertexDataTbl[6];
    struct DXRubyImage *image;
    float angle = 3.141592653589793115997963468544185161590576171875f / 180.0f * picture->angle;
    float sina = sin(angle);
    float cosa = cos(angle);
    float data1x = picture->scalex * cosa;
    float data2x = picture->scalex * sina;
    float data1y = picture->scaley * sina;
    float data2y = picture->scaley * cosa;
    float tu1;
    float tu2;
    float tv1;
    float tv2;
    float centerx = -picture->centerx;
    float centery = -picture->centery;
    float width, height;
    float basex = picture->x - centerx;
    float basey = picture->y - centery;
    int i;
    UINT pass;

    if( TYPE(picture->value) == T_ARRAY )
    {
        image = DXRUBY_GET_STRUCT( Image, RARRAY_PTR( picture->value )[0] );
    }
    else
    {
        image = DXRUBY_GET_STRUCT( Image, picture->value );
    }

    DXRUBY_CHECK_DISPOSE( image, texture );

    width = (float)image->width;
    height = (float)image->height;

    tu1 = (image->x) / image->texture->width;
    tu2 = (image->x + image->width) / image->texture->width;
    tv1 = (image->y) / image->texture->height;
    tv2 = (image->y + image->height) / image->texture->height;

    /* 頂点１ */
    VertexDataTbl[0].x =  centerx * data1x - centery * data1y + basex - 0.5f;
    VertexDataTbl[0].y =  centerx * data2x + centery * data2y + basey - 0.5f;
    /* 頂点２ */
    VertexDataTbl[1].x = VertexDataTbl[3].x =  (centerx+width) * data1x - centery * data1y + basex - 0.5f;
    VertexDataTbl[1].y = VertexDataTbl[3].y =  (centerx+width) * data2x + centery * data2y + basey - 0.5f;
    /* 頂点３ */
    VertexDataTbl[4].x =  (centerx+width) * data1x - (centery+height) * data1y + basex - 0.5f;
    VertexDataTbl[4].y =  (centerx+width) * data2x + (centery+height) * data2y + basey - 0.5f;
    /* 頂点４ */
    VertexDataTbl[2].x = VertexDataTbl[5].x =  centerx * data1x - (centery+height) * data1y + basex - 0.5f;
    VertexDataTbl[2].y = VertexDataTbl[5].y =  centerx * data2x + (centery+height) * data2y + basey - 0.5f;
    /* 頂点色 */
    VertexDataTbl[0].color = VertexDataTbl[1].color =
    VertexDataTbl[2].color = VertexDataTbl[3].color =
    VertexDataTbl[4].color = VertexDataTbl[5].color = D3DCOLOR_ARGB(picture->alpha,255,255,255);
    /* Ｚ座標 */
    VertexDataTbl[0].z  = VertexDataTbl[1].z =
    VertexDataTbl[2].z  = VertexDataTbl[3].z =
    VertexDataTbl[4].z  = VertexDataTbl[5].z = 0.0f;
    /* テクスチャ座標 */
    VertexDataTbl[0].tu = VertexDataTbl[5].tu = VertexDataTbl[2].tu = tu1;
    VertexDataTbl[0].tv = VertexDataTbl[1].tv = VertexDataTbl[3].tv = tv1;
    VertexDataTbl[1].tu = VertexDataTbl[3].tu = VertexDataTbl[4].tu = tu2;
    VertexDataTbl[4].tv = VertexDataTbl[5].tv = VertexDataTbl[2].tv = tv2;

    /* デバイスに使用する頂点フォーマットをセット */
    g_pD3DDevice->lpVtbl->SetFVF(g_pD3DDevice, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);

    if( TYPE(picture->value) == T_ARRAY ) /* Shaderあり */
    {
        struct DXRubyShaderCore *core = DXRUBY_GET_STRUCT( ShaderCore, RARRAY_PTR( picture->value )[1] );
        DXRUBY_CHECK_DISPOSE( core, pD3DXEffect );

        rb_hash_foreach( RARRAY_PTR( picture->value )[2], Window_drawShader_func_foreach, RARRAY_PTR( picture->value )[1] );

        core->pD3DXEffect->lpVtbl->SetTexture( core->pD3DXEffect, "tex0",
                                                 (IDirect3DBaseTexture9*)image->texture->pD3DTexture );

        core->pD3DXEffect->lpVtbl->Begin( core->pD3DXEffect, &pass, 0 );
        for( i = 0; i < pass; i++ )
        {
            /* 描画 */
            core->pD3DXEffect->lpVtbl->BeginPass( core->pD3DXEffect, i );
            g_pD3DDevice->lpVtbl->DrawPrimitiveUP( g_pD3DDevice, D3DPT_TRIANGLELIST, 2, VertexDataTbl, sizeof(TLVERTX) );
            core->pD3DXEffect->lpVtbl->EndPass( core->pD3DXEffect );
        }
        core->pD3DXEffect->lpVtbl->End( core->pD3DXEffect );
    }
    else
    {
        /* テクスチャをセット */
        g_pD3DDevice->lpVtbl->SetTexture( g_pD3DDevice, 0, (IDirect3DBaseTexture9*)image->texture->pD3DTexture );

        /* 描画 */
        g_pD3DDevice->lpVtbl->DrawPrimitiveUP( g_pD3DDevice, D3DPT_TRIANGLELIST, 2, VertexDataTbl, sizeof(TLVERTX) );
    }

    /* ロックカウントを減らす */
//    image->lockcount--;
}

/*--------------------------------------------------------------------
   描画設定（拡大縮小描画）
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_drawScale( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyImage *image;
    struct DXRubyPicture_drawEx *picture;
    float z;
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, obj );
    volatile VALUE temp;
    DXRUBY_CHECK_DISPOSE( rt, surface );
//    DXRUBY_CHECK_IMAGE_LOCK( rt );

    if( argc < 5 || argc > 8 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 5, 8 );

    /* 引数のイメージオブジェクトから中身を取り出す */
    DXRUBY_CHECK_IMAGE( argv[2] );
    image = DXRUBY_GET_STRUCT( Image, argv[2] );
    DXRUBY_CHECK_DISPOSE( image, texture );

    picture = (struct DXRubyPicture_drawEx *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_drawEx ) );

    /* DXRubyPictureオブジェクト設定 */
    picture->func = RenderTarget_drawEx_func;
    picture->x = NUM2INT( argv[0] ) - rt->ox;
    picture->y = NUM2INT( argv[1] ) - rt->oy;
    picture->value = argv[2];
    picture->alpha = 0xff;
    picture->blendflag = 0;
    picture->angle  = 0.0f;
    picture->scalex = NUM2FLOAT( argv[3] );
    picture->scaley = NUM2FLOAT( argv[4] );
    picture->centerx = argc < 6 || argv[5] == Qnil  ? image->width / 2 : NUM2FLOAT( argv[5] );
    picture->centery = argc < 7 || argv[6] == Qnil  ? image->height / 2 : NUM2FLOAT( argv[6] );

    /* リストデータに追加 */
    rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
    z = argc < 8 || argv[7] == Qnil ? 0.0f : NUM2FLOAT( argv[7] );
    rt->PictureList[rt->PictureCount].z = z;
    picture->z = z;
    rt->PictureCount++;

    /* RenderTargetだった場合に描画予約があればupdateする */
    RenderTerget_auto_update( argv[2] );

    /* 使われたimageのロック */
//    image->lockcount++;

    return obj;
}

/*--------------------------------------------------------------------
   描画設定（回転描画）
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_drawRot( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyImage *image;
    struct DXRubyPicture_drawEx *picture;
    float z;
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, obj );
    volatile VALUE temp;
    DXRUBY_CHECK_DISPOSE( rt, surface );
//    DXRUBY_CHECK_IMAGE_LOCK( rt );

    if( argc < 4 || argc > 7 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 4, 7 );

    /* 引数のイメージオブジェクトから中身を取り出す */
    DXRUBY_CHECK_IMAGE( argv[2] );
    image = DXRUBY_GET_STRUCT( Image, argv[2] );
    DXRUBY_CHECK_DISPOSE( image, texture );

    picture = (struct DXRubyPicture_drawEx *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_drawEx ) );

    /* DXRubyPictureオブジェクト設定 */
    picture->func = RenderTarget_drawEx_func;
    picture->x = NUM2INT( argv[0] ) - rt->ox;
    picture->y = NUM2INT( argv[1] ) - rt->oy;
    picture->value = argv[2];
    picture->alpha = 0xff;
    picture->blendflag = 0;
    picture->angle  = NUM2FLOAT( argv[3] );
    picture->scalex = 1.0f;
    picture->scaley = 1.0f;
    picture->centerx = argc < 5 || argv[4] == Qnil  ? image->width / 2 : NUM2FLOAT( argv[4] );
    picture->centery = argc < 5 || argv[5] == Qnil  ? image->height / 2 : NUM2FLOAT( argv[5] );

    /* リストデータに追加 */
    rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
    z = argc < 7 || argv[6] == Qnil ? 0.0f : NUM2FLOAT( argv[6] );
    rt->PictureList[rt->PictureCount].z = z;
    picture->z = z;
    rt->PictureCount++;

    /* RenderTargetだった場合に描画予約があればupdateする */
    RenderTerget_auto_update( argv[2] );

    /* 使われたimageのロック */
//    image->lockcount++;

    return obj;
}

/*--------------------------------------------------------------------
   描画設定（フルオプション）
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_drawEx( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyImage *image;
    VALUE vx, vy, vz, val, vangle, vscalex, vscaley, valpha, vcenterx, vcentery, vblend, vshader, voffset_sync;
    VALUE voption;
    float z;
    volatile VALUE temp;
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, obj );
    DXRUBY_CHECK_DISPOSE( rt, surface );
//    DXRUBY_CHECK_IMAGE_LOCK( rt );

    if( argc < 3 || argc > 4 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 3, 4 );

    if( argc < 4 || argv[3] == Qnil )
    {
        voption = rb_hash_new();
    }
    else
    {
        Check_Type( argv[3], T_HASH );
        voption = argv[3];
    }

    vblend = hash_lookup( voption, symbol_blend );
    vangle = hash_lookup( voption, symbol_angle );
    valpha = hash_lookup( voption, symbol_alpha );
    vscalex = hash_lookup( voption, symbol_scale_x );
    vscalex = vscalex == Qnil ? hash_lookup( voption, symbol_scalex ) : vscalex;
    vscaley = hash_lookup( voption, symbol_scale_y );
    vscaley = vscaley == Qnil ? hash_lookup( voption, symbol_scaley ) : vscaley;
    vcenterx = hash_lookup( voption, symbol_center_x );
    vcenterx = vcenterx == Qnil ? hash_lookup( voption, symbol_centerx ) : vcenterx;
    vcentery = hash_lookup( voption, symbol_center_y );
    vcentery = vcentery == Qnil ? hash_lookup( voption, symbol_centery ) : vcentery;
    vshader = hash_lookup( voption, symbol_shader );
    vz = hash_lookup( voption, symbol_z );
    voffset_sync = hash_lookup( voption, symbol_offset_sync );
    if( vshader != Qnil )
    {
        DXRUBY_CHECK_TYPE( Shader, vshader );
    }

    {
        struct DXRubyPicture_drawEx *picture;

        /* 引数のイメージオブジェクトから中身を取り出す */
        DXRUBY_CHECK_IMAGE( argv[2] );
        image = DXRUBY_GET_STRUCT( Image, argv[2] );
        DXRUBY_CHECK_DISPOSE( image, texture );

        picture = (struct DXRubyPicture_drawEx *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_drawEx ) );

        /* DXRubyPictureオブジェクト設定 */
        picture->func = RenderTarget_drawEx_func;
        picture->angle   = (vangle   == Qnil ? 0.0f             : NUM2FLOAT( vangle   ));
        picture->scalex  = (vscalex  == Qnil ? 1.0f             : NUM2FLOAT( vscalex  ));
        picture->scaley  = (vscaley  == Qnil ? 1.0f             : NUM2FLOAT( vscaley  ));
        picture->centerx = (vcenterx == Qnil ? image->width / 2.0f : NUM2FLOAT( vcenterx ));
        picture->centery = (vcentery == Qnil ? image->height / 2.0f : NUM2FLOAT( vcentery ));
        picture->alpha   = (valpha   == Qnil ? 0xff             : NUM2INT( valpha   ));
        picture->blendflag = (vblend == Qnil ? 0 :
                             (vblend == symbol_add ? 4 :
                             (vblend == symbol_none ? 1 :
                             (vblend == symbol_add2 ? 5 :
                             (vblend == symbol_sub ? 6 :
                             (vblend == symbol_sub2 ? 7 : 0))))));
        picture->x = (int)(NUM2FLOAT( argv[0] ) - (RTEST(voffset_sync) ? picture->centerx : 0) - rt->ox);
        picture->y = (int)(NUM2FLOAT( argv[1] ) - (RTEST(voffset_sync) ? picture->centery : 0) - rt->oy);
        if( vshader != Qnil )
        {
            struct DXRubyShader *shader = DXRUBY_GET_STRUCT( Shader, vshader );
            picture->value = temp = rb_ary_new3( 3, argv[2], shader->vcore, rb_obj_dup( shader->vparam ) );

            /* Shader内のImageオブジェクトをロックする */
//            rb_hash_foreach( RARRAY_PTR( picture->value )[2], Window_drawShader_func_foreach_lock, RARRAY_PTR( picture->value )[1]);
        }
        else
        {
            picture->value = argv[2];
        }

        /* リストデータに追加 */
        rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
        z = vz == Qnil ? 0.0f : NUM2FLOAT( vz );
        rt->PictureList[rt->PictureCount].z = z;
        picture->z = z;
    }

    rt->PictureCount++;

    /* RenderTargetだった場合に描画予約があればupdateする */
    RenderTerget_auto_update( argv[2] );

    /* 使われたimageのロック */
//    image->lockcount++;

    return obj;
}


static void RenderTarget_drawFont_func( struct DXRubyPicture_drawFont *picture )
{
    D3DVECTOR vector;
    D3DXMATRIX matrix;
    D3DXMATRIX matrix_t;
    RECT rect;
    struct DXRubyFont *font = DXRUBY_GET_STRUCT( Font, RARRAY_AREF( picture->value, 0 ) );
    float angle = 3.141592653589793115997963468544185161590576171875f / 180.0f * picture->angle;
    DXRUBY_CHECK_DISPOSE( font, pD3DXFont );

    /* D3DXSpriteの描画開始 */
    g_pD3DXSprite->lpVtbl->Begin( g_pD3DXSprite, D3DXSPRITE_ALPHABLEND );

    /* 回転及び拡大縮小 */
    D3DXMatrixScaling    ( &matrix_t, picture->scalex, picture->scaley, 1 );
    D3DXMatrixRotationZ  ( &matrix  , angle );
    D3DXMatrixMultiply   ( &matrix  , &matrix_t, &matrix );

    /* 平行移動 */
    D3DXMatrixTranslation( &matrix_t, (float)picture->x + picture->centerx, (float)picture->y + picture->centery, 0 );
    D3DXMatrixMultiply   ( &matrix  , &matrix, &matrix_t );

    g_pD3DXSprite->lpVtbl->SetTransform( g_pD3DXSprite, &matrix );

    rect.left   = (LONG)-picture->centerx;
    rect.top    = (LONG)-picture->centery;
    rect.right  = (LONG)picture->centerx;
    rect.bottom = (LONG)picture->centery;
    {
        int len = RSTRING_LEN( RARRAY_AREF( picture->value, 1 ) );
        char *buf = alloca( len + 2 );
        buf[len] = buf[len + 1] = 0;
        memcpy( buf, RSTRING_PTR( RARRAY_AREF(picture->value, 1) ), len );
        if( rb_enc_get_index( RARRAY_AREF(picture->value, 1) ) == 0 )
        {
            font->pD3DXFont->lpVtbl->DrawText( font->pD3DXFont, g_pD3DXSprite, buf, -1, &rect, DT_LEFT | DT_NOCLIP,
                                               ((int)picture->alpha << 24) | picture->color & 0x00ffffff);
        }
        else
        {
            font->pD3DXFont->lpVtbl->DrawTextW( font->pD3DXFont, g_pD3DXSprite, (LPCWSTR)buf, -1, &rect, DT_LEFT | DT_NOCLIP,
                                               ((int)picture->alpha << 24) | picture->color & 0x00ffffff);
        }
    }

    g_pD3DXSprite->lpVtbl->Flush( g_pD3DXSprite );

    /* ピクチャの描画終了 */
    g_pD3DXSprite->lpVtbl->End( g_pD3DXSprite );
}

/*--------------------------------------------------------------------
   フォント描画
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_drawFont( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyPicture_drawFont *picture;
    struct DXRubyFont *font;
    VALUE vcolor;
    int cr, cg, cb;
    VALUE vz, vangle, vscalex, vscaley, valpha, vcenterx, vcentery, vblend;
    VALUE voption;
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, obj );
    VALUE vstr;
    volatile VALUE temp;

    DXRUBY_CHECK_DISPOSE( rt, surface );
//    DXRUBY_CHECK_IMAGE_LOCK( rt );

    if( argc < 4 || argc > 5 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 4, 5 );
    Check_Type(argv[2], T_STRING);

    if( argc < 5 || argv[4] == Qnil )
    {
        voption = rb_hash_new();
    }
    else
    {
        Check_Type( argv[4], T_HASH );
        voption = argv[4];
    }

    vblend = hash_lookup( voption, symbol_blend );
    vangle = hash_lookup( voption, symbol_angle );
    valpha = hash_lookup( voption, symbol_alpha );
    vscalex = hash_lookup( voption, symbol_scale_x );
    vscalex = vscalex == Qnil ? hash_lookup( voption, symbol_scalex ) : vscalex;
    vscaley = hash_lookup( voption, symbol_scale_y );
    vscaley = vscaley == Qnil ? hash_lookup( voption, symbol_scaley ) : vscaley;
    vcenterx = hash_lookup( voption, symbol_center_x );
    vcenterx = vcenterx == Qnil ? hash_lookup( voption, symbol_centerx ) : vcenterx;
    vcentery = hash_lookup( voption, symbol_center_y );
    vcentery = vcentery == Qnil ? hash_lookup( voption, symbol_centery ) : vcentery;
    vz = hash_lookup( voption, symbol_z );
    vcolor = hash_lookup( voption, symbol_color );

    DXRUBY_CHECK_TYPE( Font, argv[3] );
    font = DXRUBY_GET_STRUCT( Font, argv[3] );
    DXRUBY_CHECK_DISPOSE( font, pD3DXFont );

    picture = (struct DXRubyPicture_drawFont *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_drawFont ) );
    if( picture == NULL )
    {
        rb_raise( eDXRubyError, "Out of memory" );
    }

    /* DXRubyPictureオブジェクト設定 */
    picture->func = RenderTarget_drawFont_func;
    picture->x = NUM2INT( argv[0] ) - rt->ox;
    picture->y = NUM2INT( argv[1] ) - rt->oy;
    picture->angle   = (vangle   == Qnil ? 0.0f : NUM2FLOAT( vangle ));
    picture->scalex  = (vscalex  == Qnil ? 1.0f : NUM2FLOAT( vscalex ));
    picture->scaley  = (vscaley  == Qnil ? 1.0f : NUM2FLOAT( vscaley ));
    picture->centerx = (vcenterx == Qnil ? 0.0f : NUM2FLOAT( vcenterx ));;
    picture->centery = (vcentery == Qnil ? 0.0f : NUM2FLOAT( vcentery ));;
    picture->alpha   = (valpha   == Qnil ? 0xff : NUM2INT( valpha ));
    picture->blendflag = (vblend == Qnil ? 0 :
                         (vblend == symbol_add ? 4 :
                         (vblend == symbol_none ? 1 :
                         (vblend == symbol_add2 ? 5 :
                         (vblend == symbol_sub ? 6 :
                         (vblend == symbol_sub2 ? 7 : 0))))));

    if( rb_enc_get_index( argv[2] ) != 0 )
    {
        vstr = rb_str_export_to_enc( argv[2], g_enc_utf16 );
    }
    else
    {
        vstr = rb_obj_dup( argv[2] );
    }

    picture->value = temp = rb_ary_new3( 2, argv[3], vstr );

    if( vcolor != Qnil )
    {
        Check_Type( vcolor, T_ARRAY );
        if( RARRAY_LEN( vcolor ) == 4 )
        {
            picture->alpha = NUM2INT( rb_ary_entry( vcolor, 0 ) );
            cr = NUM2INT( rb_ary_entry( vcolor, 1 ) );
            cg = NUM2INT( rb_ary_entry( vcolor, 2 ) );
            cb = NUM2INT( rb_ary_entry( vcolor, 3 ) );
        }
        else
        {
            cr = NUM2INT( rb_ary_entry( vcolor, 0 ) );
            cg = NUM2INT( rb_ary_entry( vcolor, 1 ) );
            cb = NUM2INT( rb_ary_entry( vcolor, 2 ) );
        }
    }
    else
    {
        cr = 255;
        cg = 255;
        cb = 255;
    }
    picture->color = D3DCOLOR_XRGB(cr, cg, cb);
    picture->z = 0;

    /* リストデータに追加 */
    rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
    rt->PictureList[rt->PictureCount].z = vz == Qnil ? 0.0f : NUM2FLOAT( vz );
    rt->PictureCount++;

    return obj;
}


/*--------------------------------------------------------------------
   高品質フォント描画
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_drawFontEx( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, obj );
    int fontsize, edge_width, shadow_x, shadow_y;
    VALUE vimage, voption, vedge_width, vedge, vshadow;

    DXRUBY_CHECK_DISPOSE( rt, surface );
//    DXRUBY_CHECK_IMAGE_LOCK( rt );
    DXRUBY_CHECK_TYPE( Font, argv[3] );

    if( argc < 4 || argc > 5 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 4, 5 );
    Check_Type(argv[2], T_STRING);

    if( argc < 5 || argv[4] == Qnil )
    {
        voption = rb_hash_new();
    }
    else
    {
        Check_Type( argv[4], T_HASH );
        voption = argv[4];
    }

    /* 内部Imageオブジェクト生成*/

    /* エッジオプション補正 */
    vedge = hash_lookup( voption, symbol_edge );
    if( vedge == Qnil || vedge == Qfalse )
    {
        edge_width = 0;
    }
    else
    {
        vedge_width = hash_lookup( voption, symbol_edge_width );
        edge_width = vedge_width == Qnil ? 2 : NUM2INT( vedge_width );
    }

    /* 影オプション補正 */
    vshadow = hash_lookup( voption, symbol_shadow );
    if( vshadow == Qnil || vshadow == Qfalse )
    {
        shadow_x = 0;
        shadow_y = 0;
    }
    else
    {
        VALUE vshadow_x, vshadow_y;

        vshadow_x = hash_lookup( voption, symbol_shadow_x );
        shadow_x = vshadow_x == Qnil ? NUM2INT( Font_getSize( argv[3] ) ) / 24 + 1 : NUM2INT(vshadow_x);

        vshadow_y = hash_lookup( voption, symbol_shadow_y );
        shadow_y = vshadow_y == Qnil ? NUM2INT( Font_getSize( argv[3] ) ) / 24 + 1 : NUM2INT(vshadow_y);
    }

    fontsize = NUM2INT( Font_getSize( argv[3] ) );
    vimage = Image_allocate( cImage );
    {
        struct DXRubyFont *font = DXRUBY_GET_STRUCT( Font, argv[3] );

        if( RTEST(font->vauto_fitting) )
        {
            int intBlackBoxX, intBlackBoxY, intCellIncX, intPtGlyphOriginX, intPtGlyphOriginY, intTmAscent, intTmDescent;
            Font_getInfo_internal( argv[2], font, &intBlackBoxX, &intBlackBoxY, &intCellIncX, &intPtGlyphOriginX, &intPtGlyphOriginY, &intTmAscent, &intTmDescent );

            {
                VALUE arr[2] = {INT2FIX(NUM2INT( Font_getWidth( argv[3], argv[2] ) ) + fontsize / 2 + edge_width * 2 + shadow_x), INT2FIX(intTmAscent + intTmDescent + edge_width * 2 + shadow_y)};
                Image_initialize( 2, arr, vimage );
            }
        }
        else
        {
            VALUE arr[2] = {INT2FIX(NUM2INT( Font_getWidth( argv[3], argv[2] ) ) + fontsize / 2 + edge_width * 2 + shadow_x), INT2FIX(fontsize + edge_width * 2 + shadow_y)};
            Image_initialize( 2, arr, vimage );
        }
    }

    /* Imageに描画 */
    {
        VALUE arr[5] = {INT2FIX(edge_width), INT2FIX(edge_width), argv[2], argv[3], voption};
        Image_drawFontEx( 5, arr, vimage );
    }

    /* RenderTarget_drawEx呼び出し */
    {
        VALUE arr[4] = {INT2NUM(NUM2INT(argv[0]) - edge_width), INT2NUM(NUM2INT(argv[1]) - edge_width), vimage, voption};
        RenderTarget_drawEx( 4, arr, obj );
    }

    rb_ary_push( g_WindowInfo.image_array, vimage );

    return obj;
}


static void RenderTarget_drawMorph_func( struct DXRubyPicture_drawMorph *picture )
{
    TLVERTX *VertexDataTbl;
    struct DXRubyImage *image = DXRUBY_GET_STRUCT( Image, picture->value );
    float width = (float)image->width;
    float height = (float)image->height;
    float x1 = picture->x1;
    float y1 = picture->y1;
    float x2 = picture->x2;
    float y2 = picture->y2;
    float x3 = picture->x3;
    float y3 = picture->y3;
    float x4 = picture->x4;
    float y4 = picture->y4;
    float tu1;
    float tu2;
    float tv1;
    float tv2;
    int count_x, count_y;


    DXRUBY_CHECK_DISPOSE( image, texture );
    tu1 = image->x / image->texture->width;
    tu2 = (image->x + width) / image->texture->width;
    tv1 = image->y / image->texture->height;
    tv2 = (image->y + height) / image->texture->height;

    VertexDataTbl = alloca( sizeof(TLVERTX) * picture->dividex * picture->dividey * 6 );

    for( count_y = 0; count_y < picture->dividey; count_y++ )
    {
        for( count_x = 0; count_x < picture->dividex; count_x++ )
        {
            int cur = (count_x + picture->dividex * count_y) * 6;
            float wx1 = (float)count_x / (float)picture->dividex;
            float wy1 = (float)count_y / (float)picture->dividey;
            float wx2 = (float)(count_x+1) / (float)picture->dividex;
            float wy2 = (float)(count_y+1) / (float)picture->dividey;

            float tx1 = (x2 - x1) * wx1 + x1;
            float ty1 = (y2 - y1) * wy1 + y1;
            float bx1 = (x3 - x4) * wx1 + x4;
            float by1 = (y3 - y4) * wy1 + y4;
            float lx1 = (x4 - x1) * wx1 + x1;
            float ly1 = (y4 - y1) * wy1 + y1;
            float rx1 = (x3 - x2) * wx1 + x2;
            float ry1 = (y3 - y2) * wy1 + y2;
            float tx2 = (x2 - x1) * wx2 + x1;
            float ty2 = (y2 - y1) * wy2 + y1;
            float bx2 = (x3 - x4) * wx2 + x4;
            float by2 = (y3 - y4) * wy2 + y4;
            float lx2 = (x4 - x1) * wx2 + x1;
            float ly2 = (y4 - y1) * wy2 + y1;
            float rx2 = (x3 - x2) * wx2 + x2;
            float ry2 = (y3 - y2) * wy2 + y2;

            /* 頂点１ */
            VertexDataTbl[cur+0].x = (bx1 - tx1) * wy1 + tx1 - 0.5f;
            VertexDataTbl[cur+0].y = (ry1 - ly1) * wx1 + ly1 - 0.5f;
            /* 頂点２ */
            VertexDataTbl[cur+1].x = VertexDataTbl[cur+3].x = (bx2 - tx2) * wy1 + tx2 - 0.5f;
            VertexDataTbl[cur+1].y = VertexDataTbl[cur+3].y = (ry1 - ly1) * wx2 + ly1 - 0.5f;
            /* 頂点３ */
            VertexDataTbl[cur+4].x = (bx2 - tx2) * wy2 + tx2 - 0.5f;
            VertexDataTbl[cur+4].y = (ry2 - ly2) * wx2 + ly2 - 0.5f;
            /* 頂点４ */
            VertexDataTbl[cur+2].x = VertexDataTbl[cur+5].x = (bx1 - tx1) * wy2 + tx1 - 0.5f;
            VertexDataTbl[cur+2].y = VertexDataTbl[cur+5].y = (ry2 - ly2) * wx1 + ly2 - 0.5f;
            /* 頂点色 */
            VertexDataTbl[cur+0].color = VertexDataTbl[cur+1].color =
            VertexDataTbl[cur+2].color = VertexDataTbl[cur+3].color =
            VertexDataTbl[cur+4].color = VertexDataTbl[cur+5].color = D3DCOLOR_ARGB(picture->alpha,picture->r,picture->g,picture->b);
            /* Ｚ座標 */
            VertexDataTbl[cur+0].z  = VertexDataTbl[cur+1].z =
            VertexDataTbl[cur+2].z  = VertexDataTbl[cur+3].z =
            VertexDataTbl[cur+4].z  = VertexDataTbl[cur+5].z = 0.0f;
            /* テクスチャ座標 */
            VertexDataTbl[cur+0].tu = VertexDataTbl[cur+5].tu = VertexDataTbl[cur+2].tu = tu1 + ((tu2 - tu1) / picture->dividex) * count_x;
            VertexDataTbl[cur+0].tv = VertexDataTbl[cur+1].tv = VertexDataTbl[cur+3].tv = tv1 + ((tv2 - tv1) / picture->dividey) * count_y;
            VertexDataTbl[cur+1].tu = VertexDataTbl[cur+3].tu = VertexDataTbl[cur+4].tu = tu1 + ((tu2 - tu1) / picture->dividex) * (count_x+1);
            VertexDataTbl[cur+4].tv = VertexDataTbl[cur+5].tv = VertexDataTbl[cur+2].tv = tv1 + ((tv2 - tv1) / picture->dividey) * (count_y+1);
        }
    }

    if( picture->colorflag == 1 )
    {
        g_pD3DDevice->lpVtbl->SetTextureStageState( g_pD3DDevice, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG2 );
    }

    /* テクスチャをセット */
    g_pD3DDevice->lpVtbl->SetTexture(g_pD3DDevice, 0, (IDirect3DBaseTexture9*)image->texture->pD3DTexture);

    /* デバイスに使用する頂点フォーマットをセット */
    g_pD3DDevice->lpVtbl->SetFVF(g_pD3DDevice, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);

    /* 描画 */
    g_pD3DDevice->lpVtbl->DrawPrimitiveUP(g_pD3DDevice, D3DPT_TRIANGLELIST, picture->dividex * picture->dividey * 2, VertexDataTbl, sizeof(TLVERTX));

    if( picture->colorflag == 1 )
    {
        g_pD3DDevice->lpVtbl->SetTextureStageState( g_pD3DDevice, 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
    }

    /* ロックカウントを減らす */
//    image->lockcount--;
}

/*--------------------------------------------------------------------
   描画設定（4点指定）
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_drawMorph( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyImage *image;
    struct DXRubyPicture_drawMorph *picture;
    float z;
    VALUE voption;
    VALUE vz, vdividex, vdividey, valpha, vblend, vcolor;
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, obj );
    DXRUBY_CHECK_DISPOSE( rt, surface );
//    DXRUBY_CHECK_IMAGE_LOCK( rt );

    if( argc < 9 || argc > 10 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 9, 10 );

    if( argc < 10 || argv[9] == Qnil )
    {
        voption = rb_hash_new();
    }
    else
    {
        Check_Type( argv[9], T_HASH );
        voption = argv[9];
    }

    vblend = hash_lookup( voption, symbol_blend );
    valpha = hash_lookup( voption, symbol_alpha );
    vdividex = hash_lookup( voption, symbol_dividex );
    vdividey = hash_lookup( voption, symbol_dividey );
    vz = hash_lookup( voption, symbol_z );
    vcolor = hash_lookup( voption, symbol_color );

    /* 引数のイメージオブジェクトから中身を取り出す */
    DXRUBY_CHECK_IMAGE( argv[8] );
    image = DXRUBY_GET_STRUCT( Image, argv[8] );
    DXRUBY_CHECK_DISPOSE( image, texture );

    picture = (struct DXRubyPicture_drawMorph *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_drawMorph ) );

    /* DXRubyPictureオブジェクト設定 */
    picture->func = RenderTarget_drawMorph_func;
    picture->x1 = NUM2FLOAT( argv[0] ) - rt->ox;
    picture->y1 = NUM2FLOAT( argv[1] ) - rt->oy;
    picture->x2 = NUM2FLOAT( argv[2] ) - rt->ox;
    picture->y2 = NUM2FLOAT( argv[3] ) - rt->oy;
    picture->x3 = NUM2FLOAT( argv[4] ) - rt->ox;
    picture->y3 = NUM2FLOAT( argv[5] ) - rt->oy;
    picture->x4 = NUM2FLOAT( argv[6] ) - rt->ox;
    picture->y4 = NUM2FLOAT( argv[7] ) - rt->oy;
    picture->value = argv[8];
    picture->dividex = vdividex == Qnil ? 1 : NUM2INT( vdividex );
    picture->dividey = vdividey == Qnil ? 1 : NUM2INT( vdividey );
    picture->alpha   = valpha   == Qnil ? 0xff : NUM2INT( valpha );
    picture->blendflag = (vblend == Qnil ? 0 :
                         (vblend == symbol_add ? 4 :
                         (vblend == symbol_none ? 1 :
                         (vblend == symbol_add2 ? 5 :
                         (vblend == symbol_sub ? 6 :
                         (vblend == symbol_sub2 ? 7 : 0))))));

    if( picture->dividex <= 0 || picture->dividey <= 0 )
    {
        rb_raise( eDXRubyError, "分割数に0以下は指定できません");
    }

    if( vcolor != Qnil )
    {
        Check_Type( vcolor, T_ARRAY );
        if( RARRAY_LEN( vcolor ) < 4 )
        {
            picture->r = NUM2INT( rb_ary_entry(vcolor, 0) );
            picture->g = NUM2INT( rb_ary_entry(vcolor, 1) );
            picture->b = NUM2INT( rb_ary_entry(vcolor, 2) );
        }
        else
        {
            picture->alpha = NUM2INT( rb_ary_entry(vcolor, 0) ) * picture->alpha / 255;
            picture->r = NUM2INT( rb_ary_entry(vcolor, 1) );
            picture->g = NUM2INT( rb_ary_entry(vcolor, 2) );
            picture->b = NUM2INT( rb_ary_entry(vcolor, 3) );
        }
        picture->colorflag = 1;
    }
    else
    {
        picture->r = picture->g = picture->b = 255;
        picture->colorflag = 0;
    }


    /* リストデータに追加 */
    rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
    rt->PictureList[rt->PictureCount].z = vz == Qnil ? 0.0f : NUM2FLOAT( vz );
    picture->z = vz == Qnil ? 0.0f : NUM2FLOAT( vz );
    rt->PictureCount++;

    /* RenderTargetだった場合に描画予約があればupdateする */
    RenderTerget_auto_update( argv[8] );

    /* 使われたimageのロック */
//    image->lockcount++;

    return obj;
}


void RenderTarget_drawTile_func( struct DXRubyPicture_drawTile *picture )
{
    TLVERTX VertexDataTbl[6];
    struct DXRubyImage *image;
    VALUE vmap = RARRAY_PTR(picture->value)[0];
    VALUE vmapdata = RARRAY_PTR(picture->value)[1];
    int width, height;
    VALUE *mapdata, *arr1, *arr2;
    int i, j;
    float x, y;
    int startx_mod_width;
    int startt_mod_height;

    if( RARRAY_LEN( vmap ) == 0 )
    {
        return;
    }
    if( RARRAY_LEN( vmapdata ) == 0 )
    {
        return;
    }

    mapdata = RARRAY_PTR( vmapdata );

    /* 幅と高さ取得 */
    width  = NUM2INT( rb_funcall(*mapdata, rb_intern("width"), 0) );
    height = NUM2INT( rb_funcall(*mapdata, rb_intern("height"), 0) );

    arr1 = RARRAY_PTR( vmap );

    startx_mod_width = picture->startx % width;
    startt_mod_height = picture->starty % height;

    y = picture->basey - (startt_mod_height < 0 ? height + startt_mod_height : startt_mod_height) - 0.5f;
    /* 描画 */
    for( i = startt_mod_height < 0 ? -1 : 0; i < picture->sizey + (startt_mod_height <= 0 ? 0 : 1); i++ )
    {
        int my;

        if( i + picture->starty / height < 0 )
        {
            my = (((i + picture->starty / height) % RARRAY_LEN( vmap )) + RARRAY_LEN( vmap )) % RARRAY_LEN( vmap );
        }
        else
        {
            my = (i + picture->starty / height) % RARRAY_LEN( vmap );
        }
        Check_Type(arr1[my], T_ARRAY);

        if( RARRAY_LEN( arr1[my] ) == 0 )
        {
            continue;
        }

        arr2 = RARRAY_PTR( arr1[my] );

        x = picture->basex - (startx_mod_width < 0 ? width + startx_mod_width : startx_mod_width) - 0.5f;

        for( j = startx_mod_width < 0 ? -1 : 0; j < picture->sizex + (startx_mod_width <= 0 ? 0 : 1); j++ )
        {
            int mx;
            int index;
            int len = RARRAY_LEN( arr1[my] );

            if( j + picture->startx / width < 0 )
            {
                mx = (((j + picture->startx / width) % len) + len) % len;
            }
            else
            {
                mx = (j + picture->startx / width) % len;
            }

            if( arr2[mx] != Qnil )
            {
                index = NUM2INT( arr2[mx] );

                if( index >= RARRAY_LEN( vmapdata ) ) rb_raise(eDXRubyError, "Invalid MapChipNumber - Window_drawTile");

                /* イメージオブジェクトから中身を取り出す */
                image = (struct DXRubyImage *)DATA_PTR( mapdata[index] );

                VertexDataTbl[0].x = VertexDataTbl[2].x = VertexDataTbl[5].x = x;
                VertexDataTbl[0].y = VertexDataTbl[1].y = VertexDataTbl[3].y = y;
                VertexDataTbl[1].x = VertexDataTbl[3].x = VertexDataTbl[4].x = x + width;
                VertexDataTbl[4].y = VertexDataTbl[2].y = VertexDataTbl[5].y = y + height;
                /* 頂点色 */
                VertexDataTbl[0].color = VertexDataTbl[1].color =
                VertexDataTbl[2].color = VertexDataTbl[3].color =
                VertexDataTbl[4].color = VertexDataTbl[5].color = D3DCOLOR_ARGB(255,255,255,255);
                /* Ｚ座標 */
                VertexDataTbl[0].z  = VertexDataTbl[1].z =
                VertexDataTbl[2].z  = VertexDataTbl[3].z =
                VertexDataTbl[4].z  = VertexDataTbl[5].z = 0.0f;
                /* テクスチャ座標 */
                VertexDataTbl[0].tu = VertexDataTbl[5].tu = VertexDataTbl[2].tu = image->x / image->texture->width;
                VertexDataTbl[0].tv = VertexDataTbl[1].tv = VertexDataTbl[3].tv = image->y / image->texture->height;
                VertexDataTbl[1].tu = VertexDataTbl[3].tu = VertexDataTbl[4].tu = (image->x + width) / image->texture->width;
                VertexDataTbl[4].tv = VertexDataTbl[5].tv = VertexDataTbl[2].tv = (image->y + height) / image->texture->height;

                /* テクスチャをセット */
                g_pD3DDevice->lpVtbl->SetTexture(g_pD3DDevice, 0, (IDirect3DBaseTexture9*)image->texture->pD3DTexture);

                /* デバイスに使用する頂点フォーマットをセット */
                g_pD3DDevice->lpVtbl->SetFVF(g_pD3DDevice, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);

                /* 描画 */
                g_pD3DDevice->lpVtbl->DrawPrimitiveUP(g_pD3DDevice, D3DPT_TRIANGLELIST, 2, VertexDataTbl, sizeof(TLVERTX));
            }

            x = x + width;
        }
        y = y + height;
    }

    /* ロックカウントを減らす */
//    for( i = 0; i < RARRAY_LEN( vmapdata ); i++ )
//    {
//        DXRUBY_GET_STRUCT( Image, mapdata[i] )->lockcount--;
//   }
}

/*--------------------------------------------------------------------
   マップ描画
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_drawTile( int argc, VALUE *argv, VALUE obj )
{
    VALUE vstartx, vstarty, vz;
    VALUE vbasex, vbasey, vmap, vmapdata, vsizex, vsizey;
    VALUE vmapdata_f;
    volatile VALUE temp;
    struct DXRubyPicture_drawTile *picture;
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, obj );
    int i;

    DXRUBY_CHECK_DISPOSE( rt, surface );
//    DXRUBY_CHECK_IMAGE_LOCK( rt );

    rb_scan_args( argc, argv, "81", &vbasex, &vbasey, &vmap, &vmapdata, &vstartx, &vstarty, &vsizex, &vsizey, &vz );

    Check_Type(vmap, T_ARRAY);
    Check_Type(vmapdata, T_ARRAY);

    vmapdata_f = rb_funcall( vmapdata, rb_intern("flatten"), 0 );

    /* イメージ配列が全部イメージかどうかをチェックしとく */
    for( i = 0; i < RARRAY_LEN( vmapdata_f ); i++ )
    {
        DXRUBY_CHECK_IMAGE( RARRAY_AREF(vmapdata_f, i) );
        DXRUBY_CHECK_DISPOSE( DXRUBY_GET_STRUCT( Image, RARRAY_AREF(vmapdata_f, i) ), texture );

        /* RenderTargetだった場合に描画予約があればupdateする */
        RenderTerget_auto_update( RARRAY_AREF(vmapdata_f, i) );

        /* 使われたimageのロック */
//        image->lockcount++;
   }

    picture = (struct DXRubyPicture_drawTile *)RenderTarget_AllocPictureList( rt, sizeof( struct DXRubyPicture_drawTile ) );

    /* DXRubyPictureオブジェクト設定 */
    picture->func = RenderTarget_drawTile_func;
    picture->basex = vbasex == Qnil ? 0 : (NUM2INT( vbasex ) - rt->ox);
    picture->basey = vbasey == Qnil ? 0 : (NUM2INT( vbasey ) - rt->oy);
    picture->sizex = vsizex == Qnil ? (rt->width  + DXRUBY_GET_STRUCT( Image, RARRAY_AREF(vmapdata_f, 0) )->width  - 1) / DXRUBY_GET_STRUCT( Image, RARRAY_AREF(vmapdata_f, 0) )->width  : NUM2INT( vsizex );
    picture->sizey = vsizey == Qnil ? (rt->height + DXRUBY_GET_STRUCT( Image, RARRAY_AREF(vmapdata_f, 0) )->height - 1) / DXRUBY_GET_STRUCT( Image, RARRAY_AREF(vmapdata_f, 0) )->height : NUM2INT( vsizey );
    picture->startx = vstartx == Qnil ? rt->ox : NUM2INT( vstartx );
    picture->starty = vstarty == Qnil ? rt->oy : NUM2INT( vstarty );
    temp = rb_ary_new3( 2, vmap, vmapdata_f );
    picture->value = temp;
    picture->alpha = 0xff;
    picture->blendflag = 0;

    /* リストデータに追加 */
    rt->PictureList[rt->PictureCount].picture = (struct DXRubyPicture *)picture;
    picture->z = rt->PictureList[rt->PictureCount].z = vz == Qnil ? 0.0f : NUM2FLOAT( vz );
    rt->PictureCount++;

    return obj;
}


///*--------------------------------------------------------------------
//   Sprite描画
// ---------------------------------------------------------------------*/
//static VALUE RenderTarget_drawSprite( VALUE self, VALUE varg )
//{
//    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
//    if( TYPE( varg ) == T_ARRAY )
//    {
//        int i;
//        for( i = 0; i < RARRAY_LEN( varg ); i++)
//        {
//            if( TYPE( RARRAY_PTR( varg )[i] ) == T_ARRAY )
//            {
//                RenderTarget_drawSprite( self, RARRAY_PTR( varg )[i] );
//            }
//            else
//            {
//                DXRUBY_CHECK_TYPE( Sprite, RARRAY_PTR( varg )[i] );
//                Sprite_internal_draw( RARRAY_PTR( varg )[i], self );
//            }
//        }
//    }
//    else
//    {
//        DXRUBY_CHECK_TYPE( Sprite, varg );
//        Sprite_internal_draw( varg, self );
//    }
//
//    return self;
//}


/*--------------------------------------------------------------------
   RenderTargetクリア
 ---------------------------------------------------------------------*/
VALUE RenderTarget_clear( VALUE self )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );

    g_pD3DDevice->lpVtbl->SetRenderTarget( g_pD3DDevice, 0, rt->surface );
    g_pD3DDevice->lpVtbl->Clear( g_pD3DDevice, 0, NULL, D3DCLEAR_TARGET,
                                 D3DCOLOR_ARGB( rt->a, rt->r, rt->g, rt->b ), 1.0f, 0 );
    return self;
}


/*--------------------------------------------------------------------
   RenderTarget更新
 ---------------------------------------------------------------------*/
VALUE RenderTarget_update( VALUE self )
{
    HRESULT hr;
    int x_2d, width_2d;
    int y_2d, height_2d;
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self ); /* 出力先 */
    int i;

    DXRUBY_CHECK_DISPOSE( rt, surface );

    /* シーンのクリア */
    {
        D3DVIEWPORT9 vp;
        vp.X       = x_2d = 0;
        vp.Y       = y_2d = 0;
        if( rt->texture == NULL )
        {
            vp.Width   = width_2d = g_D3DPP.BackBufferWidth;
            vp.Height  = height_2d = g_D3DPP.BackBufferHeight;
        }
        else
        {
            vp.Width   = width_2d = (int)rt->texture->width;
            vp.Height  = height_2d = (int)rt->texture->height;
            rt->clearflag = 1;
        }
        vp.MinZ    = 0.0f;
        vp.MaxZ    = 1.0f;
        g_pD3DDevice->lpVtbl->SetRenderTarget( g_pD3DDevice, 0, rt->surface );
        g_pD3DDevice->lpVtbl->SetViewport( g_pD3DDevice, &vp );
#ifdef DXRUBY15
        if( rt->vregenerate_proc == Qnil )
        {
            g_pD3DDevice->lpVtbl->Clear( g_pD3DDevice, 0, NULL, D3DCLEAR_TARGET,
                                         D3DCOLOR_ARGB( rt->a, rt->r, rt->g, rt->b ), 1.0f, 0 );
        }
#else
        g_pD3DDevice->lpVtbl->Clear( g_pD3DDevice, 0, NULL, D3DCLEAR_TARGET,
                                     D3DCOLOR_ARGB( rt->a, rt->r, rt->g, rt->b ), 1.0f, 0 );
#endif
    }

    /* シーンの描画開始 */
    if( SUCCEEDED( g_pD3DDevice->lpVtbl->BeginScene( g_pD3DDevice ) ) )
    {
        i = 0;

        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_ZENABLE,D3DZB_FALSE );
        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_ZWRITEENABLE, FALSE );
        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_LIGHTING, FALSE);
        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_FOGENABLE, FALSE );
        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_SHADEMODE, D3DSHADE_GOURAUD );
        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_SRGBWRITEENABLE, FALSE );
        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_VERTEXBLEND, FALSE );
        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_WRAP0, 0 );
        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_CULLMODE, D3DCULL_NONE);

        g_pD3DDevice->lpVtbl->SetTextureStageState( g_pD3DDevice, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
        g_pD3DDevice->lpVtbl->SetTextureStageState( g_pD3DDevice, 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
        g_pD3DDevice->lpVtbl->SetTextureStageState( g_pD3DDevice, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
        g_pD3DDevice->lpVtbl->SetTextureStageState( g_pD3DDevice, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
        g_pD3DDevice->lpVtbl->SetTextureStageState( g_pD3DDevice, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
        g_pD3DDevice->lpVtbl->SetTextureStageState( g_pD3DDevice, 0, D3DTSS_COLOROP, D3DTOP_MODULATE );

        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_ALPHABLENDENABLE, TRUE );
        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );

        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_SEPARATEALPHABLENDENABLE, TRUE );
        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_SRCBLENDALPHA, D3DBLEND_ONE );
        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA );

        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_FOGENABLE, FALSE );

        g_pD3DDevice->lpVtbl->SetSamplerState( g_pD3DDevice, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
        g_pD3DDevice->lpVtbl->SetSamplerState( g_pD3DDevice, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

        /* 拡大縮小フィルタ設定 */
        g_pD3DDevice->lpVtbl->SetSamplerState(g_pD3DDevice, 0, D3DSAMP_MINFILTER,
                                         rt->minfilter);
        g_pD3DDevice->lpVtbl->SetSamplerState(g_pD3DDevice, 0, D3DSAMP_MAGFILTER,
                                         rt->magfilter);

        if( rt->PictureCount > 0 )
        {
            D3DMATRIX matrix, matrix_t;
            int oldflag = 0;

            RenderTarget_SortPictureList( rt );

            /* 2D描画 */
            D3DXMatrixScaling    ( &matrix, 1, -1, 1 );
            D3DXMatrixTranslation( &matrix_t, (float)-(width_2d)/2.0f, (float)(height_2d)/2.0f, 0 );
            D3DXMatrixMultiply( &matrix, &matrix, &matrix_t );
            g_pD3DDevice->lpVtbl->SetTransform( g_pD3DDevice, D3DTS_VIEW, &matrix );
            matrix._11 = 2.0f / width_2d;
            matrix._12 = matrix._13 = matrix._14 = 0;
            matrix._22 = 2.0f / height_2d;
            matrix._21 = matrix._23 = matrix._24 = 0;
            matrix._31 = matrix._32 = 0;matrix._33 = 0; matrix._34 = 0;
            matrix._41 = matrix._42 = 0;matrix._43 = 1; matrix._44 = 1;
            g_pD3DDevice->lpVtbl->SetTransform( g_pD3DDevice, D3DTS_PROJECTION, &matrix );

            for( i = 0; i < rt->PictureCount; i++ )
            {
                struct DXRubyPicture_draw *temp = (struct DXRubyPicture_draw *)rt->PictureList[i].picture;

                if( temp->blendflag != oldflag ) 
                {
                    switch( temp->blendflag )
                    {
                    case 0:          /* 半透明合成 */
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_SRCBLENDALPHA, D3DBLEND_ONE );
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA );
                        break;
                    case 1:          /* 単純上書き */
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_SRCBLEND, D3DBLEND_ONE );
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_DESTBLEND, D3DBLEND_ZERO );
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_SRCBLENDALPHA, D3DBLEND_ONE );
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_DESTBLENDALPHA, D3DBLEND_ZERO );
                        break;
                    case 4:          /* 加算合成1の設定 */
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_DESTBLEND, D3DBLEND_ONE );
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_SRCBLENDALPHA, D3DBLEND_ONE );
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA );
                        break;
                    case 5:          /* 加算合成2の設定 */
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_SRCBLEND, D3DBLEND_ONE );
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_SRCBLENDALPHA, D3DBLEND_ONE );
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA );
                        break;
                    case 6:          /* 減算合成1の設定 */
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_SRCBLEND, D3DBLEND_ZERO );
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_SRCBLENDALPHA, D3DBLEND_ONE );
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA );
                        break;
                    case 7:          /* 減算合成2の設定 */
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_SRCBLEND, D3DBLEND_ZERO );
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_DESTBLEND, D3DBLEND_INVSRCCOLOR );
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_SRCBLENDALPHA, D3DBLEND_ONE );
                        g_pD3DDevice->lpVtbl->SetRenderState( g_pD3DDevice, D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA );
                        break;
                    }
                }

                oldflag = temp->blendflag;
                temp->func( temp );
            }
        }

        /* シーンの描画終了 */
        g_pD3DDevice->lpVtbl->EndScene( g_pD3DDevice );
    }

    rt->PictureCount = 0;
    rt->PictureSize = 0;
    rt->PictureDecideCount = 0;
    rt->PictureDecideSize = 0;

    return self;
}


/*--------------------------------------------------------------------
   縮小フィルタ取得
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_getMinFilter( VALUE self )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
    DXRUBY_CHECK_DISPOSE( rt, surface );
    return INT2FIX( rt->minfilter );
}


/*--------------------------------------------------------------------
   縮小フィルタ設定
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_setMinFilter( VALUE self, VALUE minfilter )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
    DXRUBY_CHECK_DISPOSE( rt, surface );
    rt->minfilter = FIX2INT( minfilter );
    return minfilter;
}


/*--------------------------------------------------------------------
   拡大フィルタ取得
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_getMagFilter( VALUE self )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
    DXRUBY_CHECK_DISPOSE( rt, surface );
    return INT2FIX( rt->magfilter );
}


/*--------------------------------------------------------------------
   拡大フィルタ設定
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_setMagFilter( VALUE self, VALUE magfilter )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
    DXRUBY_CHECK_DISPOSE( rt, surface );
    rt->magfilter = FIX2INT( magfilter );
    return magfilter;
}


/*--------------------------------------------------------------------
   描画予約確定
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_decide( VALUE self )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
    DXRUBY_CHECK_DISPOSE( rt, surface );
    rt->PictureDecideCount = rt->PictureCount;
    rt->PictureDecideSize = rt->PictureSize;
    return self;
}


/*--------------------------------------------------------------------
   描画予約破棄
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_discard( VALUE self )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
    DXRUBY_CHECK_DISPOSE( rt, surface );
    rt->PictureCount = rt->PictureDecideCount;
    rt->PictureSize = rt->PictureDecideSize;
    return self;
}


#ifdef DXRUBY15
/*--------------------------------------------------------------------
   再生成Proc取得
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_getRegenerateProc( VALUE self )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
    DXRUBY_CHECK_DISPOSE( rt, surface );
    return rt->vregenerate_proc;
}


/*--------------------------------------------------------------------
   再生成Proc設定
 ---------------------------------------------------------------------*/
static VALUE RenderTarget_setRegenerateProc( VALUE self, VALUE vregenerate_proc )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, self );
    DXRUBY_CHECK_DISPOSE( rt, surface );
    rt->vregenerate_proc = vregenerate_proc;
    return vregenerate_proc;
}
#endif


/*--------------------------------------------------------------------
  （内部関数）フレーム調整初期化
 ---------------------------------------------------------------------*/
static void InitSync( void )
{
    timeBeginPeriod( 1 );

    /* パフォーマンスカウンタの秒間カウント値取得 */
    if( QueryPerformanceFrequency( (LARGE_INTEGER *)&g_OneSecondCount ) )
    {
        /* パフォーマンスカウンタがある場合 */
        g_isPerformanceCounter = 1;
        QueryPerformanceCounter( (LARGE_INTEGER *)&g_OldTime );
        g_DrawEndTime = g_OldTime;
    }
    else
    {
        /* パフォーマンスカウンタが無い場合 */
        g_isPerformanceCounter = 0;
        g_OneSecondCount = 1000;
        g_OldTime = timeGetTime();
    }
}




/*
***************************************************************
*
*         Global functions
*
***************************************************************/

void Init_dxruby()
{
    HRESULT hr;
    int i, j;

    /* インスタンスハンドル取得 */
    g_hInstance = (HINSTANCE)GetModuleHandle( NULL );

    /* DXRubyモジュール登録 */
    mDXRuby = rb_define_module( "DXRuby" );

    /* 例外定義 */
    eDXRubyError = rb_define_class_under( mDXRuby, "DXRubyError", rb_eRuntimeError );

//    hr = CoInitializeEx( NULL, COINIT_MULTITHREADED );
    hr = CoInitialize(NULL);

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "initialize error - CoInitialize" );
    }

    /* システムのエンコード取得 */
    strcpy( sys_encode, "CP" );
    itoa( GetACP(), sys_encode+2, 10 );

    /* Windowモジュール登録 */
    mWindow = rb_define_module_under( mDXRuby, "Window" );

    /* Windowモジュールにメソッド登録 */
    rb_define_singleton_method( mWindow, "loop"     , Window_loop       , -1  );
    rb_define_singleton_method( mWindow, "draw"     , Window_draw       , -1 );
    rb_define_singleton_method( mWindow, "draw_scale", Window_drawScale  , -1 );
    rb_define_singleton_method( mWindow, "drawScale", Window_drawScale  , -1 );
    rb_define_singleton_method( mWindow, "draw_rot" , Window_drawRot    , -1 );
    rb_define_singleton_method( mWindow, "drawRot"  , Window_drawRot    , -1 );
    rb_define_singleton_method( mWindow, "draw_alpha", Window_drawAlpha  , -1 );
    rb_define_singleton_method( mWindow, "drawAlpha", Window_drawAlpha  , -1 );
    rb_define_singleton_method( mWindow, "draw_add" , Window_drawAdd    , -1 );
    rb_define_singleton_method( mWindow, "drawAdd"  , Window_drawAdd    , -1 );
    rb_define_singleton_method( mWindow, "draw_sub" , Window_drawSub    , -1 );
    rb_define_singleton_method( mWindow, "drawSub"  , Window_drawSub    , -1 );
    rb_define_singleton_method( mWindow, "draw_ex"  , Window_drawEx     , -1 );
    rb_define_singleton_method( mWindow, "drawEx"   , Window_drawEx     , -1 );
    rb_define_singleton_method( mWindow, "draw_font", Window_drawFont   , -1 );
    rb_define_singleton_method( mWindow, "drawFont" , Window_drawFont   , -1 );
#ifdef DXRUBY15
    rb_define_singleton_method( mWindow, "draw_text", Window_drawFont   , -1 );
    rb_define_singleton_method( mWindow, "drawText" , Window_drawFont   , -1 );
#endif
    rb_define_singleton_method( mWindow, "draw_font_ex", Window_drawFontEx , -1 );
    rb_define_singleton_method( mWindow, "drawFontEx" , Window_drawFontEx , -1 );
#ifdef DXRUBY15
    rb_define_singleton_method( mWindow, "draw_text_ex", Window_drawFontEx , -1 );
    rb_define_singleton_method( mWindow, "drawTextEx" , Window_drawFontEx , -1 );
#endif
    rb_define_singleton_method( mWindow, "draw_tile", Window_drawTile   , -1  );
    rb_define_singleton_method( mWindow, "drawTile" , Window_drawTile   , -1  );
#ifdef DXRUBY15
    rb_define_singleton_method( mWindow, "draw_tiles", Window_drawTile   , -1  );
    rb_define_singleton_method( mWindow, "drawTiles" , Window_drawTile   , -1  );
#endif
    rb_define_singleton_method( mWindow, "draw_morph", Window_drawMorph , -1 );
    rb_define_singleton_method( mWindow, "drawMorph" , Window_drawMorph , -1 );
    rb_define_singleton_method( mWindow, "draw_pixel", Window_drawPixel , -1 );
    rb_define_singleton_method( mWindow, "drawPixel" , Window_drawPixel , -1 );
    rb_define_singleton_method( mWindow, "draw_line" , Window_drawLine , -1 );
    rb_define_singleton_method( mWindow, "drawLine"  , Window_drawLine , -1 );
    rb_define_singleton_method( mWindow, "draw_box" , Window_drawBox , -1 );
    rb_define_singleton_method( mWindow, "drawBox"  , Window_drawBox , -1 );
    rb_define_singleton_method( mWindow, "draw_box_fill" , Window_drawBoxFill , -1 );
    rb_define_singleton_method( mWindow, "drawBoxFill"  , Window_drawBoxFill , -1 );
    rb_define_singleton_method( mWindow, "draw_circle", Window_drawCircle , -1 );
    rb_define_singleton_method( mWindow, "drawCircle" , Window_drawCircle , -1 );
    rb_define_singleton_method( mWindow, "draw_circle_fill", Window_drawCircleFill , -1 );
    rb_define_singleton_method( mWindow, "drawCircleFill"  , Window_drawCircleFill , -1 );
    rb_define_singleton_method( mWindow, "x"        , Window_x          , 0  );
    rb_define_singleton_method( mWindow, "x="       , Window_setx       , 1  );
    rb_define_singleton_method( mWindow, "y"        , Window_y          , 0  );
    rb_define_singleton_method( mWindow, "y="       , Window_sety       , 1  );
    rb_define_singleton_method( mWindow, "width"    , Window_width      , 0  );
    rb_define_singleton_method( mWindow, "width="   , Window_setwidth   , 1  );
    rb_define_singleton_method( mWindow, "height"   , Window_height     , 0  );
    rb_define_singleton_method( mWindow, "height="  , Window_setheight  , 1  );
    rb_define_singleton_method( mWindow, "caption"  , Window_getCaption , 0  );
    rb_define_singleton_method( mWindow, "caption=" , Window_setCaption , 1  );
    rb_define_singleton_method( mWindow, "scale"    , Window_getScale   , 0  );
    rb_define_singleton_method( mWindow, "scale="   , Window_setScale   , 1  );
    rb_define_singleton_method( mWindow, "windowed?", Window_getwindowed, 0  );
    rb_define_singleton_method( mWindow, "windowed=", Window_setwindowed, 1  );
    rb_define_singleton_method( mWindow, "full_screen?", Window_getfullscreen, 0  );
    rb_define_singleton_method( mWindow, "full_screen=", Window_setfullscreen, 1  );
    rb_define_singleton_method( mWindow, "real_fps" , Window_getrealfps , 0  );
    rb_define_singleton_method( mWindow, "fps"      , Window_getfps     , 0  );
    rb_define_singleton_method( mWindow, "fps="     , Window_setfps     , 1  );
    rb_define_singleton_method( mWindow, "frameskip?"   , Window_getframeskip, 0  );
    rb_define_singleton_method( mWindow, "frameskip="   , Window_setframeskip, 1  );
    rb_define_singleton_method( mWindow, "bgcolor"      , Window_get_bgcolor , 0  );
    rb_define_singleton_method( mWindow, "bgcolor="     , Window_set_bgcolor , 1  );
    rb_define_singleton_method( mWindow, "min_filter"   , Window_getMinFilter , 0 );
    rb_define_singleton_method( mWindow, "minFilter"    , Window_getMinFilter , 0 );
    rb_define_singleton_method( mWindow, "min_filter="  , Window_setMinFilter , 1 );
    rb_define_singleton_method( mWindow, "minFilter="   , Window_setMinFilter , 1 );
    rb_define_singleton_method( mWindow, "mag_filter"   , Window_getMagFilter , 0 );
    rb_define_singleton_method( mWindow, "magFilter"    , Window_getMagFilter , 0 );
    rb_define_singleton_method( mWindow, "mag_filter="  , Window_setMagFilter , 1 );
    rb_define_singleton_method( mWindow, "magFilter="   , Window_setMagFilter , 1 );
    rb_define_singleton_method( mWindow, "get_load"     , Window_getload, 0  );
    rb_define_singleton_method( mWindow, "getLoad"      , Window_getload, 0  );
    rb_define_singleton_method( mWindow, "open_filename", Window_openDialog, 2 );
    rb_define_singleton_method( mWindow, "openFilename" , Window_openDialog, 2 );
    rb_define_singleton_method( mWindow, "save_filename", Window_saveDialog, 2 );
    rb_define_singleton_method( mWindow, "saveFilename" , Window_saveDialog, 2 );
    rb_define_singleton_method( mWindow, "get_screen_shot", Window_getScreenShot, -1 );
    rb_define_singleton_method( mWindow, "getScreenShot", Window_getScreenShot, -1 );
    rb_define_singleton_method( mWindow, "create"       , Window_create , 0 );
    rb_define_singleton_method( mWindow, "close"        , Window_close , 0 );
    rb_define_singleton_method( mWindow, "created?"     , Window_get_created , 0 );
    rb_define_singleton_method( mWindow, "closed?"      , Window_get_closed , 0 );
    rb_define_singleton_method( mWindow, "update"       , Window_update , 0 );
    rb_define_singleton_method( mWindow, "sync"         , Window_sync , 0 );
    rb_define_singleton_method( mWindow, "hWnd"         , Window_gethWnd, 0  );
    rb_define_singleton_method( mWindow, "load_icon"    , Window_loadIcon, 1  );
    rb_define_singleton_method( mWindow, "loadIcon"     , Window_loadIcon, 1  );
    rb_define_singleton_method( mWindow, "resize"   , Window_resize    , 2  );
    rb_define_singleton_method( mWindow, "active?"   , Window_get_active, 0  );
    rb_define_singleton_method( mWindow, "getScreenModes", Window_getScreenModes, 0  );
    rb_define_singleton_method( mWindow, "get_screen_modes", Window_getScreenModes, 0  );
    rb_define_singleton_method( mWindow, "getCurrentMode", Window_getCurrentMode, 0  );
    rb_define_singleton_method( mWindow, "get_current_mode", Window_getCurrentMode, 0  );
    rb_define_singleton_method( mWindow, "decide"   , Window_decide   , 0 );
    rb_define_singleton_method( mWindow, "discard"  , Window_discard  , 0 );
    rb_define_singleton_method( mWindow, "running_time" , Window_running_time  , 0 );
    rb_define_singleton_method( mWindow, "runningTime" , Window_running_time  , 0 );
    rb_define_singleton_method( mWindow, "before_call"    , Window_before_call, 0 );
    rb_define_singleton_method( mWindow, "after_call"    , Window_after_call, 0 );
    rb_define_singleton_method( mWindow, "ox"           , Window_getOx , 0 );
    rb_define_singleton_method( mWindow, "ox="          , Window_setOx , 1 );
    rb_define_singleton_method( mWindow, "oy"           , Window_getOy, 0 );
    rb_define_singleton_method( mWindow, "oy="          , Window_setOy, 1 );
#ifdef DXRUBY15
    rb_define_singleton_method( mWindow, "test_device_lost" , Window_test_device_lost, 0 );
#endif
//    rb_define_singleton_method( mWindow, "to_image", Window_to_image, 0 );
//    rb_define_singleton_method( mWindow, "toImage", Window_to_image, 0 );

    rb_define_singleton_method( mWindow, "draw_shader", Window_drawShader, -1 );
    rb_define_singleton_method( mWindow, "drawShader" , Window_drawShader, -1 );


    rb_define_singleton_method( mWindow, "folderDialog" , Window_folderDialog, -1 );
    rb_define_singleton_method( mWindow, "folder_dialog" , Window_folderDialog, -1 );

    /* Shaderクラス定義 */
    cShader = rb_define_class_under( mDXRuby, "Shader", rb_cObject );

    /* Shaderクラスにメソッド登録*/
    rb_define_private_method( cShader, "initialize", Shader_initialize, -1 );
    rb_define_method( cShader, "technique"   , Shader_getTechnique   , 0 );
    rb_define_method( cShader, "technique="  , Shader_setTechnique   , 1 );

    /* Shaderオブジェクトを生成した時にinitializeの前に呼ばれるメモリ割り当て関数登録 */
    rb_define_alloc_func( cShader, Shader_allocate );


    /* ShaderCoreクラス定義 */
    cShaderCore = rb_define_class_under( cShader, "Core", rb_cObject );

    /* ShaderCoreクラスにメソッド登録*/
    rb_define_private_method( cShaderCore, "initialize", ShaderCore_initialize, -1 );
    rb_define_method( cShaderCore, "dispose"   , ShaderCore_dispose, 0 );
    rb_define_method( cShaderCore, "disposed?" , ShaderCore_check_disposed, 0 );
    rb_define_method( cShaderCore, "param"     , ShaderCore_getParam  , 0 );

    /* ShaderCoreオブジェクトを生成した時にinitializeの前に呼ばれるメモリ割り当て関数登録 */
    rb_define_alloc_func( cShaderCore, ShaderCore_allocate );


    /* RenderTargetクラス定義 */
    cRenderTarget = rb_define_class_under( mDXRuby, "RenderTarget", rb_cObject );

    /* RenderTargetクラスにメソッド登録*/
    rb_define_private_method( cRenderTarget, "initialize", RenderTarget_initialize, -1 );
    rb_define_method( cRenderTarget, "dispose"   , RenderTarget_dispose   , 0 );
    rb_define_method( cRenderTarget, "disposed?" , RenderTarget_check_disposed, 0 );
    rb_define_method( cRenderTarget, "width"     , RenderTarget_getWidth  , 0 );
    rb_define_method( cRenderTarget, "height"    , RenderTarget_getHeight , 0 );
    rb_define_method( cRenderTarget, "to_image"  , RenderTarget_to_image  , 0 );
    rb_define_method( cRenderTarget, "toImage"   , RenderTarget_to_image  , 0 );
    rb_define_method( cRenderTarget, "resize"    , RenderTarget_resize    , 2 );

    rb_define_method( cRenderTarget, "draw"     , RenderTarget_draw       , -1 );
    rb_define_method( cRenderTarget, "draw_scale", RenderTarget_drawScale  , -1 );
    rb_define_method( cRenderTarget, "drawScale", RenderTarget_drawScale  , -1 );
    rb_define_method( cRenderTarget, "draw_rot" , RenderTarget_drawRot    , -1 );
    rb_define_method( cRenderTarget, "drawRot"  , RenderTarget_drawRot    , -1 );
    rb_define_method( cRenderTarget, "draw_alpha", RenderTarget_drawAlpha  , -1 );
    rb_define_method( cRenderTarget, "drawAlpha", RenderTarget_drawAlpha  , -1 );
    rb_define_method( cRenderTarget, "draw_add" , RenderTarget_drawAdd    , -1 );
    rb_define_method( cRenderTarget, "drawAdd"  , RenderTarget_drawAdd    , -1 );
    rb_define_method( cRenderTarget, "draw_sub" , RenderTarget_drawSub    , -1 );
    rb_define_method( cRenderTarget, "drawSub"  , RenderTarget_drawSub    , -1 );
    rb_define_method( cRenderTarget, "draw_ex"  , RenderTarget_drawEx     , -1 );
    rb_define_method( cRenderTarget, "drawEx"   , RenderTarget_drawEx     , -1 );
    rb_define_method( cRenderTarget, "draw_font", RenderTarget_drawFont   , -1 );
    rb_define_method( cRenderTarget, "drawFont" , RenderTarget_drawFont   , -1 );
    rb_define_method( cRenderTarget, "draw_font_ex", RenderTarget_drawFontEx , -1 );
    rb_define_method( cRenderTarget, "drawFontEx" , RenderTarget_drawFontEx , -1 );
    rb_define_method( cRenderTarget, "draw_text", RenderTarget_drawFont   , -1 );
    rb_define_method( cRenderTarget, "drawText" , RenderTarget_drawFont   , -1 );
    rb_define_method( cRenderTarget, "draw_text_ex", RenderTarget_drawFontEx , -1 );
    rb_define_method( cRenderTarget, "drawTextEx" , RenderTarget_drawFontEx , -1 );
    rb_define_method( cRenderTarget, "draw_tile", RenderTarget_drawTile   , -1  );
    rb_define_method( cRenderTarget, "drawTile" , RenderTarget_drawTile   , -1  );
    rb_define_method( cRenderTarget, "draw_tiles", RenderTarget_drawTile   , -1  );
    rb_define_method( cRenderTarget, "drawTiles" , RenderTarget_drawTile   , -1  );
    rb_define_method( cRenderTarget, "draw_morph", RenderTarget_drawMorph , -1 );
    rb_define_method( cRenderTarget, "drawMorph" , RenderTarget_drawMorph , -1 );
    rb_define_method( cRenderTarget, "draw_pixel", RenderTarget_drawPixel , -1 );
    rb_define_method( cRenderTarget, "drawPixel" , RenderTarget_drawPixel , -1 );
    rb_define_method( cRenderTarget, "draw_line" , RenderTarget_drawLine , -1 );
    rb_define_method( cRenderTarget, "drawLine"  , RenderTarget_drawLine , -1 );
    rb_define_method( cRenderTarget, "draw_box" , RenderTarget_drawBox , -1 );
    rb_define_method( cRenderTarget, "drawBox"  , RenderTarget_drawBox , -1 );
    rb_define_method( cRenderTarget, "draw_box_fill" , RenderTarget_drawBoxFill , -1 );
    rb_define_method( cRenderTarget, "drawBoxFill"  , RenderTarget_drawBoxFill , -1 );
    rb_define_method( cRenderTarget, "draw_circle"  , RenderTarget_drawCircle , -1 );
    rb_define_method( cRenderTarget, "drawCircle"   , RenderTarget_drawCircle , -1 );
    rb_define_method( cRenderTarget, "draw_circle_fill", RenderTarget_drawCircleFill , -1 );
    rb_define_method( cRenderTarget, "drawCircleFill"  , RenderTarget_drawCircleFill , -1 );
    rb_define_method( cRenderTarget, "update"       , RenderTarget_update , 0 );
    rb_define_method( cRenderTarget, "bgcolor"      , RenderTarget_get_bgcolor , 0  );
    rb_define_method( cRenderTarget, "bgcolor="     , RenderTarget_set_bgcolor , 1  );
    rb_define_method( cRenderTarget, "min_filter"   , RenderTarget_getMinFilter , 0 );
    rb_define_method( cRenderTarget, "minFilter"    , RenderTarget_getMinFilter , 0 );
    rb_define_method( cRenderTarget, "min_filter="  , RenderTarget_setMinFilter , 1 );
    rb_define_method( cRenderTarget, "minFilter="   , RenderTarget_setMinFilter , 1 );
    rb_define_method( cRenderTarget, "mag_filter"   , RenderTarget_getMagFilter , 0 );
    rb_define_method( cRenderTarget, "magFilter"    , RenderTarget_getMagFilter , 0 );
    rb_define_method( cRenderTarget, "mag_filter="  , RenderTarget_setMagFilter , 1 );
    rb_define_method( cRenderTarget, "magFilter="   , RenderTarget_setMagFilter , 1 );
    rb_define_method( cRenderTarget, "decide"       , RenderTarget_decide   , 0 );
    rb_define_method( cRenderTarget, "discard"      , RenderTarget_discard  , 0 );
    rb_define_method( cRenderTarget, "ox"           , RenderTarget_getOx , 0 );
    rb_define_method( cRenderTarget, "ox="          , RenderTarget_setOx , 1 );
    rb_define_method( cRenderTarget, "oy"           , RenderTarget_getOy, 0 );
    rb_define_method( cRenderTarget, "oy="          , RenderTarget_setOy, 1 );
#ifdef DXRUBY15
    rb_define_method( cRenderTarget, "regenerate_proc" , RenderTarget_getRegenerateProc , 0  );
    rb_define_method( cRenderTarget, "regenerate_proc=", RenderTarget_setRegenerateProc , 1  );
    rb_define_method( cRenderTarget, "regenerateProc" , RenderTarget_getRegenerateProc , 0  );
    rb_define_method( cRenderTarget, "regenerateProc=", RenderTarget_setRegenerateProc , 1  );
#endif
    rb_define_method( cRenderTarget, "clear"        , RenderTarget_clear, 0 );

    rb_define_method( cRenderTarget, "draw_shader", RenderTarget_drawShader, -1 );
    rb_define_method( cRenderTarget, "drawShader" , RenderTarget_drawShader, -1 );


    /* RenderTargetオブジェクトを生成した時にinitializeの前に呼ばれるメモリ割り当て関数登録 */
    rb_define_alloc_func( cRenderTarget, RenderTarget_allocate );


    rb_define_const( mDXRuby, "FORMAT_JPEG"  , INT2FIX(FORMAT_JPEG));
    rb_define_const( mDXRuby, "FORMAT_JPG"   , INT2FIX(FORMAT_JPG) );
    rb_define_const( mDXRuby, "FORMAT_PNG"   , INT2FIX(FORMAT_PNG) );
    rb_define_const( mDXRuby, "FORMAT_BMP"   , INT2FIX(FORMAT_BMP) );
    rb_define_const( mDXRuby, "FORMAT_DDS"   , INT2FIX(FORMAT_DDS) );

    rb_define_const( mDXRuby, "TEXF_POINT"   , INT2FIX(D3DTEXF_POINT) );
    rb_define_const( mDXRuby, "TEXF_LINEAR"   , INT2FIX(D3DTEXF_LINEAR) );
    rb_define_const( mDXRuby, "TEXF_GAUSSIANQUAD", INT2FIX(D3DTEXF_GAUSSIANQUAD) );

    rb_define_const( mDXRuby, "C_BLACK"      , rb_ary_new3(4, INT2FIX(255), INT2FIX(0),   INT2FIX(0),   INT2FIX(0))   );
    rb_define_const( mDXRuby, "C_RED"        , rb_ary_new3(4, INT2FIX(255), INT2FIX(255), INT2FIX(0),   INT2FIX(0))   );
    rb_define_const( mDXRuby, "C_GREEN"      , rb_ary_new3(4, INT2FIX(255), INT2FIX(0),   INT2FIX(255), INT2FIX(0))   );
    rb_define_const( mDXRuby, "C_BLUE"       , rb_ary_new3(4, INT2FIX(255), INT2FIX(0),   INT2FIX(0),   INT2FIX(255)) );
    rb_define_const( mDXRuby, "C_YELLOW"     , rb_ary_new3(4, INT2FIX(255), INT2FIX(255), INT2FIX(255), INT2FIX(0))   );
    rb_define_const( mDXRuby, "C_CYAN"       , rb_ary_new3(4, INT2FIX(255), INT2FIX(0),   INT2FIX(255), INT2FIX(255)) );
    rb_define_const( mDXRuby, "C_MAGENTA"    , rb_ary_new3(4, INT2FIX(255), INT2FIX(255), INT2FIX(0),   INT2FIX(255)) );
    rb_define_const( mDXRuby, "C_WHITE"      , rb_ary_new3(4, INT2FIX(255), INT2FIX(255), INT2FIX(255), INT2FIX(255)) );
    rb_define_const( mDXRuby, "C_DEFAULT"    , rb_ary_new3(4, INT2FIX(0), INT2FIX(0), INT2FIX(0), INT2FIX(0))  );

    /* 定数登録 */
    rb_define_const( mDXRuby, "VERSION", rb_str_new2( DXRUBY_VERSION ) );

    {
        VALUE temp;
        temp = rb_eval_string( "$dxruby_no_include" );
        if( temp == Qfalse || temp == Qnil )
        {
            rb_include_module( rb_cObject, mDXRuby );
        }
    }

    /* シンボル定義 */
    symbol_blend          = ID2SYM(rb_intern("blend"));
    symbol_angle          = ID2SYM(rb_intern("angle"));
    symbol_alpha          = ID2SYM(rb_intern("alpha"));
    symbol_scalex         = ID2SYM(rb_intern("scalex"));
    symbol_scaley         = ID2SYM(rb_intern("scaley"));
    symbol_centerx        = ID2SYM(rb_intern("centerx"));
    symbol_centery        = ID2SYM(rb_intern("centery"));
    symbol_scale_x        = ID2SYM(rb_intern("scale_x"));
    symbol_scale_y        = ID2SYM(rb_intern("scale_y"));
    symbol_center_x       = ID2SYM(rb_intern("center_x"));
    symbol_center_y       = ID2SYM(rb_intern("center_y"));
    symbol_z              = ID2SYM(rb_intern("z"));
    symbol_color          = ID2SYM(rb_intern("color"));
    symbol_add            = ID2SYM(rb_intern("add"));
    symbol_add2           = ID2SYM(rb_intern("add2"));
    symbol_sub            = ID2SYM(rb_intern("sub"));
    symbol_sub2           = ID2SYM(rb_intern("sub2"));
    symbol_none           = ID2SYM(rb_intern("none"));
    symbol_offset_sync    = ID2SYM(rb_intern("offset_sync"));
    symbol_dividex        = ID2SYM(rb_intern("dividex"));
    symbol_dividey        = ID2SYM(rb_intern("dividey"));
    symbol_divide_x       = ID2SYM(rb_intern("divide_x"));
    symbol_divide_y       = ID2SYM(rb_intern("divide_y"));
    symbol_edge           = ID2SYM(rb_intern("edge"));
    symbol_edge_color     = ID2SYM(rb_intern("edge_color"));
    symbol_edge_width     = ID2SYM(rb_intern("edge_width"));
    symbol_edge_level     = ID2SYM(rb_intern("edge_level"));
    symbol_shadow         = ID2SYM(rb_intern("shadow"));
    symbol_shadow_color   = ID2SYM(rb_intern("shadow_color"));
    symbol_shadow_x       = ID2SYM(rb_intern("shadow_x"));
    symbol_shadow_y       = ID2SYM(rb_intern("shadow_y"));
    symbol_shadow_edge    = ID2SYM(rb_intern("shadow_edge"));
    symbol_shader         = ID2SYM(rb_intern("shader"));
    symbol_int            = ID2SYM(rb_intern("int"));
    symbol_float          = ID2SYM(rb_intern("float"));
    symbol_texture        = ID2SYM(rb_intern("texture"));
    symbol_technique      = ID2SYM(rb_intern("technique"));
    symbol_discard        = ID2SYM(rb_intern("discard"));
    symbol_aa             = ID2SYM(rb_intern("aa"));
    symbol_call           = ID2SYM(rb_intern("call"));

    /* 終了時に実行する関数 */
    rb_set_end_proc( Window_shutdown, Qnil );

    /* ロストリスト */
    g_RenderTargetList.pointer = malloc( sizeof(void*) * 16 );
    g_RenderTargetList.count = 0;
    g_RenderTargetList.allocate_size = 16;
    g_ShaderCoreList.pointer = malloc( sizeof(void*) * 16 );
    g_ShaderCoreList.count = 0;
    g_ShaderCoreList.allocate_size = 16;

    g_WindowInfo.x = CW_USEDEFAULT;
    g_WindowInfo.y = CW_USEDEFAULT;
    g_WindowInfo.width    = 640;
    g_WindowInfo.height   = 480;
    g_WindowInfo.windowed = 1;
    g_WindowInfo.created  = 0;
    g_WindowInfo.scale    = 1;
    g_WindowInfo.enablemouse = 1;
    g_WindowInfo.mousewheelpos = 0;
    g_WindowInfo.fps = 60;
    g_WindowInfo.frameskip = Qfalse;
    g_WindowInfo.hIcon = 0;
    g_WindowInfo.input_updated = 0;
    g_WindowInfo.requestclose = 0;
    g_WindowInfo.render_target = RenderTarget_allocate( cRenderTarget );
    rb_global_variable( &g_WindowInfo.render_target );
    g_WindowInfo.active = 0;
    g_WindowInfo.before_call = rb_hash_new();
    g_WindowInfo.after_call = rb_hash_new();
    rb_global_variable( &g_WindowInfo.before_call );
    rb_global_variable( &g_WindowInfo.after_call );
    g_WindowInfo.image_array = rb_ary_new();
    rb_global_variable( &g_WindowInfo.image_array );

    g_enc_sys = rb_enc_find( sys_encode );
    g_enc_utf16 = rb_enc_find( "UTF-16LE" );
    g_enc_utf8 = rb_enc_find( "UTF-8" );

    /* BG色の初期化 */
    Window_set_bgcolor( mWindow, rb_ary_new3( 3, INT2FIX( 0 ), INT2FIX( 0 ), INT2FIX( 0 ) ) );

    /* 元のマウスカーソル取得 */
    mouse_cursor = GetCursor();

    InitMessageThread();
    Window_setDefaultIcon();
    Init_dxruby_Input();
    Init_dxruby_Sound();
    Init_dxruby_Font();
    Init_dxruby_Image();
    Init_dxruby_Sprite();
#ifdef DXRUBY15
    Init_dxruby_Matrix();
#endif
//    Init_dxruby_Movie();

    InitSync();

    /* 円描画用Shader生成 */
    Window_createCircleShader();

    /* 塗りつぶし円描画用Shader生成 */
    Window_createCircleFillShader();
}

/* Ruby1.8の古いほう対応 */
VALUE hash_lookup(VALUE hash, VALUE key)
{
    VALUE val;

    if (!RHASH_TBL(hash) || !st_lookup(RHASH_TBL(hash), key, &val)) {
        return Qnil;
    }
    return val;
}

/*--------------------------------------------------------------------
    イメージのロック
 ---------------------------------------------------------------------*/
void *DXRuby_lock( VALUE vimage, char **address, int *pitch, int *width, int *height )
{
    struct DXRubyImage *image;
    D3DLOCKED_RECT texrect;
    RECT rect;

    /* 引数のイメージオブジェクトから中身を取り出す */
    if( TYPE( vimage ) != T_DATA ) rb_raise(rb_eTypeError, "wrong argument type %s (expected DXRuby::Image)", rb_obj_classname( vimage ));
    image = (struct DXRubyImage *)DATA_PTR( vimage );
    if( DXRUBY_CHECK( Image, vimage ) )
    {
      rb_raise(rb_eTypeError, "wrong argument type %s (expected DXRuby::Image)", rb_obj_classname( vimage ));
    }

    rect.left = (long)image->x;
    rect.top = (long)image->y;
    rect.right = (long)(image->x + image->width);
    rect.bottom = (long)(image->y + image->height);
    image->texture->pD3DTexture->lpVtbl->LockRect( image->texture->pD3DTexture, 0, &texrect, &rect, 0 );

    //    *((int*)((char *)texrect.pBits + x * 4 + y * texrect.Pitch)) = col;

    *address = (char *)texrect.pBits;
    *pitch = texrect.Pitch;
    *width = (int)image->width;
    *height = (int)image->height;

    return image->texture->pD3DTexture;
}


/*--------------------------------------------------------------------
    イメージのアンロック
 ---------------------------------------------------------------------*/
void DXRuby_unlock( void *texture )
{
    ((LPDIRECT3DTEXTURE9)texture)->lpVtbl->UnlockRect( (LPDIRECT3DTEXTURE9)texture, 0 );
}

unsigned char icon_bitmap_data[] = {
  0x28, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0xc4, 0x0e, 0x00, 0x00, 0xc4, 0x0e, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xb3, 0xb3, 
  0xb3, 0xa0, 0xa0, 0xa0, 0xb3, 0xb3, 0xb3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xb3, 0xb3, 0xb3, 0xa0, 0xa0, 0xa0, 0xb3, 
  0xb3, 0xb3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xb3, 0xb3, 0xb3, 0xa0, 0xa0, 
  0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xb3, 0xb3, 0xb3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xb3, 0xb3, 0xb3, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 
  0xa0, 0xa0, 0xb3, 0xb3, 0xb3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 
  0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xb3, 0xb3, 0xb3, 
  0xb3, 0xb3, 0xb3, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 
  0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 
  0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 
  0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 
  0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xa0, 0xa0, 0xa0, 0xb3, 0xb3, 
  0xb3, 0xc6, 0xc6, 0xc6, 0xec, 0xec, 0xec, 0xc6, 0xc6, 0xc6, 0xb3, 0xb3, 0xb3, 0xec, 0xec, 0xec, 
  0xec, 0xec, 0xec, 0xb3, 0xb3, 0xb3, 0xc6, 0xc6, 0xc6, 0xec, 0xec, 0xec, 0xc6, 0xc6, 0xc6, 0xb3, 
  0xb3, 0xb3, 0xa0, 0xa0, 0xa0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xa0, 0xa0, 0xa0, 0xc6, 0xc6, 
  0xc6, 0xff, 0xff, 0xff, 0xa0, 0xa0, 0xa0, 0xff, 0xff, 0xff, 0xc6, 0xc6, 0xc6, 0xa0, 0xa0, 0xa0, 
  0xa0, 0xa0, 0xa0, 0xc6, 0xc6, 0xc6, 0xff, 0xff, 0xff, 0xa0, 0xa0, 0xa0, 0xff, 0xff, 0xff, 0xc6, 
  0xc6, 0xc6, 0xa0, 0xa0, 0xa0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xa0, 0xa0, 0xa0, 0xec, 0xec, 
  0xec, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xec, 0xec, 0xec, 0xa0, 0xa0, 0xa0, 
  0xa0, 0xa0, 0xa0, 0xec, 0xec, 0xec, 0xa0, 0xa0, 0xa0, 0xff, 0xff, 0xff, 0xa0, 0xa0, 0xa0, 0xec, 
  0xec, 0xec, 0xa0, 0xa0, 0xa0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xa0, 0xa0, 0xa0, 0xc6, 0xc6, 
  0xc6, 0xff, 0xff, 0xff, 0xa0, 0xa0, 0xa0, 0xff, 0xff, 0xff, 0xc6, 0xc6, 0xc6, 0xa0, 0xa0, 0xa0, 
  0xa0, 0xa0, 0xa0, 0xc6, 0xc6, 0xc6, 0xff, 0xff, 0xff, 0xa0, 0xa0, 0xa0, 0xff, 0xff, 0xff, 0xc6, 
  0xc6, 0xc6, 0xa0, 0xa0, 0xa0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xd9, 0xd9, 0xd9, 0xb3, 0xb3, 
  0xb3, 0xc6, 0xc6, 0xc6, 0xec, 0xec, 0xec, 0xc6, 0xc6, 0xc6, 0xb3, 0xb3, 0xb3, 0xa0, 0xa0, 0xa0, 
  0xa0, 0xa0, 0xa0, 0xb3, 0xb3, 0xb3, 0xc6, 0xc6, 0xc6, 0xec, 0xec, 0xec, 0xc6, 0xc6, 0xc6, 0xb3, 
  0xb3, 0xb3, 0xd9, 0xd9, 0xd9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xd9, 0xd9, 
  0xd9, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 
  0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xd9, 
  0xd9, 0xd9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xd9, 0xd9, 0xd9, 0xb3, 0xb3, 0xb3, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 
  0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xb3, 0xb3, 0xb3, 0xd9, 0xd9, 0xd9, 0xff, 0xff, 0xff, 0xa0, 
  0xa0, 0xa0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xd9, 0xd9, 0xd9, 0xa0, 0xa0, 0xa0, 
  0xb3, 0xb3, 0xb3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xa0, 0xa0, 0xa0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xa0, 0xa0, 0xa0, 0xd9, 0xd9, 0xd9, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc6, 
  0xc6, 0xc6, 0xa0, 0xa0, 0xa0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xd9, 0xd9, 0xd9, 0xa0, 0xa0, 0xa0, 
  0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 
  0xa0, 0xa0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
};

unsigned char icon_bitmap_mask_data[] = {
  0xfc, 0x03,
  0xf8, 0x01,
  0xf8, 0x00,
  0xf0, 0x00,
  0xc0, 0x01,
  0x80, 0x01,
  0x80, 0x00,
  0x00, 0x00,
  0x00, 0x00,
  0x00, 0x00,
  0x00, 0x00,
  0x00, 0x00,
  0x00, 0x00,
  0x00, 0x00,
  0x83, 0xc1,
  0xc7, 0xe3,
};

/*--------------------------------------------------------------------
  デフォルトウィンドウアイコンを設定する
 ---------------------------------------------------------------------*/
static void Window_setDefaultIcon( void )
{
    HBITMAP hbmicon, hbmmask;
    BITMAP bm_mask;
    HICON hicon;
    ICONINFO iconinfo;
    HDC hdc;

    bm_mask.bmType = 0;
    bm_mask.bmWidth = 16;
    bm_mask.bmHeight = 16;
    bm_mask.bmWidthBytes = 2;
    bm_mask.bmPlanes = 1;
    bm_mask.bmBitsPixel = 1;
    bm_mask.bmBits = icon_bitmap_mask_data;
    hbmmask = CreateBitmapIndirect( &bm_mask );

    if( !hbmmask )
    {
        rb_raise( eDXRubyError, "internal error - set_default_icon" );
    }

    hdc = CreateDC( "DISPLAY", 0, 0, 0 );

    if( !hdc )
    {
        rb_raise( eDXRubyError, "internal error - set_default_icon" );
    }

    hbmicon = CreateDIBitmap( hdc, (BITMAPINFOHEADER*)icon_bitmap_data, CBM_INIT, icon_bitmap_data + 40, (BITMAPINFO*)icon_bitmap_data, DIB_RGB_COLORS );

    if( !hbmicon )
    {
        rb_raise( eDXRubyError, "internal error - set_default_icon" );
    }

    iconinfo.fIcon = 1;
    iconinfo.xHotspot = 0;
    iconinfo.yHotspot = 0;
    iconinfo.hbmMask = hbmmask;
    iconinfo.hbmColor = hbmicon;

    hicon = CreateIconIndirect( &iconinfo );

    if( hicon == NULL )
    {
        rb_raise( eDXRubyError, "internal error - set_default_icon" );
    }

    SendMessage( g_hWnd, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)hicon );
    g_WindowInfo.hIcon = hicon;
    DeleteDC( hdc );
    DeleteObject( hbmicon );
    DeleteObject( hbmmask );
}


/*--------------------------------------------------------------------
  内部用CircleShaderを生成する。
 ---------------------------------------------------------------------*/
static void Window_createCircleShader(void)
{
    LPD3DXBUFFER pErr = NULL;
    char *hlsl = "  float4 color;"
                "  float p;"
                "  float4 PS(float2 input : TEXCOORD0) : COLOR0"
                "  {"
                "    float temp = atan2(0.5 - input.y ,0.5 - input.x);"
                "    clip(float2(0.5 - distance(float2(0.5, 0.5), input), distance(float2(0.5+cos(temp)*p, 0.5+sin(temp)*p), input) - 0.5));"
                "    return color;"
                "  }"
                "  technique"
                "  {"
                "   pass"
                "   {"
                "    PixelShader = compile ps_2_0 PS();"
                "   }"
                "  }";

    if( FAILED( D3DXCreateEffect(
        g_pD3DDevice, hlsl, strlen( hlsl ), NULL, NULL,
        0 , NULL, &g_WindowInfo.pD3DXEffectCircleShader, &pErr )))
    {
        // シェーダの読み込みの失敗
        rb_raise( eDXRubyError, pErr ? pErr->lpVtbl->GetBufferPointer( pErr ) : "D3DXCreateEffect failed");
    }

    RELEASE( pErr );

    return;
}


/*--------------------------------------------------------------------
  内部用CircleFillShaderを生成する。
 ---------------------------------------------------------------------*/
static void Window_createCircleFillShader(void)
{
    LPD3DXBUFFER pErr = NULL;
    char *hlsl = "  float4 color;"
                "  float4 PS(float2 input : TEXCOORD0) : COLOR0"
                "  {"
                "    clip(0.5 - distance(float2(0.5, 0.5), input));"
                "    return color;"
                "  }"
                "  technique"
                "  {"
                "   pass"
                "   {"
                "    PixelShader = compile ps_2_0 PS();"
                "   }"
                "  }";

    if( FAILED( D3DXCreateEffect(
        g_pD3DDevice, hlsl, strlen( hlsl ), NULL, NULL,
        0 , NULL, &g_WindowInfo.pD3DXEffectCircleFillShader, &pErr )))
    {
        // シェーダの読み込みの失敗
        rb_raise( eDXRubyError, pErr ? pErr->lpVtbl->GetBufferPointer( pErr ) : "D3DXCreateEffect failed");
    }

    RELEASE( pErr );

    return;
}

