/* フォントデータ */
struct DXRubyFont {
    LPD3DXFONT pD3DXFont;       /* フォントオブジェクト   */
    HFONT hFont;                /* Image描画に使うフォント  */
    int size;                   /* フォントサイズ */
    VALUE vfontname;            /* フォント名称 */
    VALUE vweight;              /* 太さ */
    VALUE vitalic;              /* イタリックフラグ */
    VALUE vglyph_naa;           /* グリフキャッシュ(AAなし) */
    VALUE vglyph_aa;            /* グリフキャッシュ(AAあり) */
    VALUE vauto_fitting;        /* 実際の描画文字が指定サイズになるよう拡大する */
};

void Init_dxruby_Font( void );
void Font_release( struct DXRubyFont* font );
VALUE Font_getWidth( VALUE obj, VALUE vstr );
VALUE Font_getSize( VALUE obj );
char *Font_getGlyph( VALUE obj, UINT widechr, HDC hDC, GLYPHMETRICS *gm, VALUE vaa_flag );
void Font_getInfo_internal( VALUE vstr, struct DXRubyFont *font, int *intBlackBoxX, int *intBlackBoxY, int *intCellIncX, int *intPtGlyphOriginX, int *intPtGlyphOriginY, int *intTmAscent, int *intTmDescent );

