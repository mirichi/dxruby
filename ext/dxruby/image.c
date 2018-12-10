#define WINVER 0x0500                                  /* バージョン定義 Windows2000以上 */
#define _WIN32_WINNT WINVER

#include "ruby.h"
#ifndef RUBY_ST_H
#include "st.h"
#endif

#define DXRUBY_EXTERN 1
#include "dxruby.h"
#include "image.h"
#include "font.h"

VALUE cImage;        /* イメージクラス       */

#ifdef DXRUBY_USE_TYPEDDATA
extern rb_data_type_t Font_data_type;
#endif

/* 色 */
struct DXRubyColor {
    unsigned char blue;
    unsigned char green;
    unsigned char red;
    unsigned char alpha;
};


/*********************************************************************
 * Imageクラス
 *
 * 描画用の画像を保持するクラス。
 * ファイル名を渡してloadすると読み込まれ、Window::drawに渡して描画する。
 *********************************************************************/

/*--------------------------------------------------------------------
   参照されなくなったときにGCから呼ばれる関数
 ---------------------------------------------------------------------*/
static void Image_free( struct DXRubyImage *image )
{
    /* テクスチャオブジェクトの開放 */
    image->texture->refcount--;
    if( image->texture->refcount == 0 )
    {
        RELEASE( image->texture->pD3DTexture );
        free( image->texture );
        image->texture = NULL;
    }
}

void Image_release( struct DXRubyImage *image )
{
    if( image->texture )
    {
        Image_free( image );
    }
    free( image );
    image = NULL;

    g_iRefAll--;
    if( g_iRefAll == 0 )
    {
        CoUninitialize();
    }
}

#ifdef DXRUBY_USE_TYPEDDATA
const rb_data_type_t Image_data_type = {
    "Image",
    {
    0,
    Image_release,
    0,
    },
    NULL, NULL
};
#endif

/*--------------------------------------------------------------------
   Imageクラスのdispose。
 ---------------------------------------------------------------------*/
VALUE Image_dispose( VALUE self )
{
    struct DXRubyImage *image = DXRUBY_GET_STRUCT( Image, self );
    DXRUBY_CHECK_DISPOSE( image, texture );
    Image_free( image );
    return self;
}


/*--------------------------------------------------------------------
   Imageクラスのdisposed?。
 ---------------------------------------------------------------------*/
static VALUE Image_check_disposed( VALUE self )
{
    if( DXRUBY_GET_STRUCT( Image, self )->texture == NULL )
    {
        return Qtrue;
    }

    return Qfalse;
}

///*--------------------------------------------------------------------
//   Imageクラスのlocked?
// ---------------------------------------------------------------------*/
//static VALUE Image_check_locked( VALUE self )
//{
//    if( DXRUBY_GET_STRUCT( Image, self )->lockcount == 1 )
//    {
//        return Qtrue;
//    }
//
//    return Qfalse;
//}

/*--------------------------------------------------------------------
   Imageクラスのdelayed_dispose。
 ---------------------------------------------------------------------*/
static VALUE Image_delayed_dispose( VALUE self )
{
    struct DXRubyRenderTarget *rt = DXRUBY_GET_STRUCT( RenderTarget, g_WindowInfo.render_target );
    struct DXRubyImage *image = DXRUBY_GET_STRUCT( Image, self );
    DXRUBY_CHECK_DISPOSE( image, texture );

    rb_ary_push( g_WindowInfo.image_array, self );

    return self;
}


/*--------------------------------------------------------------------
   Textureロード
 ---------------------------------------------------------------------*/
static struct DXRubyTexture *Image_textureload( char *filename, D3DXIMAGE_INFO *psrcinfo)
{
    HRESULT hr;
    struct DXRubyTexture *texture;

    /* テクスチャメモリ取得 */
    texture = (struct  DXRubyTexture *)malloc( sizeof( struct DXRubyTexture ) );

    if( texture == NULL )
    {
        rb_raise( eDXRubyError, "Out of memory - Image_textureload" );
    }

    /* ファイルを読み込んでテクスチャオブジェクトを作成する */
    DXRUBY_RETRY_START;
    hr = D3DXCreateTextureFromFileEx( g_pD3DDevice, filename,psrcinfo->Width, psrcinfo->Height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                      D3DX_DEFAULT,  D3DX_DEFAULT, 0,
                                      0, 0, &texture->pD3DTexture);
    DXRUBY_RETRY_END;
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Load error - %s", filename );
    }

    return texture;
}


/*--------------------------------------------------------------------
   イメージオブジェクトのdup/clone
 ---------------------------------------------------------------------*/
VALUE Image_initialize_copy( VALUE self, VALUE obj )
{
    struct DXRubyImage *srcimage;
    struct DXRubyImage *dstimage;
    struct DXRubyTexture *texture;
    D3DLOCKED_RECT srctrect;
    D3DLOCKED_RECT dsttrect;
    int i, j;
    RECT srcrect;
    RECT dstrect;
    HRESULT hr;
    D3DSURFACE_DESC desc;
    int *psrc;
    int *pdst;

    DXRUBY_CHECK_TYPE( Image, obj );

    dstimage = DXRUBY_GET_STRUCT( Image, self );
//    DXRUBY_CHECK_DISPOSE( dstimage, texture );
    srcimage = DXRUBY_GET_STRUCT( Image, obj );
    DXRUBY_CHECK_DISPOSE( srcimage, texture );

    g_iRefAll++;

    /* テクスチャメモリ取得 */
    texture = (struct  DXRubyTexture *)malloc( sizeof( struct DXRubyTexture ) );
    if( texture == NULL )
    {
        rb_raise( eDXRubyError, "Out of memory - Image_dup" );
    }

    DXRUBY_RETRY_START;
    /* テクスチャオブジェクトを作成する */
    hr = D3DXCreateTexture( g_pD3DDevice, srcimage->width, srcimage->height,
                            1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                            &texture->pD3DTexture);
    DXRUBY_RETRY_END;
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Create texture error - Image_dup" );
    }

    texture->refcount = 1;

    texture->pD3DTexture->lpVtbl->GetLevelDesc(texture->pD3DTexture, 0, &desc );
    texture->width = (float)desc.Width;
    texture->height = (float)desc.Height;

    dstimage->texture = texture;
    dstimage->x = 0;
    dstimage->y = 0;
    dstimage->width = srcimage->width;
    dstimage->height =srcimage-> height;
//    dstimage->lockcount = 0;

    /* イメージコピー */
    dstrect.left = 0;
    dstrect.top = 0;
    dstrect.right = srcimage->width;
    dstrect.bottom = srcimage->height;
    srcrect.left = srcimage->x;
    srcrect.top = srcimage->y;
    srcrect.right = srcimage->x + srcimage->width;
    srcrect.bottom = srcimage->y + srcimage->height;

    dstimage->texture->pD3DTexture->lpVtbl->LockRect( dstimage->texture->pD3DTexture, 0, &dsttrect, &dstrect, 0 );
    srcimage->texture->pD3DTexture->lpVtbl->LockRect( srcimage->texture->pD3DTexture, 0, &srctrect, &srcrect, D3DLOCK_READONLY );

    for( i = 0; i < srcimage->height; i++)
    {
        psrc = (int*)((char *)srctrect.pBits + i * srctrect.Pitch);
        pdst = (int*)((char *)dsttrect.pBits + i * dsttrect.Pitch);
        for( j = 0; j < srcimage->width; j++)
        {
            *(pdst++) = *(psrc++);
        }
    }

    dstimage->texture->pD3DTexture->lpVtbl->UnlockRect( dstimage->texture->pD3DTexture, 0 );
    srcimage->texture->pD3DTexture->lpVtbl->UnlockRect( srcimage->texture->pD3DTexture, 0 );

    return self;
}


/*--------------------------------------------------------------------
   Imageクラスのallocate。メモリを確保する為にinitialize前に呼ばれる。
 ---------------------------------------------------------------------*/
VALUE Image_allocate( VALUE klass )
{
    VALUE obj;
    struct DXRubyImage *image;

    /* DXRubyImageのメモリ取得＆Imageオブジェクト生成 */
    image = malloc(sizeof(struct DXRubyImage));
    if( image == NULL ) rb_raise( eDXRubyError, "malloc error - Image_allocate" );
#ifdef DXRUBY_USE_TYPEDDATA
    obj = TypedData_Wrap_Struct( klass, &Image_data_type, image );
#else
    obj = Data_Wrap_Struct(klass, 0, Image_release, image);
#endif
    /* とりあえずテクスチャオブジェクトはNULLにしておく */
    image->texture = NULL;
//    image->lockcount = 0;

    return obj;
}


/*--------------------------------------------------------------------
    イメージのデータ設定(内部処理用box描画)
 ---------------------------------------------------------------------*/
static void fill( int x1, int y1, int x2, int y2, int col, struct DXRubyImage *image )
{
    D3DLOCKED_RECT texrect;
    int x, y;
    RECT rect;
    int *p;

    rect.left = x1 + image->x;
    rect.top = y1 + image->y;
    rect.right = x2 + image->x + 1;
    rect.bottom = y2 + image->y + 1;
    image->texture->pD3DTexture->lpVtbl->LockRect( image->texture->pD3DTexture, 0, &texrect, &rect, 0 );
    for( y = 0; y <= y2 - y1; y++ )
    {
        p = (int*)((char *)texrect.pBits + y * texrect.Pitch);
        for( x = 0; x <= x2 - x1; x++ )
        {
            *(p++) = col;
        }
    }
    image->texture->pD3DTexture->lpVtbl->UnlockRect( image->texture->pD3DTexture, 0 );
    return;
}


/*--------------------------------------------------------------------
    配列から色取得
 ---------------------------------------------------------------------*/
int array2color( VALUE color )
{
    int col;

    Check_Type(color, T_ARRAY);

    if( RARRAY_LEN( color ) == 3 )
    {
        col = D3DCOLOR_ARGB(255, NUM2INT(rb_ary_entry(color, 0)), NUM2INT(rb_ary_entry(color, 1)), NUM2INT(rb_ary_entry(color, 2)));
    }
    else
    {
        col = D3DCOLOR_ARGB(NUM2INT(rb_ary_entry(color, 0)), NUM2INT(rb_ary_entry(color, 1)), 
                            NUM2INT(rb_ary_entry(color, 2)), NUM2INT(rb_ary_entry(color, 3)));
    }
    return col;
}


/*--------------------------------------------------------------------
   ImageクラスのInitialize
 ---------------------------------------------------------------------*/
VALUE Image_initialize( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyImage *image;
    struct DXRubyTexture *texture;
    HRESULT hr;
    D3DSURFACE_DESC desc;
    VALUE vwidth, vheight, vary;
    int col = 0, width, height;

    g_iRefAll++;

    rb_scan_args( argc, argv, "21", &vwidth, &vheight, &vary );

    width = NUM2INT( vwidth );
    height = NUM2INT( vheight );

    if( width <= 0 || height <= 0 || width > 8192 || height > 8192)
    {
        rb_raise( eDXRubyError, "invalid size(must be between 1 to 8192) - Image_initialize" );
    }

    if( vary != Qnil )
    {
        Check_Type( vary, T_ARRAY );
        col = array2color( vary );
    }

    /* テクスチャメモリ取得 */
    texture = (struct  DXRubyTexture *)malloc( sizeof( struct DXRubyTexture ) );

    if( texture == NULL )
    {
        rb_raise( eDXRubyError, "malloc error - Image_initialize" );
    }

    DXRUBY_RETRY_START;
    /* テクスチャオブジェクトを作成する */
    hr = D3DXCreateTexture( g_pD3DDevice, width, height,
                                      1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                      &texture->pD3DTexture);
    DXRUBY_RETRY_END;
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "D3DXCreateTexture error - Image_initialize" );
    }

    texture->refcount = 1;

    texture->pD3DTexture->lpVtbl->GetLevelDesc(texture->pD3DTexture, 0, &desc );
    texture->width = (float)desc.Width;
    texture->height = (float)desc.Height;

    /* Imageオブジェクト設定 */
    image = DXRUBY_GET_STRUCT( Image, obj );
    if( image->texture )
    {
        Image_free( image );
        g_iRefAll--;
    }

    image->texture = texture;
    image->x = 0;
    image->y = 0;
    image->width = width;
    image->height = height;

    fill( 0, 0, width - 1, height - 1, col, image );

    return obj;
}


/*--------------------------------------------------------------------
   Imageクラスのload
 ---------------------------------------------------------------------*/
static VALUE Image_load( int argc, VALUE *argv, VALUE klass )
{
    struct DXRubyImage *image;
    struct DXRubyTexture *texture;
    D3DXIMAGE_INFO srcinfo;
    D3DSURFACE_DESC desc;
    HRESULT hr;
    VALUE vfilename, vx, vy, vwidth, vheight, obj, vsjisstr;
    int x, y, width, height;

    /* デバイスオブジェクトの初期化チェック */
    if( g_pD3DDevice == NULL )
    {
        rb_raise( eDXRubyError, "DirectX Graphics not initialized" );
    }

    rb_scan_args( argc, argv, "14", &vfilename, &vx, &vy, &vwidth, &vheight );

    Check_Type(vfilename, T_STRING);

    /* ファイル情報取得 */
    if( rb_enc_get_index( vfilename ) != 0 )
    {
        vsjisstr = rb_str_export_to_enc( vfilename, g_enc_sys );
    }
    else
    {
        vsjisstr = vfilename;
    }

    hr = D3DXGetImageInfoFromFile( RSTRING_PTR( vsjisstr ), &srcinfo );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Load error - %s", RSTRING_PTR( vsjisstr ) );
    }

    if( vx == Qnil )
    {
        x = 0;
        y = 0;
        width = srcinfo.Width;
        height = srcinfo.Height;
    }
    else
    {
        x = NUM2INT( vx );
        y = vy == Qnil ? 0 : NUM2INT( vy );
        if( x < 0 || x >= srcinfo.Width || y < 0 || y >= srcinfo.Height )
        {
            rb_raise( eDXRubyError, "Invalid the origin position(x=%d,y=%d, tex_width=%d,tex_height=%d) - Image_load", x, y, srcinfo.Width, srcinfo.Height );
        }
        width = vwidth == Qnil ? srcinfo.Width - x : NUM2INT( vwidth );
        height = vheight == Qnil ? srcinfo.Height - y : NUM2INT( vheight );
        if( srcinfo.Width - x < width || x + width > srcinfo.Width || srcinfo.Height - y < height || y + height > srcinfo.Height ||
            width < 0 || height < 0 )
        {
            rb_raise( eDXRubyError, "Invalid size - Image_load" );
        }
    }

    /* テクスチャロード */
    texture = Image_textureload( RSTRING_PTR( vsjisstr ), &srcinfo );
    texture->refcount = 1;

    texture->pD3DTexture->lpVtbl->GetLevelDesc(texture->pD3DTexture, 0, &desc );
    texture->width = (float)desc.Width;
    texture->height = (float)desc.Height;

    /* Imageオブジェクト設定 */
    obj = Image_allocate( cImage );
    image = DXRUBY_GET_STRUCT( Image, obj );

    image->texture = texture;
    image->x = x;
    image->y = y;
    image->width = width;
    image->height = height;
//    image->lockcount = 0;

    g_iRefAll++;

    return obj;
}


/*--------------------------------------------------------------------
   ImageクラスのloadFromFileInMemory
 ---------------------------------------------------------------------*/
static VALUE Image_loadFromFileInMemory( VALUE klass, VALUE vstr )
{
    struct DXRubyImage *image;
    struct DXRubyTexture *texture;
    D3DXIMAGE_INFO srcinfo;
    D3DSURFACE_DESC desc;
    HRESULT hr;
    VALUE obj;
    int size, x, y, width, height;

    /* デバイスオブジェクトの初期化チェック */
    if( g_pD3DDevice == NULL )
    {
        rb_raise( eDXRubyError, "DirectX Graphics not initialized" );
    }

    Check_Type(vstr, T_STRING);

    /* ファイル情報取得 */
    size = RSTRING_LEN( vstr );

    hr = D3DXGetImageInfoFromFileInMemory( RSTRING_PTR( vstr ), size, &srcinfo );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Load error - Image_loadFromFileInMemory" );
    }

    /* テクスチャメモリ取得 */
    texture = (struct  DXRubyTexture *)malloc( sizeof( struct DXRubyTexture ) );

    if( texture == NULL )
    {
        rb_raise( eDXRubyError, "Out of memory - Image_loadFromFileInMemory" );
    }

    DXRUBY_RETRY_START;
    /* ファイルを読み込んでテクスチャオブジェクトを作成する */
    hr = D3DXCreateTextureFromFileInMemoryEx( g_pD3DDevice, RSTRING_PTR( vstr ), size, srcinfo.Width, srcinfo.Height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                      D3DX_DEFAULT,  D3DX_DEFAULT, 0,
                                      0, 0, &texture->pD3DTexture);
    DXRUBY_RETRY_END;
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Load error - Image_loadFromFileInMemory" );
    }

    texture->refcount = 1;

    texture->pD3DTexture->lpVtbl->GetLevelDesc(texture->pD3DTexture, 0, &desc );
    texture->width = (float)desc.Width;
    texture->height = (float)desc.Height;

    /* Imageオブジェクト設定 */
    obj = Image_allocate( cImage );
    image = DXRUBY_GET_STRUCT( Image, obj );

    image->texture = texture;
    image->x = 0;
    image->y = 0;
    image->width = srcinfo.Width;
    image->height = srcinfo.Height;
//    image->lockcount = 0;

    g_iRefAll++;

    return obj;
}


/*--------------------------------------------------------------------
   Imageオブジェクトの分割作成
 ---------------------------------------------------------------------*/
static VALUE Image_loadToArray( int argc, VALUE *argv, VALUE klass )
{
//    VALUE vfilename, vx, vy, vswitch;
    VALUE vimage, ary[3];

    if( argc < 3 || argc > 4 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 3, 4 );
//    rb_scan_args( argc, argv, "31", &vfilename, &vx, &vy, &vswitch );

    vimage = Image_load( 1, &argv[0], cImage );
    ary[0] = argv[1];
    ary[1] = argv[2];
    ary[2] = argc < 4 ? Qtrue : argv[3];
    return Image_sliceToArray( 3, ary, vimage );
}


/*--------------------------------------------------------------------
   配列からイメージを作る
 ---------------------------------------------------------------------*/
static VALUE Image_createFromArray( VALUE klass, VALUE vwidth, VALUE vheight, VALUE array )
{
    struct DXRubyImage *image;
    struct DXRubyTexture *texture;
    HRESULT hr;
    int i, j, x, y;
    D3DLOCKED_RECT LockedRect;
    VALUE obj;
    int width, height;
    D3DSURFACE_DESC desc;

    /* デバイスオブジェクトの初期化チェック */
    if( g_pD3DDevice == NULL )
    {
        rb_raise( eDXRubyError, "DirectX Graphics not initialized" );
    }

    width = NUM2INT( vwidth );
    height = NUM2INT( vheight );
    Check_Type(array, T_ARRAY);

    if( width <= 0 || height <= 0 ) rb_raise( eDXRubyError, "Invalid size(width=%d,height=%d) - Image_loadToArray", width, height );

    /* テクスチャメモリ取得 */
    texture = (struct  DXRubyTexture *)malloc( sizeof( struct DXRubyTexture ) );
    if( texture == NULL )
    {
        rb_raise( eDXRubyError, "Out of memory - Image_textureload" );
    }

    /* テクスチャサイズ割り出し */
    for( x = 1; x < width;  x = x * 2 );
    for( y = 1; y < height; y = y * 2 );

    DXRUBY_RETRY_START;
    /* テクスチャオブジェクトを作成する */
    hr = D3DXCreateTexture( g_pD3DDevice, x, y,
                            1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                            &texture->pD3DTexture);
    DXRUBY_RETRY_END;
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Create texture error - Image_initialize" );
    }

    /* テクスチャロック */
    hr = texture->pD3DTexture->lpVtbl->LockRect( texture->pD3DTexture, 0, &LockedRect, NULL, 0 );
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Surface lock error - LockRect" );
    }

    /* 書き込み */
    for( i = 0; i < height; i++ )
    {
        for( j = 0; j < width * 4; j += 4 )
        {
            int a1 = NUM2INT(rb_ary_entry(array, j + i * width * 4));
            int a2 = NUM2INT(rb_ary_entry(array, j + i * width * 4 + 1));
            int a3 = NUM2INT(rb_ary_entry(array, j + i * width * 4 + 2));
            int a4 = NUM2INT(rb_ary_entry(array, j + i * width * 4 + 3));
            *((int*)((char *)LockedRect.pBits + j + i * LockedRect.Pitch)) = D3DCOLOR_ARGB(a1, a2, a3, a4);
        }
    }

    /* テクスチャアンロック */
    texture->pD3DTexture->lpVtbl->UnlockRect( texture->pD3DTexture, 0 );

    texture->refcount = 1;
    texture->pD3DTexture->lpVtbl->GetLevelDesc( texture->pD3DTexture, 0, &desc );
    texture->width = (float)desc.Width;
    texture->height = (float)desc.Height;

    /* DXRubyImageのメモリ取得＆Imageオブジェクト生成 */
    obj = Image_allocate( cImage );
    image = DXRUBY_GET_STRUCT( Image, obj );

    image->texture = texture;
    image->x = 0;
    image->y = 0;
    image->width = width;
    image->height = height;
//    image->lockcount = 0;

    g_iRefAll++;

    return obj;
}


/*--------------------------------------------------------------------
   Imageオブジェクトのslice
 ---------------------------------------------------------------------*/
static VALUE Image_slice_instance( int argc, VALUE *argv, VALUE vsrcimage )
{
    struct DXRubyImage *srcimage;
    struct DXRubyImage *image;
    struct DXRubyTexture *texture;
    D3DXIMAGE_INFO srcinfo;
    HRESULT hr;
    VALUE vx, vy, vwidth, vheight, obj;
    int x, y, width, height;

    rb_scan_args( argc, argv, "04", &vx, &vy, &vwidth, &vheight );

    srcimage = DXRUBY_GET_STRUCT( Image, vsrcimage );
    DXRUBY_CHECK_DISPOSE( srcimage, texture );

    if( vx == Qnil )
    {
        x = 0;
        y = 0;
        width = srcimage->width;
        height = srcimage->height;
    }
    else
    {
        x = NUM2INT( vx );
        y = vy == Qnil ? 0 : NUM2INT( vy );
        if( x < 0 || x >= srcimage->width || y < 0 || y >= srcimage->height )
        {
            rb_raise( eDXRubyError, "Invalid the origin position(x=%d,y=%d, tex_width=%d,tex_height=%d) - Image_slice", x, y, srcimage->width, srcimage->height );
        }
        width = vwidth == Qnil ? srcimage->width - x : NUM2INT( vwidth );
        height = vheight == Qnil ? srcimage->height - y : NUM2INT( vheight );
        if( srcimage->width - x < width || x + width > srcimage->width || srcimage->height - y < height || y + height > srcimage->height ||
            width < 0 || height < 0 )
        {
            rb_raise( eDXRubyError, "Invalid size - Image_slice" );
        }
    }

    /* テクスチャロード */
    texture = srcimage->texture;
    texture->refcount += 1;

    /* Imageオブジェクト設定 */
    obj = Image_allocate( cImage );
    image = DXRUBY_GET_STRUCT( Image, obj );

    image->texture = texture;
    image->x = x + srcimage->x;
    image->y = y + srcimage->y;
    image->width = width;
    image->height = height;
//    image->lockcount = 0;

    g_iRefAll++;

    /* Imageのコピー */
    return Image_initialize_copy( Image_allocate( cImage ), obj );
}


/*--------------------------------------------------------------------
   Imageオブジェクトのflush
 ---------------------------------------------------------------------*/
static VALUE Image_flush( VALUE self, VALUE vcolor )
{
    struct DXRubyImage *dstimage = DXRUBY_GET_STRUCT( Image, self );
    struct DXRubyImage *image;
    VALUE obj;
    D3DLOCKED_RECT texrect;
    RECT rect;
    int y;
    int color;

    DXRUBY_CHECK_DISPOSE( dstimage, texture );
    Check_Type(vcolor, T_ARRAY);

    /* 元Imageのコピー */
    obj = Image_initialize_copy( Image_allocate( cImage ), self );

    /* 新Imageのポインタ取得 */
    image = DXRUBY_GET_STRUCT( Image, obj );

    /* 画像編集 */
    rect.left = 0;
    rect.top = 0;
    rect.right = image->width;
    rect.bottom = image->height;
    image->texture->pD3DTexture->lpVtbl->LockRect( image->texture->pD3DTexture, 0, &texrect, &rect, 0 );

    if( RARRAY_LEN( vcolor ) == 3 )
    {
        color = D3DCOLOR_ARGB(0, NUM2INT(rb_ary_entry(vcolor, 0)), NUM2INT(rb_ary_entry(vcolor, 1)), NUM2INT(rb_ary_entry(vcolor, 2)));
        for( y = 0; y < image->height; y++ )
        {
            int *p = (int*)((char *)texrect.pBits + y * texrect.Pitch);
            int x;

            for( x = 0; x < image->width; x++ )
            {
                if( (*p & 0xff000000) != 0 )
                {
                    *p = color | (*p & 0xff000000);
                }
                p++;
            }
        }
    }
    else
    {
        color = D3DCOLOR_ARGB(NUM2INT(rb_ary_entry(vcolor, 0)), NUM2INT(rb_ary_entry(vcolor, 1)), 
                              NUM2INT(rb_ary_entry(vcolor, 2)), NUM2INT(rb_ary_entry(vcolor, 3)));
        for( y = 0; y < image->height; y++ )
        {
            int *p = (int*)((char *)texrect.pBits + y * texrect.Pitch);
            int x;

            for( x = 0; x < image->width; x++ )
            {
                if( (*p & 0xff000000) != 0 )
                {
                    *p = color;
                }
                p++;
            }
        }
    }

    image->texture->pD3DTexture->lpVtbl->UnlockRect( image->texture->pD3DTexture, 0 );

    return obj;
}


/*--------------------------------------------------------------------
   ImgaeオブジェクトのsliceToArray
 ---------------------------------------------------------------------*/
VALUE Image_sliceToArray( int argc, VALUE *argv, VALUE self )
{
    struct DXRubyImage *srcimage;
    struct DXRubyTexture *texture;
    HRESULT hr;
    int x, y, i, j;
    VALUE array, obj;
//    VALUE vx, vy, vswitch;

    if( argc < 2 || argc > 3 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 2, 3 );
//    rb_scan_args( argc, argv, "21", &vx, &vy, &vswitch );

    x = NUM2INT( argv[0] );
    y = NUM2INT( argv[1] );

    if( x <= 0 || y <= 0 ) rb_raise( eDXRubyError, "Invalid count(x=%d,y=%d) - Image_sliceToArray", x, y );

    /* 元Imageのチェック */
    srcimage = DXRUBY_GET_STRUCT( Image, self );
    DXRUBY_CHECK_DISPOSE( srcimage, texture );

    /* 元Imageのコピー */
    obj = Image_initialize_copy( Image_allocate( cImage ), self );

    /* コピーしたImageからslice */
    srcimage = DXRUBY_GET_STRUCT( Image, obj );
    DXRUBY_CHECK_DISPOSE( srcimage, texture );

    texture = srcimage->texture;
    texture->refcount += x * y;

    /* Ruby配列作成 */
    array = rb_ary_new();

    if( argc < 3 || RTEST(argv[2]) )
    {
        for( i = 0; i < y; i++ )
        {
            for( j = 0; j < x; j++ )
            {
                struct DXRubyImage *image;
                /* DXRubyImageのメモリ取得＆Imageオブジェクト生成 */
                obj = Image_allocate( cImage );
                image = DXRUBY_GET_STRUCT( Image, obj );

                image->texture = texture;
                image->x = srcimage->x + j * (srcimage->width / x);
                image->y = srcimage->y + i * (srcimage->height / y);
                image->width = srcimage->width / x;
                image->height = srcimage->height / y;
//                image->lockcount = 0;
                rb_ary_push( array, obj );
                g_iRefAll++;
            }
        }
    }
    else
    {
        for( i = 0; i < y; i++ )
        {
            for( j = 0; j < x; j++ )
            {
                VALUE ary[4] = {INT2FIX(srcimage->x + j * (srcimage->width / x)),
                                INT2FIX(srcimage->y + i * (srcimage->height / y)),
                                INT2FIX(srcimage->width / x),
                                INT2FIX(srcimage->height / y)};
                rb_ary_push( array, Image_slice_instance( 4, ary, obj ) );
            }
        }
    }
    return array;
}


/*--------------------------------------------------------------------
   イメージのデータ取得
 ---------------------------------------------------------------------*/
static VALUE Image_getPixel( VALUE obj, VALUE vx, VALUE vy )
{
    struct DXRubyImage *image;
    VALUE ary[4];
    struct DXRubyColor a;
    D3DLOCKED_RECT texrect;
    int x, y;
    RECT rect;

    x = NUM2INT( vx );
    y = NUM2INT( vy );

    image = DXRUBY_GET_STRUCT( Image, obj );
    DXRUBY_CHECK_DISPOSE( image, texture );

    if( x < 0 || x >= image->width || y < 0 || y >= image->height )
    {
        ary[0] = ary[1] = ary[2] = ary[3] = INT2FIX( 0 );
        return rb_ary_new4( 4, ary );
    }

    rect.left = x + image->x;
    rect.top = y + image->y;
    rect.right = x + image->x + 1;
    rect.bottom = y + image->y + 1;
    image->texture->pD3DTexture->lpVtbl->LockRect( image->texture->pD3DTexture, 0, &texrect, &rect, D3DLOCK_READONLY );

    a = *(struct DXRubyColor *)texrect.pBits;

    image->texture->pD3DTexture->lpVtbl->UnlockRect( image->texture->pD3DTexture, 0 );

    ary[0] = INT2FIX( a.alpha );
    ary[1] = INT2FIX( a.red );
    ary[2] = INT2FIX( a.green );
    ary[3] = INT2FIX( a.blue );

    return rb_ary_new4( 4, ary );
}


/*--------------------------------------------------------------------
   イメージのデータ設定
 ---------------------------------------------------------------------*/
static VALUE Image_setPixel( VALUE obj, VALUE vx, VALUE vy, VALUE color )
{
    struct DXRubyImage *image;
    int a1, a2, a3, a4;
    D3DLOCKED_RECT texrect;
    int x, y;
    RECT rect;

    x = NUM2INT( vx );
    y = NUM2INT( vy );
    Check_Type(color, T_ARRAY);

    image = DXRUBY_GET_STRUCT( Image, obj );
    DXRUBY_CHECK_DISPOSE( image, texture );
//    DXRUBY_CHECK_IMAGE_LOCK( image );

    if( x < 0 || x >= image->width || y < 0 || y >= image->height )
    {
        return obj;
    }

    rect.left = x + image->x;
    rect.top = y + image->y;
    rect.right = x + image->x + 1;
    rect.bottom = y + image->y + 1;
    image->texture->pD3DTexture->lpVtbl->LockRect( image->texture->pD3DTexture, 0, &texrect, &rect, 0 );

    *((int*)((char *)texrect.pBits)) = array2color( color );

    image->texture->pD3DTexture->lpVtbl->UnlockRect( image->texture->pD3DTexture, 0 );

    return color;
}


/*--------------------------------------------------------------------
   イメージの色比較
 ---------------------------------------------------------------------*/
static VALUE Image_compare( VALUE obj, VALUE vx, VALUE vy, VALUE color )
{
    struct DXRubyImage *image;
    DWORD a;
    D3DLOCKED_RECT texrect;
    int x, y, col;
    int a1, a2, a3, a4;
    RECT rect;

    x = NUM2INT( vx );
    y = NUM2INT( vy );
    Check_Type(color, T_ARRAY);

    image = DXRUBY_GET_STRUCT( Image, obj );
    DXRUBY_CHECK_DISPOSE( image, texture );

    if( x < 0 || x >= image->width || y < 0 || y >= image->height )
    {
        return INT2FIX( 0 );
    }

    col = array2color( color );

    rect.left = x + image->x;
    rect.top = y + image->y;
    rect.right = x + image->x + 1;
    rect.bottom = y + image->y + 1;
    image->texture->pD3DTexture->lpVtbl->LockRect( image->texture->pD3DTexture, 0, &texrect, &rect, D3DLOCK_READONLY );

    a = *(LPDWORD)texrect.pBits;

    image->texture->pD3DTexture->lpVtbl->UnlockRect( image->texture->pD3DTexture, 0 );

    if( RARRAY_LEN( color ) == 3 )
    {
        
        if( (a & 0x00ffffff) == ((DWORD)col & 0x00ffffff) )
        {
            return Qtrue;
        }
        else
        {
            return Qfalse;
        }
    }
    else
    {
        if( a == (DWORD)col )
        {
            return Qtrue;
        }
        else
        {
            return Qfalse;
        }
    }
}


/*--------------------------------------------------------------------
    イメージのデータ設定(box描画塗りつぶさない)
 ---------------------------------------------------------------------*/
static VALUE Image_box( VALUE obj, VALUE vx1, VALUE vy1, VALUE vx2, VALUE vy2, VALUE color )
{
    struct DXRubyImage *image;
    D3DLOCKED_RECT texrect;
    int x, y, x1, y1, x2, y2;
    int col;
    RECT rect;

    image = DXRUBY_GET_STRUCT( Image, obj );
    DXRUBY_CHECK_DISPOSE( image, texture );
//    DXRUBY_CHECK_IMAGE_LOCK( image );

    x1 = NUM2INT( vx1 );
    y1 = NUM2INT( vy1 );
    x2 = NUM2INT( vx2 );
    y2 = NUM2INT( vy2 );

    /* 左上から右下の指定に修正 */
    if( x1 > x2 )
    {
        x = x2;
        x2 = x1;
        x1 = x;
    }
    if( y1 > y2 )
    {
        y = y2;
        y2 = y1;
        y1 = y;
    }

    /* 範囲外の指定は無視 */
    if( x1 > image->width - 1 || x2 < 0 || y1 > image->height - 1 || y2 < 0)
    {
        return obj;
    }

    /* クリップ */
    if( x1 < 0 )
    {
        x1 = 0;
    }
    if( x2 > image->width - 1 )
    {
        x2 = image->width - 1;
    }
    if( y1 < 0 )
    {
        y1 = 0;
    }
    if( y2 > image->height - 1 )
    {
        y2 = image->height - 1;
    }

    Check_Type(color, T_ARRAY);
    col = array2color( color );

    rect.left = x1 + image->x;
    rect.top = y1 + image->y;
    rect.right = x2 + image->x + 1;
    rect.bottom = y2 + image->y + 1;
    image->texture->pD3DTexture->lpVtbl->LockRect( image->texture->pD3DTexture, 0, &texrect, &rect, 0 );
    for( y = 0; y <= y2 - y1; y++ )
    {
        *((int*)((char *)texrect.pBits + y * texrect.Pitch)) = col;
        *((int*)((char *)texrect.pBits + (x2 - x1)* 4 + y * texrect.Pitch)) = col;
    }
    for( x = 0; x <= x2 - x1; x++ )
    {
        *((int*)((char *)texrect.pBits + x * 4)) = col;
        *((int*)((char *)texrect.pBits + x * 4 + (y2 - y1) * texrect.Pitch)) = col;
    }
    image->texture->pD3DTexture->lpVtbl->UnlockRect( image->texture->pD3DTexture, 0 );

    return obj;
}


/*--------------------------------------------------------------------
    イメージのデータ設定(box描画塗りつぶす)
 ---------------------------------------------------------------------*/
static VALUE Image_boxFill( VALUE obj, VALUE vx1, VALUE vy1, VALUE vx2, VALUE vy2, VALUE color )
{
    struct DXRubyImage *image;
    int x, y, x1, y1, x2, y2;
    int col;

    image = DXRUBY_GET_STRUCT( Image, obj );
    DXRUBY_CHECK_DISPOSE( image, texture );
//    DXRUBY_CHECK_IMAGE_LOCK( image );

    x1 = NUM2INT( vx1 );
    y1 = NUM2INT( vy1 );
    x2 = NUM2INT( vx2 );
    y2 = NUM2INT( vy2 );

    /* 左上から右下の指定に修正 */
    if( x1 > x2 )
    {
        x = x2;
        x2 = x1;
        x1 = x;
    }
    if( y1 > y2 )
    {
        y = y2;
        y2 = y1;
        y1 = y;
    }

    /* 範囲外の指定は無視 */
    if( x1 > image->width - 1 || x2 < 0 || y1 > image->height - 1 || y2 < 0)
    {
        return obj;
    }

    /* クリップ */
    if( x1 < 0 )
    {
        x1 = 0;
    }
    if( x2 > image->width - 1 )
    {
        x2 = image->width - 1;
    }
    if( y1 < 0 )
    {
        y1 = 0;
    }
    if( y2 > image->height - 1 )
    {
        y2 = image->height - 1;
    }

    Check_Type(color, T_ARRAY);
    col = array2color( color );

    fill( x1, y1, x2, y2, col, image );

    return obj;
}


/*--------------------------------------------------------------------
    イメージのデータ設定(全部透明色で塗りつぶす)
 ---------------------------------------------------------------------*/
static VALUE Image_clear( VALUE obj )
{
    struct DXRubyImage *image;

    image = DXRUBY_GET_STRUCT( Image, obj );
    DXRUBY_CHECK_DISPOSE( image, texture );
//    DXRUBY_CHECK_IMAGE_LOCK( image );

    fill( 0, 0, image->width-1, image->height-1, 0, image );

    return obj;
}


/*--------------------------------------------------------------------
    イメージのデータ設定(全部塗りつぶす)
 ---------------------------------------------------------------------*/
static VALUE Image_fill( VALUE obj, VALUE color )
{
    struct DXRubyImage *image;
    int col;

    image = DXRUBY_GET_STRUCT( Image, obj );
    DXRUBY_CHECK_DISPOSE( image, texture );
//    DXRUBY_CHECK_IMAGE_LOCK( image );

    Check_Type(color, T_ARRAY);
    col = array2color( color );

    fill( 0, 0, image->width-1, image->height-1, col, image );

    return obj;
}


/*--------------------------------------------------------------------
    イメージのカラーキー設定。実体は指定色を透明に。
 ---------------------------------------------------------------------*/
static VALUE Image_setColorKey( VALUE obj, VALUE color )
{
    struct DXRubyImage *image;
    int col;
    D3DLOCKED_RECT texrect;
    int x, y;
    RECT rect;
    int *p;

    image = DXRUBY_GET_STRUCT( Image, obj );
    DXRUBY_CHECK_DISPOSE( image, texture );
//    DXRUBY_CHECK_IMAGE_LOCK( image );

    Check_Type(color, T_ARRAY);
    col = array2color( color ) & 0x00ffffff;

    rect.left = image->x;
    rect.top = image->y;
    rect.right = image->x + image->width;
    rect.bottom = image->y + image->height;
    image->texture->pD3DTexture->lpVtbl->LockRect( image->texture->pD3DTexture, 0, &texrect, &rect, 0 );
    for( y = 0; y < image->height; y++ )
    {
        p = (int*)((char *)texrect.pBits + y * texrect.Pitch);
        for( x = 0; x < image->width; x++ )
        {
            if( (*p & 0x00ffffff) == col )
            {
                *p = col;
            }
            p++;
        }
    }
    image->texture->pD3DTexture->lpVtbl->UnlockRect( image->texture->pD3DTexture, 0 );

    return obj;
}


/*--------------------------------------------------------------------
    イメージのデータ設定(circle用のline描画)
 ---------------------------------------------------------------------*/
static void Image_circle_line( int x1, int x2, int y, RECT *rect, int col, D3DLOCKED_RECT *texrect )
{
    int x;
    int *p;

    if( y < 0 || y >= rect->bottom - rect->top )
    {
        return;
    }

    /* クリップ */
    if( x1 < 0 )
    {
        x1 = 0;
    }
    if( x2 > rect->right - rect->left - 1 )
    {
        x2 = rect->right - rect->left - 1;
    }
    if( x1 > x2 )
    {
        return;
    }

    p = (int*)((char *)texrect->pBits + y * texrect->Pitch + x1 * 4);
    for( x = 0; x <= x2 - x1; x++ )
    {
        *(p++) = col;
    }
}


/*--------------------------------------------------------------------
    イメージのデータ設定(circle描画塗りつぶす)
 ---------------------------------------------------------------------*/
static VALUE Image_circleFill( VALUE obj, VALUE vx0, VALUE vy0, VALUE vr, VALUE color )
{
    struct DXRubyImage *image;
    int x0, y0, F, x, y;
    float r;
    int col;
    D3DLOCKED_RECT texrect;
    RECT rect;
    HRESULT hr;
    int tempx, tempy;

    image = DXRUBY_GET_STRUCT( Image, obj );
    DXRUBY_CHECK_DISPOSE( image, texture );
//    DXRUBY_CHECK_IMAGE_LOCK( image );

    x0 = NUM2INT( vx0 );
    y0 = NUM2INT( vy0 );
    r  = NUM2FLOAT( vr );

    Check_Type(color, T_ARRAY);
    col = array2color( color );

    rect.left = (LONG)(x0 < r ? image->x : x0 + image->x - r);
    rect.top = (LONG)(y0 < r ? image->y : y0 + image->y - r);
    rect.right = (LONG)(x0 + r >= image->width ? image->x + image->width : x0 + image->x + r + 1);
    rect.bottom = (LONG)(y0 + r >= image->height ? image->y + image->height : y0 + image->y + r + 1);
    if( rect.left >= rect.right || rect.top >= rect.bottom )
    {
        return obj;
    }

    hr = image->texture->pD3DTexture->lpVtbl->LockRect( image->texture->pD3DTexture, 0, &texrect, &rect, 0 );

    {//http://dencha.ojaru.jp/programs_07/pg_graphic_09a2.html
        int tempx = -rect.left + (int)image->x;
        int tempy = -rect.top + (int)image->y;
        int xm = x0, ym = y0;
        int diameter = (int)(r*2);

        LONG    cx = 0, cy=diameter/2+1;
        double d;
        int dx, dy;
        d = -diameter*diameter + 4*cy*cy -4*cy +2;
        dx = 4;
        dy = -8*cy+8;
        if( (diameter & 1) == 0 )
        {
            xm--;
            ym--;
        }

        for( cx = 0; cx <= cy; cx++ )
        {
            if( d > 0 )
            {
                d += dy;
                dy += 8;
                cy--;
            }
            Image_circle_line( tempx - cx + x0, tempx + cx + xm, tempy - cy + y0, &rect, col, &texrect );
            Image_circle_line( tempx - cy + x0, tempx + cy + xm, tempy - cx + y0, &rect, col, &texrect );
            Image_circle_line( tempx - cy + x0, tempx + cy + xm, tempy + cx + ym, &rect, col, &texrect );
            Image_circle_line( tempx - cx + x0, tempx + cx + xm, tempy + cy + ym, &rect, col, &texrect );

            d += dx;
            dx+=8;
        }
    }

    image->texture->pD3DTexture->lpVtbl->UnlockRect( image->texture->pD3DTexture, 0 );

    return obj;
}


/*--------------------------------------------------------------------
    イメージのデータ設定(内部処理用pixel描画)
 ---------------------------------------------------------------------*/
static void Image_circle_pixel( int x, int y, RECT *rect, int col, D3DLOCKED_RECT *texrect )
{
    if( x < 0 || x >= rect->right - rect->left || y < 0 || y >= rect->bottom - rect->top )
    {
        return;
    }

    *((int*)((char *)texrect->pBits + x * 4 + y * texrect->Pitch)) = col;

    return;
}


/*--------------------------------------------------------------------
    イメージのデータ設定(circle描画塗りつぶさない)
 ---------------------------------------------------------------------*/
static VALUE Image_circle( VALUE obj, VALUE vx0, VALUE vy0, VALUE vr, VALUE color )
{
    struct DXRubyImage *image;
    D3DLOCKED_RECT texrect;
    int x0, y0, F, x, y;
    float r;
    int col;
    RECT rect;

    image = DXRUBY_GET_STRUCT( Image, obj );
    DXRUBY_CHECK_DISPOSE( image, texture );
//    DXRUBY_CHECK_IMAGE_LOCK( image );

    x0 = NUM2INT( vx0 );
    y0 = NUM2INT( vy0 );
    r  = NUM2FLOAT( vr );

    Check_Type(color, T_ARRAY);
    col = array2color( color );

    rect.left = (LONG)(x0 < r ? image->x : x0 + image->x - r);
    rect.top = (LONG)(y0 < r ? image->y : y0 + image->y - r);
    rect.right = (LONG)(x0 + r >= image->width ? image->x + image->width : x0 + image->x + r + 1);
    rect.bottom = (LONG)(y0 + r >= image->height ? image->y + image->height : y0 + image->y + r + 1);
    if( rect.left >= rect.right || rect.top >= rect.bottom )
    {
        return obj;
    }

    image->texture->pD3DTexture->lpVtbl->LockRect( image->texture->pD3DTexture, 0, &texrect, &rect, 0 );

    {//http://dencha.ojaru.jp/programs_07/pg_graphic_09a2.html
        int tempx = -rect.left + (int)image->x;
        int tempy = -rect.top + (int)image->y;
        int xm = x0, ym = y0;
        int diameter = (int)(r*2);

        LONG    cx = 0, cy=diameter/2+1;
        double d;
        int dx, dy;
        d = -diameter*diameter + 4*cy*cy -4*cy +2;
        dx = 4;
        dy = -8*cy+8;
        if( (diameter & 1) == 0 )
        {
            xm--;
            ym--;
        }

        for( cx = 0; cx <= cy; cx++ )
        {
            if( d > 0 )
            {
                d += dy;
                dy += 8;
                cy--;
            }
            Image_circle_pixel( tempx - cy + x0, tempy - cx + y0, &rect, col, &texrect );
            Image_circle_pixel( tempx - cx + x0, tempy - cy + y0, &rect, col, &texrect );

            Image_circle_pixel( tempx + cx + xm, tempy - cy + y0, &rect, col, &texrect );
            Image_circle_pixel( tempx + cy + xm, tempy - cx + y0, &rect, col, &texrect );

            Image_circle_pixel( tempx + cy + xm, tempy + cx + ym, &rect, col, &texrect );
            Image_circle_pixel( tempx + cx + xm, tempy + cy + ym, &rect, col, &texrect );

            Image_circle_pixel( tempx - cx + x0, tempy + cy + ym, &rect, col, &texrect );
            Image_circle_pixel( tempx - cy + x0, tempy + cx + ym, &rect, col, &texrect );

            d += dx;
            dx+=8;
        }
    }

    image->texture->pD3DTexture->lpVtbl->UnlockRect( image->texture->pD3DTexture, 0 );

    return obj;
}


/*--------------------------------------------------------------------
    イメージのデータ設定(line描画)
 ---------------------------------------------------------------------*/
static VALUE Image_line( VALUE obj, VALUE vx1, VALUE vy1, VALUE vx2, VALUE vy2, VALUE color )
{
    struct DXRubyImage *image;
    D3DLOCKED_RECT texrect;
    int x1, y1, x2, y2, xp, yp;
    int col;
    int c, d, dx, dy, i;
    RECT rect;

    image = DXRUBY_GET_STRUCT( Image, obj );
    DXRUBY_CHECK_DISPOSE( image, texture );
//    DXRUBY_CHECK_IMAGE_LOCK( image );

    x1 = NUM2INT( vx1 );
    y1 = NUM2INT( vy1 );
    x2 = NUM2INT( vx2 );
    y2 = NUM2INT( vy2 );

    Check_Type(color, T_ARRAY);
    col = array2color( color );

    rect.left = (x1 < x2 ? x1 : x2) < image->x ? image->x : (x1 < x2 ? x1 : x2);
    rect.top = (y1 < y2 ? y1 : y2) < image->y ? image->y : (y1 < y2 ? y1 : y2);
    rect.right = (x1 > x2 ? x1 : x2) >= image->width ? image->x + image->width : image->x + (x1 > x2 ? x1 : x2) + 1;
    rect.bottom = (y1 > y2 ? y1 : y2) >= image->height ? image->y + image->height : image->y + (y1 > y2 ? y1 : y2) + 1;
    if( rect.left >= rect.right || rect.top >= rect.bottom )
    {
        return obj;
    }

    x1 = x1 - rect.left + image->x;
    x2 = x2 - rect.left + image->x;
    y1 = y1 - rect.top + image->y;
    y2 = y2 - rect.top + image->y;

    image->texture->pD3DTexture->lpVtbl->LockRect( image->texture->pD3DTexture, 0, &texrect, &rect, 0 );

    dx = x2 > x1 ? x2 - x1 : x1 - x2;
    dy = y2 > y1 ? y2 - y1 : y1 - y2;

    /* ブレゼンハムアルゴリズムによる線分描画 */
    if( dx < dy )
    {
        xp = x1 < x2 ? 1 : -1;
        d = y1 < y2 ? 1 : -1;
        c = dy;
        for( i = 0; i <= dy; i++ )
        {
            if( x1 >= 0 && x1 < (int)rect.right - rect.left  && y1 >= 0 && y1 < (int)rect.bottom - rect.top )
            {
                *((int*)((char *)texrect.pBits + x1 * 4 + y1  * texrect.Pitch)) = col;
            }
            y1 = y1 + d;
            c = c + dx*2;
            if( c >= dy*2 )
            {
                c = c - dy*2;
                x1 = x1 + xp;
            }
        }
    }
    else
    {
        yp = y1 < y2 ? 1 : -1;
        d = x1 < x2 ? 1 : -1;
        c = dx;
        for( i = 0; i <= dx; i++ )
        {
            if( x1 >= 0 && x1 < (int)rect.right - rect.left  && y1 >= 0 && y1 < (int)rect.bottom - rect.top )
            {
                *((int*)((char *)texrect.pBits + x1 * 4 + y1  * texrect.Pitch)) = col;
            }
            x1 = x1 + d;
            c = c + dy*2;
            if( c >= dx*2 )
            {
                c = c - dx*2;
                y1 = y1 + yp;
            }
        }
    }

    image->texture->pD3DTexture->lpVtbl->UnlockRect( image->texture->pD3DTexture, 0 );

    return obj;
}


/*--------------------------------------------------------------------
    イメージ三角形描画
 ---------------------------------------------------------------------*/
static VALUE Image_triangle( VALUE obj, VALUE vx1, VALUE vy1, VALUE vx2, VALUE vy2, VALUE vx3, VALUE vy3, VALUE color )
{
    Image_line( obj, vx1, vy1, vx2, vy2, color );
    Image_line( obj, vx2, vy2, vx3, vy3, color );
    Image_line( obj, vx3, vy3, vx1, vy1, color );

    return obj;
}


static void Image_triangle_line( int x1, int y1, int x2, int y2, int *buf_x_min, int *buf_x_max, RECT *rect )
{
    int dx, dy, c, d, xp, yp, i;

    dx = x2 > x1 ? x2 - x1 : x1 - x2;
    dy = y2 > y1 ? y2 - y1 : y1 - y2;

    /* ブレゼンハムアルゴリズムによる線分描画 */
    if( dx < dy )
    {
        xp = x1 < x2 ? 1 : -1;
        d = y1 < y2 ? 1 : -1;
        c = dy;
        for( i = 0; i <= dy; i++ )
        {
            if( y1 >= 0 && y1 < (int)rect->bottom - rect->top )
            {
                if( x1 < buf_x_min[y1] )
                {
                    buf_x_min[y1] = x1;
                }
                if( x1 > buf_x_max[y1] )
                {
                    buf_x_max[y1] = x1;
                }
            }
            y1 = y1 + d;
            c = c + dx*2;
            if( c >= dy*2 )
            {
                c = c - dy*2;
                x1 = x1 + xp;
            }
        }
    }
    else
    {
        yp = y1 < y2 ? 1 : -1;
        d = x1 < x2 ? 1 : -1;
        c = dx;
        for( i = 0; i <= dx; i++ )
        {
            if( y1 >= 0 && y1 < (int)rect->bottom - rect->top )
            {
                if( x1 < buf_x_min[y1] )
                {
                    buf_x_min[y1] = x1;
                }
                if( x1 > buf_x_max[y1] )
                {
                    buf_x_max[y1] = x1;
                }
            }
            x1 = x1 + d;
            c = c + dy*2;
            if( c >= dx*2 )
            {
                c = c - dx*2;
                y1 = y1 + yp;
            }
        }
    }
}

/*--------------------------------------------------------------------
    イメージ三角形描画(塗りつぶす)
 ---------------------------------------------------------------------*/
static VALUE Image_triangle_fill( VALUE obj, VALUE vx1, VALUE vy1, VALUE vx2, VALUE vy2, VALUE vx3, VALUE vy3, VALUE color )
{
    struct DXRubyImage *image;
    D3DLOCKED_RECT texrect;
    int x[3], y[3]; /* 頂点 */
    int col;
    RECT rect;
    int i;
    int xv1, yv1, xv2, yv2; /* 境界ボリューム */

    image = DXRUBY_GET_STRUCT( Image, obj );
    DXRUBY_CHECK_DISPOSE( image, texture );
//    DXRUBY_CHECK_IMAGE_LOCK( image );

    x[0] = NUM2INT( vx1 );
    y[0] = NUM2INT( vy1 );
    x[1] = NUM2INT( vx2 );
    y[1] = NUM2INT( vy2 );
    x[2] = NUM2INT( vx3 );
    y[2] = NUM2INT( vy3 );

    Check_Type(color, T_ARRAY);
    col = array2color( color );

    /* テクスチャロック用境界ボリューム作成 */
    xv1 = xv2 = x[2];
    yv1 = yv2 = y[2];
    for( i = 0; i < 2; i++ )
    {
        if( xv1 > x[i] )
        {
            xv1 = x[i];
        }
        if( xv2 < x[i] )
        {
            xv2 = x[i];
        }
        if( yv1 > y[i] )
        {
            yv1 = y[i];
        }
        if( yv2 < y[i] )
        {
            yv2 = y[i];
        }
    }

    rect.left = xv1 < image->x ? image->x : xv1;
    rect.top = yv1 < image->y ? image->y : yv1;
    rect.right = xv2 >= image->width ? image->x + image->width : xv2 + 1;
    rect.bottom = yv2 >= image->height ? image->y + image->height : image->y + yv2 + 1;
    if( rect.left >= rect.right || rect.top >= rect.bottom )
    {
        return obj;
    }

    image->texture->pD3DTexture->lpVtbl->LockRect( image->texture->pD3DTexture, 0, &texrect, &rect, 0 );

    {
        int *buf_x_min, *buf_x_max;
        buf_x_min = alloca( sizeof(int) * (rect.bottom - rect.top) );
        buf_x_max = alloca( sizeof(int) * (rect.bottom - rect.top) );

        for( i = 0; i < rect.bottom - rect.top; i++ )
        {
            buf_x_min[i] = image->width;
            buf_x_max[i] = -1;
        }

        Image_triangle_line( x[0] - rect.left + image->x, y[0] - rect.top + image->y, x[1] - rect.left + image->x, y[1] - rect.top + image->y, buf_x_min, buf_x_max, &rect );
        Image_triangle_line( x[1] - rect.left + image->x, y[1] - rect.top + image->y, x[2] - rect.left + image->x, y[2] - rect.top + image->y, buf_x_min, buf_x_max, &rect );
        Image_triangle_line( x[2] - rect.left + image->x, y[2] - rect.top + image->y, x[0] - rect.left + image->x, y[0] - rect.top + image->y, buf_x_min, buf_x_max, &rect );

        for( i = 0; i < rect.bottom - rect.top; i++ )
        {
//            printf("y = %d, left = %d, right = %d\n", i, buf_x_min[i], buf_x_max[i]);
            Image_circle_line( buf_x_min[i], buf_x_max[i], i, &rect, col, &texrect );
        }
    }

    image->texture->pD3DTexture->lpVtbl->UnlockRect( image->texture->pD3DTexture, 0 );

    return obj;
}


/*--------------------------------------------------------------------
   イメージオブジェクト間データ転送
 ---------------------------------------------------------------------*/
static VALUE Image_copyRect( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyImage *srcimage;
    struct DXRubyImage *dstimage;
    D3DLOCKED_RECT srctrect;
    D3DLOCKED_RECT dsttrect;
    VALUE vx, vy, data, vx1, vy1, vwidth, vheight;
    int x, y, x1, y1, width, height;
    int i, j;
    RECT srcrect;
    RECT dstrect;
    int *psrc;
    int *pdst;

    rb_scan_args( argc, argv, "34", &vx, &vy, &data, &vx1, &vy1, &vwidth, &vheight );

    dstimage = DXRUBY_GET_STRUCT( Image, obj );
    DXRUBY_CHECK_DISPOSE( dstimage, texture );
//    DXRUBY_CHECK_IMAGE_LOCK( dstimage );
    DXRUBY_CHECK_TYPE( Image, data );
    srcimage = DXRUBY_GET_STRUCT( Image, data );
    DXRUBY_CHECK_DISPOSE( srcimage, texture );

    if( dstimage == srcimage ) rb_raise( eDXRubyError, "The same Image object is specified as the drawing source and the destination - Image_copyRect" );

    x = NUM2INT( vx );
    y = NUM2INT( vy );
    x1 = vx1 == Qnil ? 0 : NUM2INT( vx1 );
    y1 = vy1 == Qnil ? 0 : NUM2INT( vy1 );
    width = vwidth == Qnil ? srcimage->width - x1 : NUM2INT( vwidth );
    height = vheight == Qnil ? srcimage->height - y1 : NUM2INT( vheight );

    /* 画像のクリッピング */
    if( x < 0 )
    {
        x1 -= x;
        width -= x;
        x = 0;
    }
    if( y < 0 )
    {
        y1 -= y;
        height -= y;
        y = 0;
    }
    if( x1 < 0 )
    {
        x -=x1;
        width -= x1;
        x1 = 0;
    }
    if( y1 < 0 )
    {
        y -=y1;
        height -= y1;
        y1 = 0;
    }
    if( x + width > dstimage->width )
    {
        width -= x + width - dstimage->width;
    }
    if( y + height > dstimage->height )
    {
        height -= y + height - dstimage->height;
    }
    if( x1 + width > srcimage->width )
    {
        width -= x1 + width - srcimage->width;
    }
    if( y1 + height > srcimage->height )
    {
        height -= y1 + height - srcimage->height;
    }

    /* 範囲外 */
    if( x >= dstimage->width || y >= dstimage->height || x1 >= srcimage->width || y1 >= srcimage->height ||
        width < 0 || height < 0 )
    {
        return obj;
    }

    dstrect.left = x + dstimage->x;
    dstrect.top = y + dstimage->y;
    dstrect.right = x + dstimage->x + width;
    dstrect.bottom = y + dstimage->y + height;
    srcrect.left = x1 + srcimage->x;
    srcrect.top = y1 + srcimage->y;
    srcrect.right = x1 + srcimage->x + width;
    srcrect.bottom = y1 + srcimage->y + height;

    dstimage->texture->pD3DTexture->lpVtbl->LockRect( dstimage->texture->pD3DTexture, 0, &dsttrect, &dstrect, 0 );
    srcimage->texture->pD3DTexture->lpVtbl->LockRect( srcimage->texture->pD3DTexture, 0, &srctrect, &srcrect, D3DLOCK_READONLY );

    for( i = 0; i < height; i++)
    {
        psrc = (int*)((char *)srctrect.pBits + i * srctrect.Pitch);
        pdst = (int*)((char *)dsttrect.pBits + i * dsttrect.Pitch);
        for( j = 0; j < width; j++)
        {
            *(pdst++) = *(psrc++);
        }
    }

    dstimage->texture->pD3DTexture->lpVtbl->UnlockRect( dstimage->texture->pD3DTexture, 0 );
    srcimage->texture->pD3DTexture->lpVtbl->UnlockRect( srcimage->texture->pD3DTexture, 0 );

    return obj;
}


/*--------------------------------------------------------------------
   イメージオブジェクト間αつきデータ転送
 ---------------------------------------------------------------------*/
static VALUE Image_draw( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyImage *srcimage;
    struct DXRubyImage *dstimage;
    D3DLOCKED_RECT srctrect;
    D3DLOCKED_RECT dsttrect;
    VALUE vx, vy, data, vx1, vy1, vwidth, vheight;
    int x, y, x1, y1, width, height;
    int i, j;
    RECT srcrect;
    RECT dstrect;
    int *psrc;
    int *pdst;

    rb_scan_args( argc, argv, "34", &vx, &vy, &data, &vx1, &vy1, &vwidth, &vheight );

    dstimage = DXRUBY_GET_STRUCT( Image, obj );
    DXRUBY_CHECK_DISPOSE( dstimage, texture );
//    DXRUBY_CHECK_IMAGE_LOCK( dstimage );
    DXRUBY_CHECK_TYPE( Image, data );
    srcimage = DXRUBY_GET_STRUCT( Image, data );
    DXRUBY_CHECK_DISPOSE( srcimage, texture );

    if( dstimage == srcimage ) rb_raise( eDXRubyError, "The same Image object is specified as the drawing source and the destination - Image_draw" );

    x = NUM2INT( vx );
    y = NUM2INT( vy );
    x1 = vx1 == Qnil ? 0 : NUM2INT( vx1 );
    y1 = vy1 == Qnil ? 0 : NUM2INT( vy1 );
    width = vwidth == Qnil ? srcimage->width - x1 : NUM2INT( vwidth );
    height = vheight == Qnil ? srcimage->height - y1 : NUM2INT( vheight );

    /* 画像のクリッピング */
    if( x < 0 )
    {
        x1 -= x;
        width -= x;
        x = 0;
    }
    if( y < 0 )
    {
        y1 -= y;
        height -= y;
        y = 0;
    }
    if( x1 < 0 )
    {
        x -=x1;
        width -= x1;
        x1 = 0;
    }
    if( y1 < 0 )
    {
        y -=y1;
        height -= y1;
        y1 = 0;
    }
    if( x + width > dstimage->width )
    {
        width -= x + width - dstimage->width;
    }
    if( y + height > dstimage->height )
    {
        height -= y + height - dstimage->height;
    }
    if( x1 + width > srcimage->width )
    {
        width -= x1 + width - srcimage->width;
    }
    if( y1 + height > srcimage->height )
    {
        height -= y1 + height - srcimage->height;
    }

    /* 範囲外 */
    if( x >= dstimage->width || y >= dstimage->height || x1 >= srcimage->width || y1 >= srcimage->height ||
        width < 0 || height < 0 )
    {
        return obj;
    }

    dstrect.left = x + dstimage->x;
    dstrect.top = y + dstimage->y;
    dstrect.right = x + dstimage->x + width;
    dstrect.bottom = y + dstimage->y + height;
    srcrect.left = x1 + srcimage->x;
    srcrect.top = y1 + srcimage->y;
    srcrect.right = x1 + srcimage->x + width;
    srcrect.bottom = y1 + srcimage->y + height;

    dstimage->texture->pD3DTexture->lpVtbl->LockRect( dstimage->texture->pD3DTexture, 0, &dsttrect, &dstrect, 0 );
    srcimage->texture->pD3DTexture->lpVtbl->LockRect( srcimage->texture->pD3DTexture, 0, &srctrect, &srcrect, D3DLOCK_READONLY );

    for( i = 0; i < height; i++)
    {
        psrc = (int*)((char *)srctrect.pBits + i * srctrect.Pitch);
        pdst = (int*)((char *)dsttrect.pBits + i * dsttrect.Pitch);
        for( j = 0; j < width; j++)
        {
            struct DXRubyColor s = *((struct DXRubyColor*)(psrc));

            if( s.alpha == 255 )
            {
                *((struct DXRubyColor*)(pdst)) = s;
            }
            else if( s.alpha != 0 )
            {
                struct DXRubyColor d = *((struct DXRubyColor*)(pdst));
                int alpha = (255 - s.alpha) * d.alpha + s.alpha * 255;

                s.red = (((int)s.alpha * (int)s.red * 255) + (int)d.alpha * (int)d.red * (255 - s.alpha) ) / alpha;
                s.green = (((int)s.alpha * (int)s.green * 255) + (int)d.alpha * (int)d.green * (255 - s.alpha) ) / alpha;
                s.blue = (((int)s.alpha * (int)s.blue * 255) + (int)d.alpha * (int)d.blue * (255 - s.alpha) ) / alpha;
                s.alpha = alpha / 255;

                *((struct DXRubyColor*)(pdst)) = s;
            }
            psrc++;
            pdst++;
        }
    }

    dstimage->texture->pD3DTexture->lpVtbl->UnlockRect( dstimage->texture->pD3DTexture, 0 );
    srcimage->texture->pD3DTexture->lpVtbl->UnlockRect( srcimage->texture->pD3DTexture, 0 );

    return obj;
}


static void get_color( VALUE vcolor, int *cr, int *cg, int *cb )
{
    Check_Type(vcolor, T_ARRAY);
    if( RARRAY_LEN(vcolor) == 4 )
    {
        *cr = NUM2INT(rb_ary_entry(vcolor, 1));
        *cg = NUM2INT(rb_ary_entry(vcolor, 2));
        *cb = NUM2INT(rb_ary_entry(vcolor, 3));
    }
    else
    {
        *cr = NUM2INT(rb_ary_entry(vcolor, 0));
        *cg = NUM2INT(rb_ary_entry(vcolor, 1));
        *cb = NUM2INT(rb_ary_entry(vcolor, 2));
    }
}
static void drawfont_sub( int blackboxX, int blackboxY, int baseX, int baseY, int pitch,  char *buf, D3DLOCKED_RECT *dsttrect, int cr, int cg, int cb, int width, int height )
{
    int v, u, xx, yy;

    for( v = baseY < 0 ? -baseY : 0; v < blackboxY; v++ )
    {
        int yy = baseY + v;
        int p1, p2;

        if( yy >= height )
        {
            break;
        }

        p1 = v * pitch;
        p2 = yy * dsttrect->Pitch;
        for( u = baseX < 0 ? -baseX : 0; u < blackboxX; u++ )
        {
            int xx, src;

            xx = baseX + u;
            if( xx >= width )
            {
                break;
            }

            src = (int)buf[ u + p1 ];

            if( src == 64 )
            {
                *((LPDWORD)((char*)dsttrect->pBits + xx * 4 + p2)) = D3DCOLOR_ARGB(0xff, cr, cg, cb);
            }
            else if( src != 0 )
            {
                struct DXRubyColor d = *((struct DXRubyColor*)((char*)dsttrect->pBits + xx * 4 + p2));
                struct DXRubyColor data;
                int temp;
                src = src * 255 / 64;

                temp = (255 - src) * d.alpha + src * 255;
                data.alpha = temp / 255;
                data.red = (src * cr * 255 + (int)d.alpha * d.red * (255 - src)) / temp;
                data.green = (src * cg * 255 + (int)d.alpha * d.green * (255 - src)) / temp;
                data.blue = (src * cb * 255 + (int)d.alpha * d.blue * (255 - src)) / temp;

                *((struct DXRubyColor*)((char*)dsttrect->pBits + xx * 4 + yy * dsttrect->Pitch)) = data;
            }
        }
    }
}

/*--------------------------------------------------------------------
   イメージにフォント描画
 ---------------------------------------------------------------------*/
static VALUE Image_drawFont( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyFont *font;
    struct DXRubyImage *image;
    VALUE vx, vy, vcolor, vstr, vfont;
    int cr=255, cg=255, cb=255, x, y;
    LPDIRECT3DSURFACE9 pD3DSurface;
    HDC hDC;
    RECT rc;
    HRESULT hr;
    D3DLOCKED_RECT srctrect;
    D3DLOCKED_RECT dsttrect;
    int i, j;
    int h;
    RECT srcrect;
    RECT dstrect;
    TEXTMETRIC tm;
    GLYPHMETRICS gm;

    LPWSTR widestr;
    VALUE vwidestr;
    MAT2 mat2 = {{0,1},{0,0},{0,0},{0,1}};

    /* 引数取得 */
    rb_scan_args( argc, argv, "41", &vx, &vy, &vstr, &vfont, &vcolor);

    Check_Type(vstr, T_STRING);

    /* 引数のフォントオブジェクトから中身を取り出す */
    image = DXRUBY_GET_STRUCT( Image, obj );
    DXRUBY_CHECK_DISPOSE( image, texture );
//    DXRUBY_CHECK_IMAGE_LOCK( image );
    DXRUBY_CHECK_TYPE( Font, vfont );
    font = DXRUBY_GET_STRUCT( Font, vfont );
    DXRUBY_CHECK_DISPOSE( font, pD3DXFont );

    x = NUM2INT( vx );
    y = NUM2INT( vy );
    if( vcolor != Qnil )
    {
        get_color( vcolor, &cr, &cg, &cb );
    }

    if( x >= image->width || y >= image->height )
    {
        return obj;
    }

    /* 描画文字のUTF16LE化 */
    if( rb_enc_get_index( vstr ) != 0 )
    {
        vwidestr = rb_str_export_to_enc( vstr, g_enc_utf16 );
    }
    else
    {
        vwidestr = rb_str_conv_enc( vstr, g_enc_sys, g_enc_utf16 );
    }
    widestr = (LPWSTR)RSTRING_PTR( vwidestr );

    hDC = GetDC( g_hWnd );
    SelectObject( hDC, font->hFont );
    GetTextMetrics( hDC, &tm );

    dstrect.left = image->x;
    dstrect.top = image->y;
    dstrect.right = image->x + image->width;
    dstrect.bottom = image->y + image->height;

    image->texture->pD3DTexture->lpVtbl->LockRect( image->texture->pD3DTexture, 0, &dsttrect, &dstrect, 0 );
    for( i = 0; i < RSTRING_LEN( vwidestr ) / 2; i++ )
    {
        char *buf = Font_getGlyph( vfont, *(widestr + i), hDC, &gm, Qnil );
        if( buf )
        {
            drawfont_sub( gm.gmBlackBoxX, gm.gmBlackBoxY, x + gm.gmptGlyphOrigin.x, y + tm.tmAscent - gm.gmptGlyphOrigin.y, (gm.gmBlackBoxX + 3) & 0xfffc, buf, &dsttrect, cr, cg, cb, image->width, image->height );
        }
        x += gm.gmCellIncX;
        if( x >= image->width )
        {
            break;
        }
    }

    image->texture->pD3DTexture->lpVtbl->UnlockRect( image->texture->pD3DTexture, 0 );

    ReleaseDC( g_hWnd, hDC );
    return obj;
}


/* fast_int_hypot from http://demo.and.or.jp/makedemo/effect/math/hypot/fast_hypot.c */
int fast_int_hypot(int lx, int ly)
{
	int len1, len2,t,length;

/*	lx = abs(lx); */
/*	ly = abs(ly); */
	if(lx<0) lx = -lx;
	if(ly<0) ly = -ly;
	/*
		CWD
		XOR EAX,EDX
		SUB EAX,EDX
	*/
	
	if (lx >= ly)
	{
		len1 = lx ; len2 = ly;
	}
	else
	{
		len1 = ly ; len2 = lx;
	}

	t = len2 + (len2 >> 1) ;
	length = len1 - (len1 >> 5) - (len1 >> 7) + (t >> 2) + (t >> 6) ;
	return length;
}
/*--------------------------------------------------------------------
   イメージにフォント描画高機能版
 ---------------------------------------------------------------------*/
VALUE Image_drawFontEx( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyFont *font;
    struct DXRubyImage *image;
    VALUE vx, vy, vcolor, vstr, vfont, voption, vaa_flag;
    VALUE vedge;
    VALUE vshadow;
    int edge_width, edge_level, shadow_x, shadow_y, shadow_edge;
    int cr=255, cg=255, cb=255, x, y;
    int br=0, bg=0, bb=0;
    int sr=0, sg=0, sb=0;
    LPDIRECT3DSURFACE9 pD3DSurface;
    HDC hDC;
    RECT rc;
    HRESULT hr;
    D3DLOCKED_RECT srctrect;
    D3DLOCKED_RECT dsttrect;
    int i, j;
    RECT srcrect;
    RECT dstrect;
    TEXTMETRIC tm;
    GLYPHMETRICS gm;

    LPWSTR widestr;
    VALUE vwidestr;
    MAT2 mat2 = {{0,1},{0,0},{0,0},{0,1}};

    /* 引数取得 */
    if( argc < 4 || argc > 5 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 4, 5 );

    vx = argv[0];
    vy = argv[1];
    vstr = argv[2];
    vfont = argv[3];

    x = NUM2INT( vx );
    y = NUM2INT( vy );

    Check_Type(vstr, T_STRING);

    /* 引数のフォントオブジェクトから中身を取り出す */
    image = DXRUBY_GET_STRUCT( Image, obj );
    DXRUBY_CHECK_DISPOSE( image, texture );
//    DXRUBY_CHECK_IMAGE_LOCK( image );
    DXRUBY_CHECK_TYPE( Font, vfont );
    font = DXRUBY_GET_STRUCT( Font, vfont );
    DXRUBY_CHECK_DISPOSE( font, pD3DXFont );

    if( argc < 5 || argv[4] == Qnil )
    {
        voption = rb_hash_new();
    }
    else
    {
        Check_Type( argv[4], T_HASH );
        voption = argv[4];
    }

    /* 文字の色 */
    vcolor = hash_lookup( voption, symbol_color );
    if( vcolor != Qnil )
    {
        get_color( vcolor, &cr, &cg, &cb );
    }

    /* エッジオプション */
    vedge = hash_lookup( voption, symbol_edge );
    if( vedge == Qnil || vedge == Qfalse )
    {
        edge_width = 0;
        edge_level = 0;
    }
    else
    {
        VALUE vedge_color, vedge_width, vedge_level;
        vedge_color = hash_lookup( voption, symbol_edge_color );
        if( vedge_color != Qnil )
        {
            get_color( vedge_color, &br, &bg, &bb );
        }

        vedge_width = hash_lookup( voption, symbol_edge_width );
        edge_width = vedge_width == Qnil ? 2 : NUM2INT( vedge_width ); /* エッジの幅 */

        vedge_level = hash_lookup( voption, symbol_edge_level );
        edge_level = vedge_level == Qnil ? 4 : NUM2INT( vedge_level ); /* エッジの強さ */
    }

    /* 影オプション */
    vshadow = hash_lookup( voption, symbol_shadow );
    if( vshadow == Qnil || vshadow == Qfalse )
    {
        shadow_x = 0;
        shadow_y = 0;
    }
    else
    {
        VALUE vshadow_color, vshadow_x, vshadow_y, vshadow_edge;
        vshadow_color = hash_lookup( voption, symbol_shadow_color );
        if( vshadow_color != Qnil )
        {
            get_color( vshadow_color, &sr, &sg, &sb );
        }

        vshadow_x = hash_lookup( voption, symbol_shadow_x );
        shadow_x = vshadow_x == Qnil ? NUM2INT( Font_getSize( vfont ) ) / 24 + 1 : NUM2INT(vshadow_x);

        vshadow_y = hash_lookup( voption, symbol_shadow_y );
        shadow_y = vshadow_y == Qnil ? NUM2INT( Font_getSize( vfont ) ) / 24 + 1 : NUM2INT(vshadow_y);

        vshadow_edge = hash_lookup( voption, symbol_shadow_edge );
        if( vshadow_edge == Qnil || vshadow_edge == Qfalse )
        {
            shadow_edge = 0;
        }
        else
        {
            shadow_edge = 1;
        }
    }

    /* aaフラグ */
    vaa_flag = hash_lookup( voption, symbol_aa );

    if( x >= image->width || y >= image->height )
    {
        return obj;
    }

    /* 描画文字のUTF16LE化 */
    if( rb_enc_get_index( vstr ) != 0 )
    {
        vwidestr = rb_str_export_to_enc( vstr, g_enc_utf16 );
    }
    else
    {
        vwidestr = rb_str_conv_enc( vstr, g_enc_sys, g_enc_utf16 );
    }
    widestr = (LPWSTR)RSTRING_PTR( vwidestr );

    hDC = GetDC( g_hWnd );
    SelectObject( hDC, font->hFont );
    GetTextMetrics( hDC, &tm );

    dstrect.left = image->x;
    dstrect.top = image->y;
    dstrect.right = image->x + image->width;
    dstrect.bottom = image->y + image->height;

    image->texture->pD3DTexture->lpVtbl->LockRect( image->texture->pD3DTexture, 0, &dsttrect, &dstrect, 0 );

    for( i = 0; i < RSTRING_LEN( vwidestr ) / 2; i++ )
    {
        char *buf = Font_getGlyph( vfont, *(widestr + i), hDC, &gm, vaa_flag );
        if( buf )
        {
            int v, u;

            if( edge_width > 0 && edge_level > 0 )
            { /* エッジ生成。吉里吉里2のソースを参考にしている。 */
                int edge_pitch  = ((gm.gmBlackBoxX + edge_width * 2 - 1) >> 2) + 1 << 2;
                unsigned char *blurbuf = alloca( edge_pitch * (gm.gmBlackBoxY + edge_width * 2) ); /* バッファ */
                int lvsum = 0;
                memset( blurbuf, 0, edge_pitch * (gm.gmBlackBoxY + edge_width * 2) ); /* バッファのクリア */

                for( v = -edge_width; v <= edge_width; v++ )
                {
                    for( u = -edge_width; u <= edge_width; u++ )
                    {
                        int len = fast_int_hypot(u, v);
                        if(len <= edge_width)
                            lvsum += (edge_width - len + 1);
                    }
                }

                if( lvsum )
                {
                    lvsum = (1<<18) / lvsum;
                }
                else
                {
                    lvsum = (1<<18);
                }

                for( v = -edge_width; v <= edge_width; v++ )
                {
                    for( u = -edge_width; u <= edge_width; u++ )
                    {
                        int len = fast_int_hypot(u, v);
                        if( len <= edge_width )
                        {
                            int sx, sy;
                            for( sy = 0; sy < gm.gmBlackBoxY; sy++ )
                            {
                                for( sx = 0; sx < gm.gmBlackBoxX; sx++ )
                                {
                                    int temp;
                                    temp = blurbuf[(v + sy + edge_width) * edge_pitch + u + sx + edge_width] + ((buf[ sx + sy * ((gm.gmBlackBoxX + 3) & 0xfffc) ] * lvsum * (edge_width - len + 1) * edge_level) >> 18);
                                    if( temp > 64 ) temp = 64;
                                    blurbuf[(v + sy + edge_width) * edge_pitch + u + sx + edge_width] = temp;
                                }
                            }
                        }
                    }
                }

                if( shadow_x != 0 || shadow_y != 0 )
                {
                    /* エッジがある場合の影の描画 */
                    if( shadow_edge == 1 )
                    {
                        drawfont_sub( gm.gmBlackBoxX + edge_width * 2, gm.gmBlackBoxY + edge_width * 2, x + gm.gmptGlyphOrigin.x - edge_width + shadow_x, y + tm.tmAscent - gm.gmptGlyphOrigin.y - edge_width + shadow_y, edge_pitch, blurbuf, &dsttrect, sr, sg, sb, image->width, image->height );
                    }
                    /* エッジがあるけどエッジ無し影の描画 */
                    else
                    {
                        drawfont_sub( gm.gmBlackBoxX, gm.gmBlackBoxY, x + gm.gmptGlyphOrigin.x + shadow_x, y + tm.tmAscent - gm.gmptGlyphOrigin.y + shadow_y, (gm.gmBlackBoxX + 3) & 0xfffc, buf, &dsttrect, sr, sg, sb, image->width, image->height );
                    }
                }

                /* エッジの描画 */
                drawfont_sub( gm.gmBlackBoxX + edge_width * 2, gm.gmBlackBoxY + edge_width * 2, x + gm.gmptGlyphOrigin.x - edge_width, y + tm.tmAscent - gm.gmptGlyphOrigin.y - edge_width, edge_pitch, blurbuf, &dsttrect, br, bg, bb, image->width, image->height );

            }
            else if( shadow_x != 0 || shadow_y != 0 ) /* エッジがない場合の影の描画 */
            {
                drawfont_sub( gm.gmBlackBoxX, gm.gmBlackBoxY, x + gm.gmptGlyphOrigin.x + shadow_x, y + tm.tmAscent - gm.gmptGlyphOrigin.y + shadow_y, (gm.gmBlackBoxX + 3) & 0xfffc, buf, &dsttrect, sr, sg, sb, image->width, image->height );
            }

            /* 文字の描画 */
            drawfont_sub( gm.gmBlackBoxX, gm.gmBlackBoxY, x + gm.gmptGlyphOrigin.x, y + tm.tmAscent - gm.gmptGlyphOrigin.y, (gm.gmBlackBoxX + 3) & 0xfffc, buf, &dsttrect, cr, cg, cb, image->width, image->height );

        }
        x += gm.gmCellIncX;
        if( x >= image->width )
        {
            break;
        }
    }

    image->texture->pD3DTexture->lpVtbl->UnlockRect( image->texture->pD3DTexture, 0 );

    ReleaseDC( g_hWnd, hDC );
    return obj;
}


/*--------------------------------------------------------------------
   画像にエフェクトをかける
 ---------------------------------------------------------------------*/
VALUE Image_effect( int argc, VALUE *argv, VALUE self )
{
    struct DXRubyImage *srcimage;
    struct DXRubyImage *dstimage;
    struct DXRubyTexture *texture;
    VALUE vx, vy, vcolor, vstr, vfont, voption;
    VALUE vedge;
    VALUE vshadow;
    int edge_width, edge_level, shadow_x, shadow_y, shadow_edge;
    int cr=255, cg=255, cb=255, x, y;
    int br=0, bg=0, bb=0;
    int sr=0, sg=0, sb=0;
    LPDIRECT3DSURFACE9 pD3DSurface;
    RECT rc;
    HRESULT hr;
    D3DLOCKED_RECT srctrect;
    D3DLOCKED_RECT dsttrect;
    int i, j;
    RECT srcrect;
    RECT dstrect;
    char *buf;
    VALUE obj;
    D3DSURFACE_DESC desc;

    if( argc < 0 || argc > 1 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 0, 1 );

    srcimage = DXRUBY_GET_STRUCT( Image, self );
    DXRUBY_CHECK_DISPOSE( srcimage, texture );
//    DXRUBY_CHECK_IMAGE_LOCK( srcimage );

    if( argc < 1 || argv[0] == Qnil )
    {
        voption = rb_hash_new();
    }
    else
    {
        voption = argv[0];
    }

    /* 文字の色 */
    vcolor = hash_lookup( voption, symbol_color );
    if( vcolor != Qnil )
    {
        get_color( vcolor, &cr, &cg, &cb );
    }

    /* エッジオプション */
    vedge = hash_lookup( voption, symbol_edge );
    if( vedge == Qnil || vedge == Qfalse )
    {
        edge_width = 0;
        edge_level = 0;
    }
    else
    {
        VALUE vedge_color, vedge_width, vedge_level;
        vedge_color = hash_lookup( voption, symbol_edge_color );
        if( vedge_color != Qnil )
        {
            get_color( vedge_color, &br, &bg, &bb );
        }

        vedge_width = hash_lookup( voption, symbol_edge_width );
        edge_width = vedge_width == Qnil ? 2 : NUM2INT( vedge_width ); /* エッジの幅 */

        vedge_level = hash_lookup( voption, symbol_edge_level );
        edge_level = vedge_level == Qnil ? 4 : NUM2INT( vedge_level ); /* エッジの強さ */
    }

    /* 影オプション */
    vshadow = hash_lookup( voption, symbol_shadow );
    if( vshadow == Qnil || vshadow == Qfalse )
    {
        shadow_x = 0;
        shadow_y = 0;
    }
    else
    {
        VALUE vshadow_color, vshadow_x, vshadow_y, vshadow_edge;
        vshadow_color = hash_lookup( voption, symbol_shadow_color );
        if( vshadow_color != Qnil )
        {
            get_color( vshadow_color, &sr, &sg, &sb );
        }

        vshadow_x = hash_lookup( voption, symbol_shadow_x );
        shadow_x = vshadow_x == Qnil ? (int)srcimage->height / 24 + 1 : NUM2INT(vshadow_x);

        vshadow_y = hash_lookup( voption, symbol_shadow_y );
        shadow_y = vshadow_y == Qnil ? (int)srcimage->height / 24 + 1 : NUM2INT(vshadow_y);

        vshadow_edge = hash_lookup( voption, symbol_shadow_edge );
        if( vshadow_edge == Qnil || vshadow_edge == Qfalse )
        {
            shadow_edge = 0;
        }
        else
        {
            shadow_edge = 1;
        }
    }

    srcrect.left = srcimage->x;
    srcrect.top = srcimage->y;
    srcrect.right = srcimage->x + srcimage->width;
    srcrect.bottom = srcimage->y + srcimage->height;

    buf = alloca( srcimage->width * srcimage->height );
    srcimage->texture->pD3DTexture->lpVtbl->LockRect( srcimage->texture->pD3DTexture, 0, &srctrect, &srcrect, D3DLOCK_READONLY );
    for( y = 0; y < srcimage->height; y++ )
    {
        for( x = 0; x < srcimage->width; x++)
        {
            buf[y * (int)srcimage->width + x] = (char)(((int)*((unsigned char *)srctrect.pBits + x * 4 + y * srctrect.Pitch + 2)) *
                                                       ((int)*((unsigned char *)srctrect.pBits + x * 4 + y * srctrect.Pitch + 3)) * 64 / 255 / 255);
        }
    }

    srcimage->texture->pD3DTexture->lpVtbl->UnlockRect( srcimage->texture->pD3DTexture, 0 );

    /* 新Image生成 */
    obj = Image_allocate( cImage );

    /* 新Imageのポインタ取得 */
    dstimage = DXRUBY_GET_STRUCT( Image, obj );

    /* テクスチャメモリ取得 */
    texture = (struct  DXRubyTexture *)malloc( sizeof( struct DXRubyTexture ) );

    if( texture == NULL )
    {
        rb_raise( eDXRubyError, "Out of memory - Image_initialize" );
    }

    DXRUBY_RETRY_START;
    /* テクスチャオブジェクトを作成する */
    hr = D3DXCreateTexture( g_pD3DDevice, srcimage->width + edge_width * 2 + shadow_x, srcimage->height + edge_width * 2 + shadow_y,
                                      1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                      &texture->pD3DTexture);
    DXRUBY_RETRY_END;
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Create texture error - Image_initialize" );
    }

    texture->refcount = 1;
    texture->pD3DTexture->lpVtbl->GetLevelDesc(texture->pD3DTexture, 0, &desc );
    texture->width = (float)desc.Width;
    texture->height = (float)desc.Height;

    g_iRefAll++;

    dstimage->texture = texture;
    dstimage->x = 0;
    dstimage->y = 0;
    dstimage->width = srcimage->width + edge_width * 2 + shadow_x;
    dstimage->height = srcimage->height + edge_width * 2 + shadow_y;
//    dstimage->lockcount = 0;

    fill( 0, 0, dstimage->width - 1, dstimage->height - 1, 0, dstimage );

    dstrect.left = 0;
    dstrect.top = 0;
    dstrect.right = dstimage->width;
    dstrect.bottom = dstimage->height;
    dstimage->texture->pD3DTexture->lpVtbl->LockRect( dstimage->texture->pD3DTexture, 0, &dsttrect, &dstrect, 0 );

    x = y = edge_width;

    {
        int v, u;

        if( edge_width > 0 && edge_level > 0 )
        { /* エッジ生成。吉里吉里2のソースを参考にしている。 */
            int edge_pitch  = (((int)srcimage->width + edge_width * 2 - 1) >> 2) + 1 << 2;
            unsigned char *blurbuf = alloca( edge_pitch * (srcimage->height + edge_width * 2) ); /* バッファ */
            int lvsum = 0;
            memset( blurbuf, 0, edge_pitch * (srcimage->height + edge_width * 2) ); /* バッファのクリア */

            for( v = -edge_width; v <= edge_width; v++ )
            {
                for( u = -edge_width; u <= edge_width; u++ )
                {
                    int len = fast_int_hypot(u, v);
                    if(len <= edge_width)
                        lvsum += (edge_width - len + 1);
                }
            }

            if( lvsum )
            {
                lvsum = (1<<18) / lvsum;
            }
            else
            {
                lvsum = (1<<18);
            }

            for( v = -edge_width; v <= edge_width; v++ )
            {
                for( u = -edge_width; u <= edge_width; u++ )
                {
                    int len = fast_int_hypot(u, v);
                    if( len <= edge_width )
                    {
                        int sx, sy;
                        for( sy = 0; sy < srcimage->height; sy++ )
                        {
                            for( sx = 0; sx < srcimage->width; sx++ )
                            {
                                int temp;
                                temp = blurbuf[(v + sy + edge_width) * edge_pitch + u + sx + edge_width] + ((buf[ sx + sy * (int)srcimage->width ] * lvsum * (edge_width - len + 1) * edge_level) >> 18);
                                if( temp > 64 ) temp = 64;
                                blurbuf[(v + sy + edge_width) * edge_pitch + u + sx + edge_width] = temp;
                            }
                        }
                    }
                }
            }

            if( shadow_x != 0 || shadow_y != 0 )
            {
                /* エッジがある場合の影の描画 */
                if( shadow_edge == 1 )
                {
                    drawfont_sub( srcimage->width + edge_width * 2, srcimage->height + edge_width * 2, x - edge_width + shadow_x, y - edge_width + shadow_y, edge_pitch, blurbuf, &dsttrect, sr, sg, sb, dstimage->width, dstimage->height );
                }
                /* エッジがあるけどエッジ無し影の描画 */
                else
                {
                    drawfont_sub( srcimage->width, srcimage->height, x + shadow_x, y + shadow_y, srcimage->width, buf, &dsttrect, sr, sg, sb, dstimage->width, dstimage->height );
                }
            }

            /* エッジの描画 */
            drawfont_sub( srcimage->width + edge_width * 2, srcimage->height + edge_width * 2, x - edge_width, y - edge_width, edge_pitch, blurbuf, &dsttrect, br, bg, bb, dstimage->width, dstimage->height );

        }
        else if( shadow_x != 0 || shadow_y != 0 ) /* エッジがない場合の影の描画 */
        {
            drawfont_sub( srcimage->width, srcimage->height,x + shadow_x,y + shadow_y, srcimage->width, buf, &dsttrect, sr, sg, sb, dstimage->width, dstimage->height );
        }

        /* 文字の描画 */
        drawfont_sub( srcimage->width, srcimage->height, x, y, srcimage->width, buf, &dsttrect, cr, cg, cb, dstimage->width, dstimage->height );

    }

    dstimage->texture->pD3DTexture->lpVtbl->UnlockRect( dstimage->texture->pD3DTexture, 0 );

    return obj;
}


/*--------------------------------------------------------------------
   HLSで色を調整する
 ---------------------------------------------------------------------*/
static void RGBToHSB( unsigned int rgb, int *hue, int *saturation, int *brightness )
{
    float r, g, b, h, s, l, cmax, cmin, sum, dif;

    r = ((rgb & 0x00ff0000) >> 16) / 255.0f;
    g = ((rgb & 0x0000ff00) >> 8 ) / 255.0f;
    b = ((rgb & 0x000000ff)      ) / 255.0f;

    cmax = (r > g) ? ((b > r) ? b : r) : ((b > g) ? b : g);
    cmin = (r < g) ? ((b < r) ? b : r) : ((b < g) ? b : g);
//    printf("max %f\n", cmax);
//    printf("min %f\n", cmin);

    sum = cmax + cmin;
    dif = cmax - cmin;
//    printf("sum %f\n", sum);
//    printf("dif %f\n", dif);

    l = sum / 2.0f;

    if( dif == 0.0f )
    {
        h = 0;
        s = 0;
    }
    else
    {
        if( l <= 0.5f )
        {
            s = dif / sum;
        }
        else
        {
            s = dif / (2.0f - sum);
        }

        if( r == cmax )
        {
            h = (g - b) / dif;
        }
        if( g == cmax )
        {
            h = 2.0f + (b - r) / dif;
        }
        if( b == cmax )
        {
            h = 4.0f + (r - g) / dif;
        }

        h = h * 60.0f;
        if( h < 0.0f )
        {
            h = h + 360.0f;
        }
    }

    *hue = (int)h;
    *saturation = (int)(s * 100);
    *brightness = (int)(l * 100);
}

static int HToRGB( float cmin, float cmax, int hue )
{
    int h = hue % 360;
    if( h < 0 )
    {
        h = h + 360;
    }

    if( h < 60 )
    {
        return (int)((cmin + (cmax - cmin) * h / 60) * 255);
    }
    else if( h < 180 )
    {
        return (int)(cmax * 255);
    }
    else if( h < 240 )
    {
        return (int)((cmin + (cmax - cmin) * (240 - h) / 60) * 255);
    }
    else
    {
        return (int)(cmin * 255);
    }
}

static int HSBToRGB( int hue, int saturation, int brightness )
{
    int h;
    float s, l, cmax, cmin;

    s = saturation / 100.0f;
    l = brightness / 100.0f;

    h = hue % 360;
    if( h < 0 )
    {
        h = h + 360;
    }

    if( l < 0.0f )
    {
        l = 0.0f;
    }
    if( l > 1.0f )
    {
        l = 1.0f;
    }

    if( s < 0.0f )
    {
        s = 0.0f;
    }
    if( s > 1.0f )
    {
        s = 1.0f;
    }

    if( l <= 0.5f )
    {
        cmin = l * (1.0f - s);
        cmax = 2.0f * l - cmin;
    }
    else
    {
        cmax = l * (1.0f - s) + s;
        cmin = 2.0f * l - cmax;
    }

    return (HToRGB(cmin, cmax, h + 120) << 16) | (HToRGB(cmin, cmax, h) << 8) | HToRGB(cmin, cmax, h - 120);
}

VALUE Image_change_hls( int argc, VALUE *argv, VALUE self )
{
    struct DXRubyImage *srcimage = DXRUBY_GET_STRUCT( Image, self );
    struct DXRubyImage *dstimage;
    struct DXRubyTexture *texture;
    D3DLOCKED_RECT srctrect;
    D3DLOCKED_RECT dsttrect;
    int i, j;
    RECT srcrect;
    RECT dstrect;
    HRESULT hr;
    D3DSURFACE_DESC desc;
    int *psrc;
    int *pdst;
    VALUE obj;
    int hue, luminance, saturation;
    int dhue, dluminance, dsaturation;

    DXRUBY_CHECK_DISPOSE( srcimage, texture );

    if( argc < 1 || argc > 3 ) rb_raise( rb_eArgError, "wrong number of arguments (%d for %d..%d)", argc, 1, 3 );

    dhue = NUM2INT( argv[0] );
    dluminance = argc < 2 || argv[1] == Qnil ? 0 : NUM2INT( argv[1] );
    dsaturation = argc < 3 || argv[2] == Qnil ? 0 : NUM2INT( argv[2] );

    obj = Image_allocate( cImage );
    dstimage = DXRUBY_GET_STRUCT( Image, obj );
    g_iRefAll++;

    /* テクスチャメモリ取得 */
    texture = (struct  DXRubyTexture *)malloc( sizeof( struct DXRubyTexture ) );
    if( texture == NULL )
    {
        rb_raise( eDXRubyError, "Out of memory - Image_change_hls" );
    }

    DXRUBY_RETRY_START;
    /* テクスチャオブジェクトを作成する */
    hr = D3DXCreateTexture( g_pD3DDevice, srcimage->width, srcimage->height,
                            1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                            &texture->pD3DTexture);
    DXRUBY_RETRY_END;
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Create texture error - Image_change_hls" );
    }

    texture->refcount = 1;

    texture->pD3DTexture->lpVtbl->GetLevelDesc(texture->pD3DTexture, 0, &desc );
    texture->width = (float)desc.Width;
    texture->height = (float)desc.Height;

    dstimage->texture = texture;
    dstimage->x = 0;
    dstimage->y = 0;
    dstimage->width = srcimage->width;
    dstimage->height =srcimage-> height;
//    dstimage->lockcount = 0;

    /* イメージコピー */
    dstrect.left = 0;
    dstrect.top = 0;
    dstrect.right = srcimage->width;
    dstrect.bottom = srcimage->height;
    srcrect.left = srcimage->x;
    srcrect.top = srcimage->y;
    srcrect.right = srcimage->x + srcimage->width;
    srcrect.bottom = srcimage->y + srcimage->height;

    dstimage->texture->pD3DTexture->lpVtbl->LockRect( dstimage->texture->pD3DTexture, 0, &dsttrect, &dstrect, 0 );
    srcimage->texture->pD3DTexture->lpVtbl->LockRect( srcimage->texture->pD3DTexture, 0, &srctrect, &srcrect, D3DLOCK_READONLY );

    for( i = 0; i < srcimage->height; i++)
    {
        psrc = (int*)((char *)srctrect.pBits + i * srctrect.Pitch);
        pdst = (int*)((char *)dsttrect.pBits + i * dsttrect.Pitch);
        for( j = 0; j < srcimage->width; j++)
        {
            int s = *(psrc++);
            if( (s & 0xff000000) == 0 )
            {
                *(pdst++) = s;
            }
            else
            {
                int col;
                RGBToHSB( s, &hue, &saturation, &luminance );
                col = HSBToRGB( hue + dhue, saturation + dsaturation, luminance + dluminance );
                *(pdst++) = (s & 0xff000000) | col ;
            }
        }
    }

    dstimage->texture->pD3DTexture->lpVtbl->UnlockRect( dstimage->texture->pD3DTexture, 0 );
    srcimage->texture->pD3DTexture->lpVtbl->UnlockRect( srcimage->texture->pD3DTexture, 0 );

    return obj;
}


/*--------------------------------------------------------------------
   イメージをファイルに保存する。
 ---------------------------------------------------------------------*/
VALUE Image_save( int argc, VALUE *argv, VALUE self )
{
    HRESULT hr;
    struct DXRubyImage *image = DXRUBY_GET_STRUCT( Image, self );
    VALUE vfilename, vformat, vsjisstr, vdowncase;
    char ext[5][6] = {".jpeg", ".jpg", ".png", ".bmp", ".dds"};
    char len[5] = {4, 3, 3, 3, 3};
    int format[5] = {FORMAT_JPG, FORMAT_JPG, FORMAT_PNG, FORMAT_BMP, FORMAT_DDS};
    int i, j, k, f;

    rb_scan_args( argc, argv, "11", &vfilename, &vformat );

    DXRUBY_CHECK_DISPOSE( image, texture );

    if( rb_enc_get_index( vfilename ) != 0 )
    {
        vsjisstr = rb_str_export_to_enc( vfilename, g_enc_sys );
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

    hr = D3DXSaveTextureToFile(
            RSTRING_PTR( vsjisstr ),                            /* 保存ファイル名 */
            f,                                                  /* ファイルフォーマット */
            (IDirect3DBaseTexture9*)image->texture->pD3DTexture,/* 保存するサーフェス */
            NULL);                                              /* パレット */
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Save error - Image_save" );
    }

    return self;
}


/*--------------------------------------------------------------------
   イメージの開始位置xを返す。
 ---------------------------------------------------------------------*/
static VALUE Image_getX( VALUE obj )
{
    struct DXRubyImage *image = DXRUBY_GET_STRUCT( Image, obj );

    DXRUBY_CHECK_DISPOSE( image, texture );

    return INT2FIX( image->x );
}


/*--------------------------------------------------------------------
   イメージの開始位置yを返す。
 ---------------------------------------------------------------------*/
static VALUE Image_getY( VALUE obj )
{
    struct DXRubyImage *image = DXRUBY_GET_STRUCT( Image, obj );

    DXRUBY_CHECK_DISPOSE( image, texture );

    return INT2FIX( image->y );
}


/*--------------------------------------------------------------------
   イメージのサイズ（幅）を返す。
 ---------------------------------------------------------------------*/
static VALUE Image_getWidth( VALUE obj )
{
    struct DXRubyImage *image = DXRUBY_GET_STRUCT( Image, obj );

    DXRUBY_CHECK_DISPOSE( image, texture );

    return INT2FIX( image->width );
}


/*--------------------------------------------------------------------
   イメージのサイズ（高さ）を返す。
 ---------------------------------------------------------------------*/
static VALUE Image_getHeight( VALUE obj )
{
    struct DXRubyImage *image = DXRUBY_GET_STRUCT( Image, obj );

    DXRUBY_CHECK_DISPOSE( image, texture );

    return INT2FIX( image->height );
}


/*--------------------------------------------------------------------
   イメージの開始位置xを設定する。
 ---------------------------------------------------------------------*/
static VALUE Image_setX( VALUE obj, VALUE vx )
{
    struct DXRubyImage *image = DXRUBY_GET_STRUCT( Image, obj );
    int x = NUM2INT( vx );

    DXRUBY_CHECK_DISPOSE( image, texture );
//    DXRUBY_CHECK_IMAGE_LOCK( image );

    if( x < 0 || x + image->width > image->texture->width )
    {
        rb_raise( eDXRubyError, "bad value of x - Image#x=" );
    }

    image->x = x;
    return vx;
}


/*--------------------------------------------------------------------
   イメージの開始位置yを設定する。
 ---------------------------------------------------------------------*/
static VALUE Image_setY( VALUE obj, VALUE vy )
{
    struct DXRubyImage *image = DXRUBY_GET_STRUCT( Image, obj );
    int y = NUM2INT( vy );

    DXRUBY_CHECK_DISPOSE( image, texture );
//    DXRUBY_CHECK_IMAGE_LOCK( image );

    if( y < 0 || y + image->height > image->texture->height )
    {
        rb_raise( eDXRubyError, "bad value of y - Image#y=" );
    }

    image->y = y;
    return vy;
}


/*--------------------------------------------------------------------
   イメージのサイズ（幅）を設定する。
 ---------------------------------------------------------------------*/
static VALUE Image_setWidth( VALUE obj, VALUE vwidth )
{
    struct DXRubyImage *image = DXRUBY_GET_STRUCT( Image, obj );
    int width = NUM2INT( vwidth );

    DXRUBY_CHECK_DISPOSE( image, texture );
//    DXRUBY_CHECK_IMAGE_LOCK( image );

    if( width < 1 || width + image->x > image->texture->width )
    {
        rb_raise( eDXRubyError, "bad value of width - Image#width=" );
    }

    image->width = width;
    return vwidth;
}


/*--------------------------------------------------------------------
   イメージのサイズ（高さ）を設定する。
 ---------------------------------------------------------------------*/
static VALUE Image_setHeight( VALUE obj, VALUE vheight )
{
    struct DXRubyImage *image = DXRUBY_GET_STRUCT( Image, obj );
    int height = NUM2INT( vheight );

    DXRUBY_CHECK_DISPOSE( image, texture );
//    DXRUBY_CHECK_IMAGE_LOCK( image );

    if( height < 1 || height + image->y > image->texture->height )
    {
        rb_raise( eDXRubyError, "bad value of height - Image#height=" );
    }

    image->height = height;
    return vheight;
}


/*--------------------------------------------------------------------
   Perlin Noise
 ---------------------------------------------------------------------*/

//http://postd.cc/understanding-perlin-noise/
static int permutation[512] = { 151,160,137,91,90,15,                               // Hash lookup table as defined by Ken Perlin.  This is a randomly
    131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,    // arranged array of all numbers from 0-255 inclusive.
    190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
    88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
    77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
    102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
    135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
    5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
    223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
    129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
    251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
    49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
    138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

static int *perlinp;

static double grad(int hash, double x, double y, double z) {
    switch(hash & 0xF)
    {
        case 0x0: return  x + y;
        case 0x1: return -x + y;
        case 0x2: return  x - y;
        case 0x3: return -x - y;
        case 0x4: return  x + z;
        case 0x5: return -x + z;
        case 0x6: return  x - z;
        case 0x7: return -x - z;
        case 0x8: return  y + z;
        case 0x9: return -y + z;
        case 0xA: return  y - z;
        case 0xB: return -y - z;
        case 0xC: return  y + x;
        case 0xD: return -y + z;
        case 0xE: return  y - x;
        case 0xF: return -y - z;
        default: return 0; // never happens
    }
}

static double fade(double t) {
                                                         // Fade function as defined by Ken Perlin.  This eases coordinate values
                                                         // so that they will "ease" towards integral values.  This ends up smoothing
                                                         // the final output.
    return t * t * t * (t * (t * 6 - 15) + 10);          // 6t^5 - 15t^4 + 10t^3
}

static double lerp(double a, double b, double x) {
    return a + x * (b - a);
}

static double perlin(double x, double y, double z, int repeat_x, int repeat_y, int repeat_z) {
    {
        int xi = (int)x % repeat_x;                               // Calculate the "unit cube" that the point asked will be located in
        int yi = (int)y % repeat_y;                               // The left bound is ( |_x_|,|_y_|,|_z_| ) and the right bound is that
        int zi = (int)z % repeat_z;                               // plus 1.  Next we calculate the location (from 0.0 to 1.0) in that cube.
        double xf = x-(int)x;                                // We also fade the location to smooth the result.
        double yf = y-(int)y;
        double zf = z-(int)z;
        double u = fade(xf);
        double v = fade(yf);
        double w = fade(zf);
        double x1, x2, y1, y2;

        int aaa, aba, aab, abb, baa, bba, bab, bbb;
        aaa = perlinp[perlinp[perlinp[ xi            ]+ yi            ]+ zi            ];
        aba = perlinp[perlinp[perlinp[ xi            ]+(yi+1)%repeat_y]+ zi            ];
        aab = perlinp[perlinp[perlinp[ xi            ]+ yi            ]+(zi+1)%repeat_z];
        abb = perlinp[perlinp[perlinp[ xi            ]+(yi+1)%repeat_y]+(zi+1)%repeat_z];
        baa = perlinp[perlinp[perlinp[(xi+1)%repeat_x]+ yi            ]+ zi            ];
        bba = perlinp[perlinp[perlinp[(xi+1)%repeat_x]+(yi+1)%repeat_y]+ zi            ];
        bab = perlinp[perlinp[perlinp[(xi+1)%repeat_x]+ yi            ]+(zi+1)%repeat_z];
        bbb = perlinp[perlinp[perlinp[(xi+1)%repeat_x]+(yi+1)%repeat_y]+(zi+1)%repeat_z];

        x1 = lerp(grad (aaa, xf  , yf  , zf),                // The gradient function calculates the dot product between a pseudorandom
                  grad (baa, xf-1, yf  , zf),                // gradient vector and the vector from the input coordinate to the 8
                  u);                                        // surrounding points in its unit cube.
        x2 = lerp(grad (aba, xf  , yf-1, zf),                // This is all then lerped together as a sort of weighted average based on the faded (u,v,w)
                  grad (bba, xf-1, yf-1, zf),                // values we made earlier.
                  u);
        y1 = lerp(x1, x2, v);

        x1 = lerp(grad (aab, xf  , yf  , zf-1),
                  grad (bab, xf-1, yf  , zf-1),
                  u);
        x2 = lerp(grad (abb, xf  , yf-1, zf-1),
                  grad (bbb, xf-1, yf-1, zf-1),
                  u);
        y2 = lerp (x1, x2, v);

        return (lerp (y1, y2, w)+1)/2;                        // For convenience we bound it to 0 - 1 (theoretical min/max before is -1 - 1)
    }
}

static double OctavePerlin(double x, double y, double z, int octaves, double persistence, int repeat_x, int repeat_y, int repeat_z) {
    double total = 0;
    double frequency = 1;
    double amplitude = 1;
    double maxValue = 0; // Used for normalizing result to 0.0 - 1.0
    int i;

    for( i = 0; i < octaves; i++ )
    {
        total += perlin( x * frequency, y * frequency, z * frequency, repeat_x, repeat_y, repeat_z ) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2;
    }
    return total / maxValue;
}

static double CustomPerlin(double x, double y, double z, VALUE vary, int repeat_x, int repeat_y, int repeat_z) {
    double total = 0;
    double amplitude = 1;
    double maxValue = 0; // Used for normalizing result to 0.0 - 1.0
    int i;

    for( i = 0; i < RARRAY_LEN(vary); i++)
    {
        double frequency = NUM2DBL( rb_ary_entry( rb_ary_entry( vary, i ), 0 ) );
        double amplitude = NUM2DBL( rb_ary_entry( rb_ary_entry( vary, i ), 1 ) );

        total += perlin( x * frequency, y * frequency, z * frequency, repeat_x, repeat_y, repeat_z ) * amplitude;
        maxValue += amplitude;
    }
    return total / maxValue;
}

static VALUE Image_perlin_noise( int argc, VALUE *argv, VALUE obj )
{
    VALUE vx, vy, vz, vrepeat_x, vrepeat_y, vrepeat_z;
    int repeat_x, repeat_y, repeat_z;

    rb_scan_args( argc, argv, "33", &vx, &vy, &vz, &vrepeat_x, &vrepeat_y, &vrepeat_z );

    repeat_x = vrepeat_x == Qnil ? 256 : NUM2INT(vrepeat_x);
    repeat_y = vrepeat_y == Qnil ? 256 : NUM2INT(vrepeat_y);
    repeat_z = vrepeat_z == Qnil ? 256 : NUM2INT(vrepeat_z);

    return rb_float_new( perlin( NUM2DBL(vx), NUM2DBL(vy), NUM2DBL(vz), repeat_x, repeat_y, vrepeat_z ) );
}

static VALUE Image_octave_perlin_noise( int argc, VALUE *argv, VALUE obj )
{
    VALUE vx, vy, vz, voctave, vpersistence, vrepeat_x, vrepeat_y, vrepeat_z;
    int repeat_x, repeat_y, repeat_z;

    rb_scan_args( argc, argv, "53", &vx, &vy, &vz, &voctave, &vpersistence, &vrepeat_x, &vrepeat_y, &vrepeat_z );

    repeat_x = vrepeat_x == Qnil ? 256 : NUM2INT(vrepeat_x);
    repeat_y = vrepeat_y == Qnil ? 256 : NUM2INT(vrepeat_y);
    repeat_z = vrepeat_z == Qnil ? 256 : NUM2INT(vrepeat_z);

    return rb_float_new( OctavePerlin( NUM2DBL(vx), NUM2DBL(vy), NUM2DBL(vz), NUM2INT(voctave), NUM2DBL(vpersistence), repeat_x, repeat_y, repeat_z ) );
}

static VALUE Image_custom_perlin_noise( int argc, VALUE *argv, VALUE obj )
{
    VALUE vx, vy, vz, vary, vrepeat_x, vrepeat_y, vrepeat_z;
    int repeat_x, repeat_y, repeat_z;

    rb_scan_args( argc, argv, "43", &vx, &vy, &vz, &vary, &vrepeat_x, &vrepeat_y, &vrepeat_z );

    Check_Type( vary, T_ARRAY );

    repeat_x = vrepeat_x == Qnil ? 256 : NUM2INT(vrepeat_x);
    repeat_y = vrepeat_y == Qnil ? 256 : NUM2INT(vrepeat_y);
    repeat_z = vrepeat_z == Qnil ? 256 : NUM2INT(vrepeat_z);

    return rb_float_new( CustomPerlin( NUM2DBL(vx), NUM2DBL(vy), NUM2DBL(vz), vary, repeat_x, repeat_y, vrepeat_z ) );
}

static VALUE Image_perlin_seed( VALUE obj, VALUE vseed )
{
    int i;

    srand(NUM2INT(vseed));

    free( perlinp );
    perlinp = malloc(512*sizeof(int));

    for( i = 0; i < 256; i++ )
    {
        perlinp[i] = perlinp[i+256] = rand() % 256;
    }

    return Qnil;
}

void Init_dxruby_Image()
{
    int i;

    /* Imageクラス定義 */
    cImage = rb_define_class_under( mDXRuby, "Image", rb_cObject );

    /* Imageクラスにクラスメソッド登録*/
    rb_define_singleton_method( cImage, "load", Image_load, -1 );
    rb_define_singleton_method( cImage, "load_to_array", Image_loadToArray, -1 );
    rb_define_singleton_method( cImage, "loadToArray", Image_loadToArray, -1 );
    rb_define_singleton_method( cImage, "load_tiles", Image_loadToArray, -1 );
    rb_define_singleton_method( cImage, "loadTiles", Image_loadToArray, -1 );
    rb_define_singleton_method( cImage, "create_from_array", Image_createFromArray, 3 );
    rb_define_singleton_method( cImage, "createFromArray", Image_createFromArray, 3 );
    rb_define_singleton_method( cImage, "load_from_file_in_memory", Image_loadFromFileInMemory, 1 );
    rb_define_singleton_method( cImage, "loadFromFileInMemory", Image_loadFromFileInMemory, 1 );
    rb_define_singleton_method( cImage, "load_from_memory", Image_loadFromFileInMemory, 1 );
    rb_define_singleton_method( cImage, "loadFromMemory", Image_loadFromFileInMemory, 1 );
    rb_define_singleton_method( cImage, "perlin_noise", Image_perlin_noise, -1);
    rb_define_singleton_method( cImage, "octave_perlin_noise", Image_octave_perlin_noise, -1);
    rb_define_singleton_method( cImage, "custom_perlin_noise", Image_custom_perlin_noise, -1);
    rb_define_singleton_method( cImage, "perlin_seed", Image_perlin_seed, 1);

    /* Imageクラスにメソッド登録*/
    rb_define_private_method( cImage, "initialize", Image_initialize, -1 );
    rb_define_method( cImage, "dispose"   , Image_dispose   , 0 );
    rb_define_method( cImage, "disposed?" , Image_check_disposed, 0 );
    rb_define_method( cImage, "delayed_dispose"   , Image_delayed_dispose   , 0 );
    rb_define_method( cImage, "width"     , Image_getWidth  , 0 );
    rb_define_method( cImage, "height"    , Image_getHeight , 0 );
//    rb_define_method( cImage, "width="    , Image_setWidth  , 1 );
//    rb_define_method( cImage, "height="   , Image_setHeight , 1 );
    rb_define_method( cImage, "[]="       , Image_setPixel  , 3 );
    rb_define_method( cImage, "[]"        , Image_getPixel  , 2 );
    rb_define_method( cImage, "box"       , Image_box       , 5 );
    rb_define_method( cImage, "box_fill"  , Image_boxFill   , 5 );
    rb_define_method( cImage, "boxFill"   , Image_boxFill   , 5 );
    rb_define_method( cImage, "fill"      , Image_fill      , 1 );
    rb_define_method( cImage, "clear"     , Image_clear     , 0 );
    rb_define_method( cImage, "line"      , Image_line      , 5 );
    rb_define_method( cImage, "triangle"  , Image_triangle  , 7 );
    rb_define_method( cImage, "triangle_fill", Image_triangle_fill, 7 );
    rb_define_method( cImage, "triangleFill" , Image_triangle_fill, 7 );
    rb_define_method( cImage, "circle"    , Image_circle    , 4 );
    rb_define_method( cImage, "circle_fill", Image_circleFill, 4 );
    rb_define_method( cImage, "circleFill", Image_circleFill, 4 );
    rb_define_method( cImage, "compare"   , Image_compare   , 3 );
    rb_define_method( cImage, "copy_rect" , Image_copyRect  , -1 );
    rb_define_method( cImage, "copyRect"  , Image_copyRect  , -1 );
    rb_define_method( cImage, "draw"      , Image_draw      , -1 );
    rb_define_method( cImage, "draw_font" , Image_drawFont  , -1 );
    rb_define_method( cImage, "drawFont"  , Image_drawFont  , -1 );
    rb_define_method( cImage, "save"      , Image_save      , -1 );
    rb_define_method( cImage, "slice"     , Image_slice_instance, -1);
    rb_define_method( cImage, "flush"     , Image_flush, 1);
    rb_define_method( cImage, "set_color_key", Image_setColorKey , 1 );
    rb_define_method( cImage, "setColorKey", Image_setColorKey , 1 );
    rb_define_method( cImage, "slice_to_array", Image_sliceToArray , -1 );
    rb_define_method( cImage, "sliceToArray", Image_sliceToArray , -1 );
    rb_define_method( cImage, "slice_tiles", Image_sliceToArray , -1 );
    rb_define_method( cImage, "sliceTiles", Image_sliceToArray , -1 );
    rb_define_method( cImage, "initialize_copy", Image_initialize_copy , 1 );
    rb_define_method( cImage, "draw_font_ex" , Image_drawFontEx  , -1 );
    rb_define_method( cImage, "drawFontEx"   , Image_drawFontEx  , -1 );
    rb_define_method( cImage, "effect_image_font", Image_effect, -1);
    rb_define_method( cImage, "effectImageFont", Image_effect, -1);
    rb_define_method( cImage, "change_hls", Image_change_hls, -1);
    rb_define_method( cImage, "changeHLS", Image_change_hls, -1);

    /* Imageオブジェクトを生成した時にinitializeの前に呼ばれるメモリ割り当て関数登録 */
    rb_define_alloc_func( cImage, Image_allocate );

    /* PerlinNoise初期化 */
    perlinp = malloc( 512 * sizeof(int) );

    for( i = 0; i < 256; i++ )
    {
        perlinp[i] = perlinp[i + 256] = permutation[i];
    }
}

void finalize_dxruby_Image()
{
    free( perlinp );
}


