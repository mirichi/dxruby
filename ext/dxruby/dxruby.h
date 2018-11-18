#include "ruby/version.h"
#include "ruby/encoding.h"
#include <stdlib.h>
#include <mmsystem.h>
#include <tchar.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <memory.h>
#include <math.h>
#include <d3d9types.h>
#include <WINNLS32.H>
//#include <dshow.h>

#include "version.h"

/* Ruby1.8の古いほう対応 */
#ifndef RSTRING_PTR
#  define RSTRING_PTR(s) (RSTRING(s)->ptr)
#endif
#ifndef RSTRING_LEN
#  define RSTRING_LEN(s) (RSTRING(s)->len)
#endif

#ifndef RARRAY_PTR
#  define RARRAY_PTR(s) (RARRAY(s)->ptr)
#endif
#ifndef RARRAY_LEN
#  define RARRAY_LEN(s) (RARRAY(s)->len)
#endif

#ifndef RARRAY_AREF
#  define RARRAY_AREF(a, i) (RARRAY_PTR(a)[i])
#endif

#ifndef RARRAY_ASET
#  define RARRAY_ASET(a, i, v) (RARRAY_PTR(a)[i] = v)
#endif

#ifndef RARRAY_PTR_USE_START
#  define RARRAY_PTR_USE_START(a) /* */
#endif

#ifndef RARRAY_PTR_USE_END
#  define RARRAY_PTR_USE_END(a) /* */
#endif

#ifndef RARRAY_PTR_USE
#  define RARRAY_PTR_USE(ary, ptr_name, expr) do { \
    expr; \
} while (0)
#endif

#ifndef OBJ_WRITE
#  define OBJ_WRITE(obj, slot, data) *slot=data;
#endif

#ifdef RUBY_VERSION_MAJOR
#  if !(RUBY_VERSION_MAJOR == 1 || RUBY_API_VERSION_MAJOR == 1 || (RUBY_API_VERSION_MAJOR == 2 && RUBY_API_VERSION_MINOR == 0))
#    define DXRUBY_USE_TYPEDDATA
#  endif
#endif

#ifdef DXRUBY_USE_TYPEDDATA
#  define DXRUBY_CHECK_TYPE( a, b ) \
    if( !RB_TYPE_P(b, T_DATA) || RTYPEDDATA_TYPE(b) != &a##_data_type) {\
        rb_raise(rb_eTypeError, "wrong argument type %s (expected DXRuby::"#a")", rb_obj_classname( b ));}
#  define DXRUBY_CHECK_IMAGE( a ) \
    if( !RB_TYPE_P(a, T_DATA) || (RTYPEDDATA_TYPE(a) != &Image_data_type && RTYPEDDATA_TYPE(a) != &RenderTarget_data_type) ) {\
        rb_raise(rb_eTypeError, "wrong argument type %s (expected DXRuby::Image or DXRuby::RenderTarget)", rb_obj_classname( a ));}
#  define DXRUBY_GET_STRUCT( c, v ) ((struct DXRuby##c *)RTYPEDDATA_DATA( v ))
#  define DXRUBY_CHECK( a, b ) ( RB_TYPE_P(b, T_DATA) && RTYPEDDATA_TYPE(b) == &a##_data_type )
#else
#  define DXRUBY_CHECK_TYPE( a, b ) \
        {if( TYPE( b ) != T_DATA || RDATA( b )->dfree != (RUBY_DATA_FUNC)a##_release )\
        rb_raise(rb_eTypeError, "wrong argument type %s (expected DXRuby::"#a")", rb_obj_classname( b ));}
#  define DXRUBY_CHECK_IMAGE( a ) \
        {if( TYPE( a ) != T_DATA || (RDATA( a )->dfree != (RUBY_DATA_FUNC)Image_release && RDATA( a )->dfree != (RUBY_DATA_FUNC)RenderTarget_release) )\
        rb_raise(rb_eTypeError, "wrong argument type %s (expected DXRuby::Image or DXRuby::RenderTarget)", rb_obj_classname( a ));}
#  define DXRUBY_GET_STRUCT( c, v ) ((struct DXRuby##c *)DATA_PTR( v ))
#  define DXRUBY_CHECK( a, b ) ( TYPE( b ) == T_DATA && RDATA( b )->dfree == (RUBY_DATA_FUNC)a##_release )
#endif

#define DXRUBY_CHECK_DISPOSE( a, b ) {if( a->b == NULL ) { rb_raise( eDXRubyError, "disposed object" );}}
#define NUM2FLOAT( x ) ((float)( FIXNUM_P(x) ? FIX2INT(x) : NUM2DBL(x) ))
#define RELEASE( x )  if( x ) { if(FAILED(x->lpVtbl->Release( x )))rb_raise(eDXRubyError,"release error"); x = NULL; }
#define DXRUBY_RETRY_START {retry_flag = FALSE;do {
#define DXRUBY_RETRY_END if( FAILED( hr ) && !retry_flag ){rb_gc_start();retry_flag = TRUE;}else{retry_flag = FALSE;}} while( retry_flag );}

#define FORMAT_JPEG D3DXIFF_JPG
#define FORMAT_JPG  D3DXIFF_JPG
#define FORMAT_PNG  D3DXIFF_PNG
#define FORMAT_BMP  D3DXIFF_BMP
#define FORMAT_DDS  D3DXIFF_DDS

#define IME_BUF_SIZE 1024
#define IME_VK_BUF_SIZE 1024

struct DXRubyWindowInfo {
    int x;              /* ｘ座標 */
    int y;              /* ｙ座標 */
    int width;          /* 幅     */
    int height;         /* 高さ   */
    int windowed;       /* ウィンドウモード時にtrue */
    int created;        /* ウィンドウを作成したらtrue */
    float scale;        /* ウィンドウのサイズ倍率 */
//    int RefreshRate;    /* リフレッシュレート */
    int enablemouse;    /* マウスを表示するかどうか */
    int mousewheelpos;  /* マウスホイールの位置 */
    int fps;            /* fps */
    int fpscheck;       /* 現在のfps */
    int frameskip;      /* コマ落ち制御フラグ */
    HANDLE hIcon;       /* ウィンドウアイコンハンドル */
    int input_updated;  /* 入力更新したら1 */
    int requestclose;   /* ウィンドウが閉じられたら1 */
    VALUE render_target; /* スクリーンレンダーターゲット */
    VALUE before_call;   /* 毎フレーム自動で最初に呼ばれる */
    VALUE after_call;    /* 毎フレーム自動で最後に呼ばれる */
    VALUE image_array;   /* DrawFontExによる自動生成Image置き場 */
    int active;         /* ゲーム調整用 */
    LPD3DXEFFECT pD3DXEffectCircleShader; /* 円描画用Shader */
    LPD3DXEFFECT pD3DXEffectCircleFillShader; /* 塗りつぶし円描画用Shader */
};

/* ピクチャ配列 */
static struct DXRubyPictureList {
    float z;                        /* ピクチャのZ座標 */
    struct DXRubyPicture *picture;    /* ピクチャ構造体へのポインタ */
};

/* テクスチャデータ */
struct DXRubyTexture {
    LPDIRECT3DTEXTURE9 pD3DTexture;     /* ピクチャに使うテクスチャ   */
    float width;
    float height;
    int refcount;
};

/* RenderTargetオブジェクトの中身 */
struct DXRubyRenderTarget {
    struct DXRubyTexture *texture;
    int x;     /* x始点位置      */
    int y;     /* y始点位置      */
    int width; /* イメージの幅   */
    int height;/* イメージの高さ */
//    int   lockcount;  /* ロックカウント ここまでImageと共通 */
    IDirect3DSurface9 *surface;

    int PictureCount;                  /* ピクチャの登録数 */
    int PictureAllocateCount;          /* ピクチャ登録のメモリ確保数 */
    int PictureSize;                   /* ピクチャデータの使用済みサイズ */
    int PictureAllocateSize;           /* ピクチャデータのメモリ確保サイズ */
    char *PictureStruct;

    struct DXRubyPictureList *PictureList;

    int minfilter;      /* 縮小フィルタ */
    int magfilter;      /* 拡大フィルタ */

    int a;              /* 背景クリア色 α成分 */
    int r;              /* 背景クリア色 赤成分 */
    int g;              /* 背景クリア色 緑成分 */
    int b;              /* 背景クリア色 青成分 */

#ifdef DXRUBY15
    VALUE vregenerate_proc;
#endif

    int PictureDecideCount;            /* ピクチャの登録確定数 */
    int PictureDecideSize;             /* ピクチャデータの登録確定サイズ */
    int clearflag;                     /* 1フレーム1回のクリア処理をやったかどうか */

    int ox;                            /* ビュー変換補正x */
    int oy;                            /* ビュー変換補正y */
};

struct DXRubyPicture_drawLine {
    void (*func)(void*);
    VALUE value;
    unsigned char blendflag; /* 半透明(000)、加算合成1(100)、加算合成2(101)、減算合成1(110)、減算合成2(111)のフラグ */
    unsigned char alpha;     /* アルファ（透明）値 */
    char reserve1;           /* 予約3 */
    char reserve2;           /* 予約4 */
    int x1;
    int y1;
    int x2;
    int y2;
    float z;
    int col;
};

struct DXRubyPicture_drawCircle {
    void (*func)(void*);
    VALUE value;
    unsigned char blendflag; /* 半透明(000)、加算合成1(100)、加算合成2(101)、減算合成1(110)、減算合成2(111)のフラグ */
    unsigned char alpha;     /* アルファ（透明）値 */
    char reserve1;           /* 予約3 */
    char reserve2;           /* 予約4 */
    int x;
    int y;
    int r;
    float z;
    int col;
};

struct DXRubyPicture_draw {
    void (*func)(void*);
    VALUE value;
    unsigned char blendflag; /* 半透明(000)、加算合成1(100)、加算合成2(101)、減算合成1(110)、減算合成2(111)のフラグ */
    unsigned char alpha;     /* アルファ（透明）値 */
    char reserve1;           /* 予約3 */
    char reserve2;           /* 予約4 */
    int x;
    int y;
    float z;
};

struct DXRubyPicture_drawEx {
    void (*func)(void*);
    VALUE value;
    unsigned char blendflag; /* 半透明(000)、加算合成1(100)、加算合成2(101)、減算合成1(110)、減算合成2(111)のフラグ */
    unsigned char alpha;     /* アルファ（透明）値 */
    char reserve1;           /* 予約3 */
    char reserve2;           /* 予約4 */
    int x;
    int y;
    float z;
    float scalex;
    float scaley;
    float centerx;
    float centery;
    float angle;
};

struct DXRubyPicture_drawFont {
    void (*func)(void*);
    VALUE value;
    unsigned char blendflag; /* 半透明(000)、加算合成1(100)、加算合成2(101)、減算合成1(110)、減算合成2(111)のフラグ */
    unsigned char alpha;     /* アルファ（透明）値 */
    char reserve1;           /* 予約3 */
    char reserve2;           /* 予約4 */
    int x;
    int y;
    int z;
    float scalex;
    float scaley;
    float centerx;
    float centery;
    float angle;
    int color;                  /* フォントの色 */
};

struct DXRubyPicture_drawMorph {
    void (*func)(void*);
    VALUE value;
    unsigned char blendflag; /* 半透明(000)、加算合成1(100)、加算合成2(101)、減算合成1(110)、減算合成2(111)のフラグ */
    unsigned char alpha;     /* アルファ（透明）値 */
    char reserve1;           /* 予約3 */
    char reserve2;           /* 予約4 */
    float x1;
    float y1;
    float x2;
    float y2;
    float x3;
    float y3;
    float x4;
    float y4;
    float z;
    int dividex;
    int dividey;
    char colorflag, r, g, b;
};

struct DXRubyPicture_drawTile {
    void (*func)(void*);
    VALUE value;
    unsigned char blendflag; /* 半透明(000)、加算合成1(100)、加算合成2(101)、減算合成1(110)、減算合成2(111)のフラグ */
    unsigned char alpha;     /* アルファ（透明）値 */
    char reserve1;           /* 予約3 */
    char reserve2;           /* 予約4 */
    int basex;
    int basey;
    int sizex;
    int sizey;
    int startx;
    int starty;
    float z;
};

/* ShaderCore */
struct DXRubyShaderCore {
    LPD3DXEFFECT pD3DXEffect;
    VALUE vtype; /* 引数名と型のセット */
};

/* Shader */
struct DXRubyShader {
    VALUE vcore;
    VALUE vparam; /* 引数名と中身のセット*/
    VALUE vname;
};


#ifdef DXRUBY_EXTERN
extern HINSTANCE g_hInstance; /* アプリケーションインスタンス   */
extern HANDLE    g_hWnd;      /* ウィンドウハンドル             */
extern int g_iRefAll;        /* インターフェースの参照カウント */

extern LPDIRECT3D9           g_pD3D;       /* Direct3Dインターフェイス       */
extern LPDIRECT3DDEVICE9     g_pD3DDevice; /* Direct3DDeviceインターフェイス */
extern D3DPRESENT_PARAMETERS g_D3DPP;      /* D3DDeviceの設定                */
extern LPD3DXSPRITE   g_pD3DXSprite; /* D3DXSprite                     */
extern struct DXRubyLostList {
    void **pointer;
    int allocate_size;
    int count;
} g_RenderTargetList, g_ShaderCoreList;
extern int g_sync;         /* 垂直同期モード = 1                 */
extern int retry_flag;
extern BYTE g_byMouseState_L_buf;
extern BYTE g_byMouseState_M_buf;
extern BYTE g_byMouseState_R_buf;

/* エンコーディング情報 */
extern rb_encoding *g_enc_sys;
extern rb_encoding *g_enc_utf16;
extern rb_encoding *g_enc_utf8;

extern struct DXRubyWindowInfo g_WindowInfo;
extern char sys_encode[256];

extern VALUE mDXRuby;        /* DXRubyモジュール     */
extern VALUE eDXRubyError;   /* 例外                 */
extern VALUE mWindow;       /* ウィンドウモジュール */
extern VALUE cRenderTarget; /* レンダーターゲットクラス */
extern VALUE cShaderCore;   /* シェーダコアクラス   */
extern VALUE cShader;   /* シェーダクラス       */

/* シンボル */
extern VALUE symbol_blend;
extern VALUE symbol_angle;
extern VALUE symbol_alpha;
extern VALUE symbol_scalex;
extern VALUE symbol_scale_x;
extern VALUE symbol_scaley;
extern VALUE symbol_scale_y;
extern VALUE symbol_centerx;
extern VALUE symbol_center_x;
extern VALUE symbol_centery;
extern VALUE symbol_center_y;
extern VALUE symbol_z;
extern VALUE symbol_color;
extern VALUE symbol_add;
extern VALUE symbol_add2;
extern VALUE symbol_sub;
extern VALUE symbol_sub2;
extern VALUE symbol_none;
extern VALUE symbol_offset_sync;
extern VALUE symbol_dividex;
extern VALUE symbol_dividey;
extern VALUE symbol_edge;
extern VALUE symbol_edge_color;
extern VALUE symbol_edge_width;
extern VALUE symbol_edge_level;
extern VALUE symbol_shadow;
extern VALUE symbol_shadow_color;
extern VALUE symbol_shadow_x;
extern VALUE symbol_shadow_y;
extern VALUE symbol_shadow_edge;
extern VALUE symbol_shader;
extern VALUE symbol_int;
extern VALUE symbol_float;
extern VALUE symbol_texture;
extern VALUE symbol_technique;
extern VALUE symbol_discard;
extern VALUE symbol_aa;
extern VALUE symbol_call;

extern int MainThreadError;
#endif

void *RenderTarget_AllocPictureList( struct DXRubyRenderTarget *rt, int size );
void RenderTarget_draw_func( struct DXRubyPicture_draw *picture );
void RenderTarget_drawShader_func( struct DXRubyPicture_draw *picture );
void RenderTarget_drawEx_func( struct DXRubyPicture_drawEx *picture );
void RenderTarget_release( struct DXRubyRenderTarget* rt );
void ShaderCore_release( struct DXRubyShaderCore *core );
void Shader_release( struct DXRubyShader *shader );
VALUE RenderTarget_update( VALUE self );
//int Window_drawShader_func_foreach_lock( VALUE key, VALUE value, VALUE obj );

VALUE hash_lookup(VALUE hash, VALUE key);

