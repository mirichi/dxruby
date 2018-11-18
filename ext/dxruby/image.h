/* Imageオブジェクトの中身 */
struct DXRubyImage {
    struct DXRubyTexture *texture;
    int x;     /* x始点位置      */
    int y;     /* y始点位置      */
    int width; /* イメージの幅   */
    int height;/* イメージの高さ */
//    int lockcount;    /* ロックカウント */
};

void Init_dxruby_Image( void );
void finalize_dxruby_Image( void );

void Image_release( struct DXRubyImage* image );
VALUE Image_drawFontEx( int argc, VALUE *argv, VALUE obj );
VALUE Image_dispose( VALUE self );
VALUE Image_allocate( VALUE klass );
VALUE Image_initialize( int argc, VALUE *argv, VALUE obj );
int array2color( VALUE color );
VALUE Image_save( int argc, VALUE *argv, VALUE self );
VALUE Image_sliceToArray( int argc, VALUE *argv, VALUE self );

