#define WINVER 0x0500                                  /* バージョン定義 Windows2000以上 */
#define _WIN32_WINNT WINVER

#include "ruby.h"
#ifndef RUBY_ST_H
#include "st.h"
#endif

#define DXRUBY_EXTERN 1
#include "dxruby.h"
#include "font.h"

static VALUE cFont;         /* フォントクラス       */
static VALUE cFontInfo;     /* フォント情報クラス   */
static VALUE symbol_italic  = Qundef;
static VALUE symbol_weight  = Qundef;
static VALUE symbol_auto_fitting = Qundef;

VALUE hash_lookup(VALUE hash, VALUE key);
static VALUE Font_enum( VALUE klass );

/*********************************************************************
 * Fontクラス
 *
 * D3DXFontインターフェースを使用してフォントを描画する。
 * スプライト描画に混ぜ込むことでとりあえず実装。
 *********************************************************************/
/*--------------------------------------------------------------------
   参照されなくなったときにGCから呼ばれる関数
 ---------------------------------------------------------------------*/
static void Font_free( struct DXRubyFont *font )
{
    RELEASE( font->pD3DXFont );
    DeleteObject( font->hFont );
}
void Font_release( struct DXRubyFont *font )
{
    /* フォントオブジェクトの開放 */
    if( font->pD3DXFont )
    {
        Font_free( font );
    }
    free( font );
    font = NULL;

    g_iRefAll--;
    if( g_iRefAll == 0 )
    {
        CoUninitialize();
    }
}

/*--------------------------------------------------------------------
   Fontクラスのmark
 ---------------------------------------------------------------------*/
static void Font_mark( struct DXRubyFont *font )
{
    rb_gc_mark( font->vfontname );
    rb_gc_mark( font->vitalic );
    rb_gc_mark( font->vweight );
    rb_gc_mark( font->vauto_fitting );
    font->vglyph_naa = Qnil;
    font->vglyph_aa = Qnil;
}

#ifdef DXRUBY_USE_TYPEDDATA
const rb_data_type_t Font_data_type = {
    "Font",
    {
    Font_mark,
    Font_release,
    0,
    },
    NULL, NULL
};
#endif

/*--------------------------------------------------------------------
   Fontクラスのdispose。
 ---------------------------------------------------------------------*/
static VALUE Font_dispose( VALUE self )
{
    struct DXRubyFont *font = DXRUBY_GET_STRUCT( Font, self );
    DXRUBY_CHECK_DISPOSE( font, pD3DXFont );
    Font_free( font );
    return Qnil;
}

/*--------------------------------------------------------------------
   Fontクラスのdisposed?。
 ---------------------------------------------------------------------*/
static VALUE Font_check_disposed( VALUE self )
{
    if( DXRUBY_GET_STRUCT( Font, self )->pD3DXFont == NULL )
    {
        return Qtrue;
    }

    return Qfalse;
}

/*--------------------------------------------------------------------
   Fontクラスのallocate。メモリを確保する為にinitialize前に呼ばれる。
 ---------------------------------------------------------------------*/
static VALUE Font_allocate( VALUE klass )
{
    VALUE obj;
    struct DXRubyFont *font;

    /* DXRubyFontのメモリ取得＆Fontオブジェクト生成 */
    font = malloc( sizeof(struct DXRubyFont) );
    if( font == NULL ) rb_raise( eDXRubyError, "out of memory - Font_allocate" );
#ifdef DXRUBY_USE_TYPEDDATA
    obj = TypedData_Wrap_Struct( klass, &Font_data_type, font );
#else
    obj = Data_Wrap_Struct(klass, Font_mark, Font_release, font);
#endif

    font->pD3DXFont = NULL;
    font->hFont = NULL;
    font->size = 0;
    font->vfontname = Qnil;
    font->vitalic = Qnil;
    font->vweight = Qnil;
    font->vglyph_aa = Qnil;
    font->vglyph_naa = Qnil;
    font->vauto_fitting = Qnil;
    return obj;
}


/*--------------------------------------------------------------------
   FontクラスのInitialize
 ---------------------------------------------------------------------*/
static VALUE Font_initialize( int argc, VALUE *argv, VALUE obj )
{
    struct DXRubyFont *font;
    HRESULT hr;
    D3DXFONT_DESC desc;
    VALUE size, vfontname, voption, hash, vweight, vitalic, vauto_fitting;
    LOGFONT logfont;
    int weight, italic, auto_fitting;

    g_iRefAll++;

    rb_scan_args( argc, argv, "12", &size, &vfontname, &hash );

    if( hash == Qnil )
    {
        voption = rb_hash_new();
    }
    else
    {
        Check_Type( hash, T_HASH );
        voption = hash;
    }

    vweight = hash_lookup( voption, symbol_weight );
    vitalic = hash_lookup( voption, symbol_italic );
    vauto_fitting = hash_lookup( voption, symbol_auto_fitting );

    if( vweight == Qnil || vweight == Qfalse )
    {
        weight = 400;
    }
    else if( vweight == Qtrue )
    {
        weight = 1000;
    }
    else
    {
        weight = NUM2INT( vweight );
    }

    if( vitalic == Qnil || vitalic == Qfalse )
    {
        italic = FALSE;
    }
    else
    {
        italic = TRUE;
    }

    if( vauto_fitting == Qnil || vauto_fitting == Qfalse )
    {
        auto_fitting = 1;
    }
    else
    {
        auto_fitting = -1;
    }

    desc.Height          = NUM2INT( size ) * auto_fitting;
    desc.Width           = 0;
    desc.Weight          = weight;
    desc.MipLevels       = 0;
    desc.Italic          = italic;
    desc.CharSet         = DEFAULT_CHARSET;
    desc.OutputPrecision = 0;
    desc.Quality         = DEFAULT_QUALITY;
    desc.PitchAndFamily  = DEFAULT_PITCH || FF_DONTCARE;

    ZeroMemory( &logfont, sizeof(logfont) );
    logfont.lfHeight          = NUM2INT( size ) * auto_fitting;
    logfont.lfWidth           = 0;
    logfont.lfWeight          = weight;
    logfont.lfItalic          = italic;
    logfont.lfCharSet         = DEFAULT_CHARSET;
    logfont.lfQuality         = ANTIALIASED_QUALITY;

    if( vfontname == Qnil )
    {
        lstrcpy(desc.FaceName, "ＭＳ Ｐゴシック");
        lstrcpy(logfont.lfFaceName, "ＭＳ Ｐゴシック");
    }
    else
    {
        VALUE vsjisstr;
        Check_Type(vfontname, T_STRING);

        if( rb_enc_get_index( vfontname ) != 0 )
        {
            vsjisstr = rb_str_export_to_enc( vfontname, g_enc_sys );
        }
        else
        {
            vsjisstr = vfontname;
        }

        lstrcpy(desc.FaceName, RSTRING_PTR( vsjisstr ));
        lstrcpy(logfont.lfFaceName, RSTRING_PTR( vsjisstr ));
    }

    /* フォントオブジェクト取り出し */
    font = DXRUBY_GET_STRUCT( Font, obj );
    if( font->pD3DXFont )
    {
        Font_free( font );
        font->vglyph_aa = Qnil;
        font->vglyph_naa = Qnil;
        g_iRefAll--;
    }

    hr = D3DXCreateFontIndirect( g_pD3DDevice, &desc, &font->pD3DXFont );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Create font error - D3DXCreateFontIndirect" );
    }

    font->hFont = CreateFontIndirect( &logfont );

    if( font->hFont == NULL )
    {
        rb_raise( eDXRubyError, "Create font error - CreateFontIndirect" );
    }

    font->size = NUM2INT( size );
    OBJ_WRITE( obj, &font->vitalic, vitalic );
    OBJ_WRITE( obj, &font->vweight, vweight );
    OBJ_WRITE( obj, &font->vauto_fitting, vauto_fitting );
    OBJ_WRITE( obj, &font->vfontname, vfontname == Qnil ? Qnil : rb_str_new_shared( vfontname ) );

    return obj;
}


/*--------------------------------------------------------------------
   Fontクラスのinstall
 ---------------------------------------------------------------------*/
static VALUE Font_install( VALUE klass, VALUE vstr )
{
    int result;
    VALUE vsjisstr, vary1, vary2;

    Check_Type( vstr, T_STRING );

    if( rb_enc_get_index( vstr ) != 0 )
    {
        vsjisstr = rb_str_export_to_enc( vstr, g_enc_sys );
    }
    else
    {
        vsjisstr = vstr;
    }

    vary1 = Font_enum( klass );
    result = AddFontResourceEx( RSTRING_PTR( vsjisstr ), 0x10, 0 );
    if( result == 0 )
    {
        rb_raise( eDXRubyError, "Font install error - Font_install" );
    }
    vary2 = Font_enum( klass );

    return rb_funcall( vary2, rb_intern( "-" ), 1, vary1 );
}


/*--------------------------------------------------------------------
   フォントの幅を取得する
 ---------------------------------------------------------------------*/
VALUE Font_getWidth( VALUE obj, VALUE vstr )
{
    HDC hDC;
    struct DXRubyFont *font;
    RECT rc = {0};

    Check_Type( vstr, T_STRING );

    /* フォントオブジェクト取り出し */
    font = DXRUBY_GET_STRUCT( Font, obj );
    DXRUBY_CHECK_DISPOSE( font, pD3DXFont );

    hDC = GetDC( g_hWnd );
    if( hDC == NULL )
    {
        rb_raise( eDXRubyError, "get DC failed - GetDC" );
    }

    SelectObject( hDC, font->hFont );

    if( rb_enc_get_index( vstr ) != 0 )
    {
        VALUE vutf16str = rb_str_export_to_enc( vstr, g_enc_utf16 );
        DrawTextW( hDC, (LPWSTR)RSTRING_PTR( vutf16str ), RSTRING_LEN( vutf16str ) / 2, &rc, DT_LEFT | DT_NOCLIP | DT_NOPREFIX | DT_CALCRECT | DT_SINGLELINE);
    }
    else
    {
        DrawText( hDC, RSTRING_PTR( vstr ), -1, &rc, DT_LEFT | DT_NOCLIP | DT_NOPREFIX | DT_CALCRECT | DT_SINGLELINE);
    }

    ReleaseDC( g_hWnd, hDC );

    return INT2FIX( rc.right );
}


/*--------------------------------------------------------------------
   フォントのサイズを取得する
 ---------------------------------------------------------------------*/
VALUE Font_getSize( VALUE obj )
{
    struct DXRubyFont *font;

    /* フォントオブジェクト取り出し */
    font = DXRUBY_GET_STRUCT( Font, obj );
    DXRUBY_CHECK_DISPOSE( font, pD3DXFont );

    return INT2FIX( font->size );
}


/*--------------------------------------------------------------------
   生成時に指定したフォント名称を取得する
 ---------------------------------------------------------------------*/
static VALUE Font_getFontname( VALUE obj )
{
    struct DXRubyFont *font;

    /* フォントオブジェクト取り出し */
    font = DXRUBY_GET_STRUCT( Font, obj );
    DXRUBY_CHECK_DISPOSE( font, pD3DXFont );

    return font->vfontname;
}


/*--------------------------------------------------------------------
   実際に生成されたフォント名称を取得する
 ---------------------------------------------------------------------*/
static VALUE Font_getName( VALUE obj )
{
    struct DXRubyFont *font;
    int hr;
    HDC hDC;
    char buf[1024];

    /* フォントオブジェクト取り出し */
    font = DXRUBY_GET_STRUCT( Font, obj );
    DXRUBY_CHECK_DISPOSE( font, pD3DXFont );

    hDC = GetDC( g_hWnd );
    if( hDC == NULL )
    {
        rb_raise( eDXRubyError, "get DC failed - GetDC" );
    }

    SelectObject( hDC, font->hFont );
    hr = GetTextFace( hDC, 1024, buf );
    ReleaseDC( g_hWnd, hDC );

    if( FAILED( hr ) )
    {
        return rb_str_new( "\0", 1 );
    }

    return rb_enc_associate( rb_str_new2( buf ), g_enc_sys );
}


/*--------------------------------------------------------------------
   イタリックフラグを取得する
 ---------------------------------------------------------------------*/
static VALUE Font_getItalic( VALUE obj )
{
    struct DXRubyFont *font;

    /* フォントオブジェクト取り出し */
    font = DXRUBY_GET_STRUCT( Font, obj );
    DXRUBY_CHECK_DISPOSE( font, pD3DXFont );

    return font->vitalic;
}


/*--------------------------------------------------------------------
   フォントの太さを取得する
 ---------------------------------------------------------------------*/
static VALUE Font_getWeight( VALUE obj )
{
    struct DXRubyFont *font;

    /* フォントオブジェクト取り出し */
    font = DXRUBY_GET_STRUCT( Font, obj );
    DXRUBY_CHECK_DISPOSE( font, pD3DXFont );

    return font->vweight;
}

/*--------------------------------------------------------------------
   auto_fittingフラグを取得する
 ---------------------------------------------------------------------*/
static VALUE Font_getAutoFitting( VALUE obj )
{
    struct DXRubyFont *font;

    /* フォントオブジェクト取り出し */
    font = DXRUBY_GET_STRUCT( Font, obj );
    DXRUBY_CHECK_DISPOSE( font, pD3DXFont );

    return font->vauto_fitting;
}

/*--------------------------------------------------------------------
   グリフ情報を取得する
 ---------------------------------------------------------------------*/
char *Font_getGlyph( VALUE self, UINT widechr, HDC hDC, GLYPHMETRICS *gm, VALUE vaa_flag )
{
    struct DXRubyFont *font = DXRUBY_GET_STRUCT( Font, self );
    MAT2 mat2 = {{0,1},{0,0},{0,0},{0,1}};
    VALUE vstr;
    VALUE temp_naa = font->vglyph_naa;
    VALUE temp_aa = font->vglyph_aa;

    if( temp_naa == Qnil )
    {
        temp_naa = rb_hash_new();
    }
    if( temp_aa == Qnil )
    {
        temp_aa = rb_hash_new();
    }

    if( vaa_flag == Qnil )
    {
        vaa_flag = Qtrue;
    }

    if( RTEST(vaa_flag) )
    { /* AAあり */
        int bufsize;
        char *buf;
        vstr = hash_lookup( temp_aa, INT2NUM( widechr ) );

        bufsize = GetGlyphOutlineW( hDC, widechr, GGO_GRAY8_BITMAP,
                                        gm, 0, NULL, &mat2 );

        if( vstr == Qnil )
        {
            buf = alloca( bufsize );
            GetGlyphOutlineW( hDC, widechr, GGO_GRAY8_BITMAP, 
                              gm, bufsize, (LPVOID)buf, &mat2 );

            vstr = rb_str_new( buf, bufsize );
            rb_hash_aset( temp_aa, INT2NUM( widechr ), vstr );
        }
    }
    else
    { /* AAなし。モノクロデータをGRAY8形式に変換してキャッシュする */
        vstr = hash_lookup( temp_naa, INT2NUM( widechr ) );

        if( vstr == Qnil )
        {
            int bufsize_mono, bufsize_gray8;
            char *buf_mono, *buf_gray8;
            int x, y;
            int pitch_mono, pitch_gray8;

            /* モノクロデータ取得 */
            bufsize_mono  = GetGlyphOutlineW( hDC, widechr, GGO_BITMAP,
                                              gm, 0, NULL, &mat2 );
            buf_mono = alloca( bufsize_mono );
            GetGlyphOutlineW( hDC, widechr, GGO_BITMAP, 
                              gm, bufsize_mono, (LPVOID)buf_mono, &mat2 );

            bufsize_gray8 = ((gm->gmBlackBoxX + 3) & 0xfffc) * gm->gmBlackBoxY;
            buf_gray8 = alloca( bufsize_gray8 );

            /* 変換処理 */
            pitch_mono = ((gm->gmBlackBoxX + 31) / 32) * 4;
            pitch_gray8 = ((gm->gmBlackBoxX + 3) & 0xfffc);
            for( y = 0; y < gm->gmBlackBoxY; y++ )
            {
                for( x = 0; x < gm->gmBlackBoxX; x++ )
                {
                    if( *(buf_mono + x/8 + y * pitch_mono) & (1 << (7-(x%8))) )
                    {
                        *(buf_gray8 + x + y * pitch_gray8) = 64;
                    }
                    else
                    {
                        *(buf_gray8 + x + y * pitch_gray8) = 0;
                    }
                }
            }

            vstr = rb_str_new( buf_gray8, bufsize_gray8 );
            rb_hash_aset( temp_naa, INT2NUM( widechr ), vstr );
        }
        else
        {
            int bufsize;
            bufsize = GetGlyphOutlineW( hDC, widechr, GGO_BITMAP,
                                        gm, 0, NULL, &mat2 );
        }
    }

    OBJ_WRITE( self, &font->vglyph_naa, temp_naa );
    OBJ_WRITE( self, &font->vglyph_naa, temp_aa );
    return RSTRING_PTR( vstr );
}

void Font_getInfo_internal( VALUE vstr, struct DXRubyFont *font, int *intBlackBoxX, int *intBlackBoxY, int *intCellIncX, int *intPtGlyphOriginX, int *intPtGlyphOriginY, int *intTmAscent, int *intTmDescent )
{
    MAT2 mat2 = {{0,1},{0,0},{0,0},{0,1}};
    int bufsize;
    char *buf;
    TEXTMETRIC tm;
    GLYPHMETRICS gm;
    HDC hDC;
    WCHAR widechar;
    VALUE vwidestr;

    Check_Type( vstr, T_STRING );

    if( RSTRING_LEN( vstr ) == 0 )
    {
        rb_raise( eDXRubyError, "String is empty - info" );
    }

    /* 描画文字のUTF16LE化 */
    if( rb_enc_get_index( vstr ) != 0 )
    {
        vwidestr = rb_str_export_to_enc( vstr, g_enc_utf16 );
        widechar = *(WCHAR*)RSTRING_PTR( vwidestr );
    }
    else
    {
        MultiByteToWideChar( CP_ACP, 0, RSTRING_PTR( vstr ), 1, (LPWSTR)&widechar, 2 );
    }

    hDC = GetDC( g_hWnd );
    SelectObject( hDC, font->hFont );
    GetTextMetrics( hDC, &tm );

    bufsize = GetGlyphOutlineW( hDC, widechar, GGO_GRAY8_BITMAP,
                                    &gm, 0, NULL, &mat2 );
    ReleaseDC( g_hWnd, hDC );

    *intBlackBoxX = gm.gmBlackBoxX;
    *intBlackBoxY = gm.gmBlackBoxY;
    *intCellIncX = gm.gmCellIncX;
    *intPtGlyphOriginX = gm.gmptGlyphOrigin.x;
    *intPtGlyphOriginY = gm.gmptGlyphOrigin.y;
    *intTmAscent = tm.tmAscent;
    *intTmDescent = tm.tmDescent;
}

static VALUE Font_getInfo( VALUE self, VALUE vstr )
{
    struct DXRubyFont *font = DXRUBY_GET_STRUCT( Font, self );
    int intBlackBoxX, intBlackBoxY, intCellIncX, intPtGlyphOriginX, intPtGlyphOriginY, intTmAscent, intTmDescent;
    VALUE ary[7];

    Font_getInfo_internal( vstr, font, &intBlackBoxX, &intBlackBoxY, &intCellIncX, &intPtGlyphOriginX, &intPtGlyphOriginY, &intTmAscent, &intTmDescent );

    ary[0] = INT2NUM(intBlackBoxX);
    ary[1] = INT2NUM(intBlackBoxY);
    ary[2] = INT2NUM(intCellIncX);
    ary[3] = INT2NUM(intPtGlyphOriginX);
    ary[4] = INT2NUM(intPtGlyphOriginY);
    ary[5] = INT2NUM(intTmAscent);
    ary[6] = INT2NUM(intTmDescent);

    return rb_class_new_instance(7, ary, cFontInfo);
}


static VALUE Font_get_default( VALUE klass )
{
    return rb_ivar_get( klass, rb_intern("default") );
}

static VALUE Font_set_default( VALUE klass, VALUE vfont )
{
    rb_ivar_set( cFont, rb_intern("default"), vfont );

    return vfont;
}


int CALLBACK EnumFontsProc(
  CONST LOGFONT *lplf,     // 論理フォントデータへのポインタ
  CONST TEXTMETRIC *lptm,  // 物理フォントデータへのポインタ
  DWORD dwType,            // フォントタイプ
  LPARAM lpData            // アプリケーション定義のデータへのポインタ
)
{
    VALUE vstr = rb_str_new2( lplf->lfFaceName );
    rb_enc_associate( vstr, g_enc_sys );
    rb_ary_push( (VALUE)lpData, vstr );
    return 1;
}

static VALUE Font_enum( VALUE klass )
{
    HDC hdc = GetDC( g_hWnd );
    VALUE vary = rb_ary_new();
    EnumFonts( hdc , NULL ,EnumFontsProc , (LPARAM)vary );
    ReleaseDC( g_hWnd, hdc );
    return vary;
}


void Init_dxruby_Font()
{
    VALUE vdefaultfont, vsize;

    /* Fontクラス登録 */
    cFont = rb_define_class_under( mDXRuby, "Font", rb_cObject );

    /* Fontクラスにクラスメソッド登録*/
    rb_define_singleton_method( cFont, "install", Font_install, 1 );
    rb_define_singleton_method( cFont, "default", Font_get_default, 0 );
    rb_define_singleton_method( cFont, "default=", Font_set_default, 1 );

    /* Fontクラスにメソッド登録*/
    rb_define_private_method( cFont, "initialize", Font_initialize, -1 );
    rb_define_method( cFont, "dispose"   , Font_dispose   , 0 );
    rb_define_method( cFont, "disposed?" , Font_check_disposed, 0 );
    rb_define_method( cFont, "get_width" , Font_getWidth  , 1 );
    rb_define_method( cFont, "getWidth"  , Font_getWidth  , 1 );
    rb_define_method( cFont, "fontname", Font_getFontname  , 0 );
    rb_define_method( cFont, "name", Font_getName  , 0 );
    rb_define_method( cFont, "italic", Font_getItalic  , 0 );
    rb_define_method( cFont, "weight" , Font_getWeight  , 0 );
    rb_define_method( cFont, "auto_fitting" , Font_getAutoFitting , 0 );
    rb_define_method( cFont, "size"      , Font_getSize  , 0 );
    rb_define_method( cFont, "info"      , Font_getInfo  , 1 );

    /* Fontオブジェクトを生成した時にinitializeの前に呼ばれるメモリ割り当て関数登録 */
    rb_define_alloc_func( cFont, Font_allocate );

    symbol_italic         = ID2SYM(rb_intern("italic"));
    symbol_weight         = ID2SYM(rb_intern("weight"));
    symbol_auto_fitting   = ID2SYM(rb_intern("auto_fitting"));

    cFontInfo = rb_struct_define( NULL, "gm_blackbox_x", "gm_blackbox_y", "gm_cellinc_x", "gmpt_glyphorigin_x", "gmpt_glyphorigin_y", "tm_ascent", "tm_descent", 0 );
    rb_define_const( cFont, "FontInfo", cFontInfo );

    vdefaultfont = Font_allocate( cFont );
    vsize = INT2FIX(24);
    Font_initialize( 1, &vsize, vdefaultfont );
    rb_ivar_set( cFont, rb_intern("default"), vdefaultfont );
}

