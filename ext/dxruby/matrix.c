#define WINVER 0x0500                                  /* バージョン定義 Windows2000以上 */
#define _WIN32_WINNT WINVER

#include "ruby.h"
#ifndef RUBY_ST_H
#include "st.h"
#endif

#define DXRUBY_EXTERN 1
#include "dxruby.h"
#include "matrix.h"

VALUE cMatrix;         /* 行列クラス       */
VALUE cVector;         /* ベクトルクラス   */

#ifdef DXRUBY_USE_TYPEDDATA
const rb_data_type_t Matrix_data_type;
const rb_data_type_t Vector_data_type;
#endif

static float pi = 3.141592653589793115997963468544185161590576171875f;

/*********************************************************************
 * Matrixクラス
 *
 * 行列を表現する。
 *********************************************************************/

/*--------------------------------------------------------------------
   参照されなくなったときにGCから呼ばれる関数
 ---------------------------------------------------------------------*/
void Matrix_release( struct DXRubyMatrix* mat )
{
    free( mat );
}

#ifdef DXRUBY_USE_TYPEDDATA
const rb_data_type_t Matrix_data_type = {
    "Matrix",
    {
    0,
    Matrix_release,
    0,
    },
    NULL, NULL
};
#endif

/*--------------------------------------------------------------------
   Matrixクラスのallocate。メモリを確保する為にinitialize前に呼ばれる。
 ---------------------------------------------------------------------*/
static VALUE Matrix_allocate( VALUE klass )
{
    VALUE obj;
    struct DXRubyMatrix *mat;

    /* DXRubyMatrixのメモリ取得＆Matrixオブジェクト生成 */
    mat = malloc( sizeof( struct DXRubyMatrix ) );
    if( mat == NULL ) rb_raise( eDXRubyError, "メモリの取得に失敗しました - Matrix_allocate" );
#ifdef DXRUBY_USE_TYPEDDATA
    obj = TypedData_Wrap_Struct( klass, &Matrix_data_type, mat );
#else
    obj = Data_Wrap_Struct( klass, 0, Matrix_release, mat );
#endif
    mat->x = 0;
    mat->y = 0;
    ZeroMemory( mat->m, sizeof( float ) * 16 );

    return obj;
}

/*--------------------------------------------------------------------
   MatrixクラスのInitialize
 ---------------------------------------------------------------------*/
static VALUE Matrix_initialize( int argc, VALUE *argv, VALUE self )
{
    struct DXRubyMatrix *mat = DXRUBY_GET_STRUCT( Matrix, self );
    VALUE ary, *ary_p;
    int i, j, count;

    if( argc == 0 )
    {
        mat->m11 = 1;mat->m22 = 1;mat->m33 = 1;mat->m44 = 1;
        mat->x = mat->y = 4;
        return self;
    }

    if( argc == 1 )
    {
        ary = argv[0];
        Check_Type( ary, T_ARRAY );
        if( RARRAY_LEN( ary ) > 4 || RARRAY_LEN( ary ) < 1 ) rb_raise( eDXRubyError, "配列の数が正しくありません。 - Matrix_initialize");
        ary_p = RARRAY_PTR( ary );
        count = RARRAY_LEN( ary );
    }
    else
    {
        if( argc > 4 ) rb_raise( eDXRubyError, "引数の数が正しくありません。 - Matrix_initialize");
        ary_p = argv;
        count = argc;
    }

    mat->y = count;
    for( i = 0; i < count; i++ )
    {
        VALUE ary2 = ary_p[i];
        Check_Type( ary2, T_ARRAY );
        if( RARRAY_LEN( ary2 ) > 4 || RARRAY_LEN( ary2 ) < 1 || RARRAY_LEN( ary2 ) != mat->y ) rb_raise( eDXRubyError, "配列の数が正しくありません。 - Matrix_initialize");

        for( j = 0; j < RARRAY_LEN( ary2 ); j++ )
        {
            mat->m[i][j] = NUM2FLOAT( RARRAY_PTR( ary2 )[j] );
        }
    }
    mat->x = RARRAY_LEN( ary_p[0] );

    return self;
}

static VALUE Matrix_mul( VALUE self, VALUE varg )
{
    struct DXRubyMatrix *mat_d = DXRUBY_GET_STRUCT( Matrix, self );
    struct DXRubyMatrix *result;
    VALUE vresult;

    if( FIXNUM_P( varg ) || TYPE( varg ) == T_FLOAT || TYPE( varg ) == T_BIGNUM )
    {
        int i, j;

        vresult = Matrix_allocate( cMatrix );
        result = DXRUBY_GET_STRUCT( Matrix, vresult );
        result->x = mat_d->x;
        result->y = mat_d->y;
        for( i = 0; i < mat_d->y; i++ )
        {
            for( j = 0; j < mat_d->x; j++ )
            {
                result->m[i][j] = mat_d->m[i][j] * NUM2FLOAT( varg );
            }
        }
    }
    else
    {
        int i, j, k;
        struct DXRubyMatrix *mat_s;

        DXRUBY_CHECK_TYPE( Matrix, varg );
        mat_s = DXRUBY_GET_STRUCT( Matrix, varg );

        if( mat_d->x != mat_s->y || mat_d->y != mat_s->x ) rb_raise( eDXRubyError, "要素数が一致していません。 - Matrix_*");
        vresult = Matrix_allocate( cMatrix );
        result = DXRUBY_GET_STRUCT( Matrix, vresult );
        result->x = mat_s->x;
        result->y = mat_d->y;

        for( i = 0; i < mat_d->y; i++ )
        {
            for( j = 0; j < mat_s->x; j++ )
            {
                for( k = 0; k < mat_s->x; k++ )
                {
                    result->m[i][j] += mat_d->m[i][k] * mat_s->m[k][j];
                }
            }
        }
    }
    return vresult;
}


static VALUE Matrix_to_s( VALUE self )
{
    struct DXRubyMatrix *mat = DXRUBY_GET_STRUCT( Matrix, self );
    char buf[1024];
    char temp[256];
    int i;

    sprintf( buf, "size = %d,%d ", mat->x, mat->y );

    for( i = 0; i < mat->y; i++ )
    {
        switch( mat->x ) {
        case 1:
            sprintf( temp, "(%f)", mat->m[i][0] );
            strcat( buf, temp );
            break;
        case 2:
            sprintf( temp, "(%f, %f)", mat->m[i][0], mat->m[i][1] );
            strcat( buf, temp );
            break;
        case 3:
            sprintf( temp, "(%f, %f, %f)", mat->m[i][0], mat->m[i][1], mat->m[i][2] );
            strcat( buf, temp );
            break;
        default:
            sprintf( temp, "(%f, %f, %f, %f)", mat->m[i][0], mat->m[i][1], mat->m[i][2], mat->m[i][3] );
            strcat( buf, temp );
            break;
        }
    }

    return rb_str_new2( buf );
}

/* ビュー行列作成 */
static VALUE Matrix_look_at( VALUE klass, VALUE veye, VALUE vat, VALUE vup )
{
    struct DXRubyMatrix *result;
    D3DVECTOR eye, at, up;
    struct DXRubyVector *vec_eye, *vec_at, *vec_up;
    VALUE vresult;

    DXRUBY_CHECK_TYPE( Vector, veye );
    vec_eye =  DXRUBY_GET_STRUCT( Vector, veye );
    DXRUBY_CHECK_TYPE( Vector, vat );
    vec_at =  DXRUBY_GET_STRUCT( Vector, vat );
    DXRUBY_CHECK_TYPE( Vector, vup );
    vec_up =  DXRUBY_GET_STRUCT( Vector, vup );

    vresult = Matrix_allocate( cMatrix );
    result = DXRUBY_GET_STRUCT( Matrix, vresult );

//    memcpy( &matrix, mat->m, sizeof( float ) * 16 );
//    D3DXMatrixInverse( (D3DMATRIX *)result->m, 0, (D3DMATRIX *)mat->m);

    eye.x = vec_eye->v1;
    eye.y = vec_eye->v2;
    eye.z = vec_eye->v3;
    at.x = vec_at->v1;
    at.y = vec_at->v2;
    at.z = vec_at->v3;
    up.x = vec_up->v1;
    up.y = vec_up->v2;
    up.z = vec_up->v3;
    D3DXMatrixLookAtLH( (D3DMATRIX *)result->m, &eye, &at, &up );
    
    result->x = 4;
    result->y = 4;
    return vresult;
}


/* 射影変換行列作成 */
static VALUE Matrix_create_projection( VALUE klass, VALUE vwidth, VALUE vheight, VALUE vzn, VALUE vzf )
{
    struct DXRubyMatrix *result;
    VALUE vresult;
    vresult = Matrix_allocate( cMatrix );
    result = DXRUBY_GET_STRUCT( Matrix, vresult );
    result->x = result->y = 4;

    D3DXMatrixPerspectiveLH( (D3DMATRIX*)result->m, NUM2FLOAT( vwidth ), NUM2FLOAT( vheight ), NUM2FLOAT( vzn ), NUM2FLOAT( vzf ) );

    return vresult;
}


/* 射影変換行列作成(画角指定) */
static VALUE Matrix_create_projection_fov( VALUE klass, VALUE vfov, VALUE vaspect, VALUE vzn, VALUE vzf )
{
    struct DXRubyMatrix *result;
    D3DMATRIX matrix;
    VALUE vresult;
    float angle;
    vresult = Matrix_allocate( cMatrix );
    result = DXRUBY_GET_STRUCT( Matrix, vresult );
    result->x = result->y = 4;

    angle = pi / 180.0f * NUM2FLOAT( vfov );
    D3DXMatrixPerspectiveFovLH( (D3DMATRIX*)result->m, angle, NUM2FLOAT( vaspect ), NUM2FLOAT( vzn ), NUM2FLOAT( vzf ) );

    return vresult;
}


/* 正射影変換行列作成 */
static VALUE Matrix_create_projection_ortho( VALUE klass, VALUE vwidth, VALUE vheight, VALUE vzn, VALUE vzf )
{
    struct DXRubyMatrix *result;
    D3DMATRIX matrix;
    VALUE vresult;
    vresult = Matrix_allocate( cMatrix );
    result = DXRUBY_GET_STRUCT( Matrix, vresult );
    result->x = result->y = 4;

    D3DXMatrixOrthoLH( (D3DMATRIX*)result->m, NUM2FLOAT( vwidth ), NUM2FLOAT( vheight ), NUM2FLOAT( vzn ), NUM2FLOAT( vzf ) );

    return vresult;
}


/* 2D回転行列作成 */
static VALUE Matrix_create_rot( VALUE klass, VALUE vangle )
{
    struct DXRubyMatrix *result;
    VALUE vresult;
    float angle;

    vresult = Matrix_allocate( cMatrix );
    result = DXRUBY_GET_STRUCT( Matrix, vresult );

    result->x = 3;
    result->y = 3;
    angle = pi / 180.0f * NUM2FLOAT( vangle );
    result->m11 = cos( angle );
    result->m12 = sin( angle );
    result->m21 = -result->m12;
    result->m22 = result->m11;
    result->m33 = 1;

    return vresult;
}
/* x軸回転行列作成 */
static VALUE Matrix_create_rot_x( VALUE klass, VALUE vangle )
{
    struct DXRubyMatrix *result;
    VALUE vresult;
    float angle;

    vresult = Matrix_allocate( cMatrix );
    result = DXRUBY_GET_STRUCT( Matrix, vresult );

    result->x = 4;
    result->y = 4;
    angle = pi / 180.0f * NUM2FLOAT( vangle );
    result->m11 = 1;
    result->m22 = cos( angle );
    result->m23 = sin( angle );
    result->m32 = -result->m23;
    result->m33 = result->m22;
    result->m44 = 1;

    return vresult;
}
/* y軸回転行列作成 */
static VALUE Matrix_create_rot_y( VALUE klass, VALUE vangle )
{
    struct DXRubyMatrix *result;
    VALUE vresult;
    float angle;

    vresult = Matrix_allocate( cMatrix );
    result = DXRUBY_GET_STRUCT( Matrix, vresult );

    result->x = 4;
    result->y = 4;
    angle = pi / 180.0f * NUM2FLOAT( vangle );
    result->m11 = cos( angle );
    result->m13 = -sin( angle );
    result->m22 = 1;
    result->m31 = -result->m13;
    result->m33 = result->m11;
    result->m44 = 1;

    return vresult;
}
/* z軸回転行列作成 */
static VALUE Matrix_create_rot_z( VALUE klass, VALUE vangle )
{
    struct DXRubyMatrix *result;
    VALUE vresult;
    float angle;

    vresult = Matrix_allocate( cMatrix );
    result = DXRUBY_GET_STRUCT( Matrix, vresult );

    result->x = 4;
    result->y = 4;
    angle = pi / 180.0f * NUM2FLOAT( vangle );
    result->m11 = cos( angle );
    result->m12 = sin( angle );
    result->m21 = -result->m12;
    result->m22 = result->m11;
    result->m33 = 1;
    result->m44 = 1;

    return vresult;
}
/* 平行移動行列作成 */
static VALUE Matrix_create_trans( int argc, VALUE *argv, VALUE self )
{
    struct DXRubyMatrix *result;
    VALUE vresult;

    if( argc < 1 || argc > 3 ) rb_raise( eDXRubyError, "引数の数が正しくありません。 - Matrix_create_trans");

    vresult = Matrix_allocate( cMatrix );
    result = DXRUBY_GET_STRUCT( Matrix, vresult );

    if( argc == 1 )
    {
        result->x = 2;
        result->y = 2;
        result->m11 = 1;
        result->m21 = NUM2FLOAT( argv[0] );
        result->m22 = 1;
    }
    else if( argc == 2 )
    {
        result->x = 3;
        result->y = 3;
        result->m11 = 1;
        result->m22 = 1;
        result->m31 = NUM2FLOAT( argv[0] );
        result->m32 = NUM2FLOAT( argv[1] );
        result->m33 = 1;
    }
    else if( argc == 3)
    {
        result->x = 4;
        result->y = 4;
        result->m11 = 1;
        result->m22 = 1;
        result->m33 = 1;
        result->m41 = NUM2FLOAT( argv[0] );
        result->m42 = NUM2FLOAT( argv[1] );
        result->m43 = NUM2FLOAT( argv[2] );
        result->m44 = 1;
    }

    return vresult;
}
/* スケーリング行列作成 */
static VALUE Matrix_create_scale( int argc, VALUE *argv, VALUE self )
{
    struct DXRubyMatrix *result;
    VALUE vresult;

    if( argc < 1 || argc > 3 ) rb_raise( eDXRubyError, "引数の数が正しくありません。 - Matrix_create_scale");

    vresult = Matrix_allocate( cMatrix );
    result = DXRUBY_GET_STRUCT( Matrix, vresult );

    if( argc == 1 )
    {
        result->x = 2;
        result->y = 2;
        result->m11 = NUM2FLOAT( argv[0] );
        result->m22 = 1;
    }
    else if( argc == 2 )
    {
        result->x = 3;
        result->y = 3;
        result->m11 = NUM2FLOAT( argv[0] );
        result->m22 = NUM2FLOAT( argv[1] );
        result->m33 = 1;
    }
    else if( argc == 3)
    {
        result->x = 4;
        result->y = 4;
        result->m11 = NUM2FLOAT( argv[0] );
        result->m22 = NUM2FLOAT( argv[1] );
        result->m33 = NUM2FLOAT( argv[2] );
        result->m44 = 1;
    }

    return vresult;
}

/* 配列化 */
static VALUE Matrix_to_a( VALUE self )
{
    struct DXRubyMatrix *mat = DXRUBY_GET_STRUCT( Matrix, self );
    VALUE vresult;
    int i, j;

    vresult = rb_ary_new();
    for( i = 0; i < mat->y; i++ )
    {
        for( j = 0; j < mat->x; j++ )
        {
            rb_ary_push( vresult, rb_float_new( mat->m[i][j] ) );
        }
    }
    return vresult;
}

/* 逆行列作成 */
static VALUE Matrix_inverse( VALUE self )
{
    struct DXRubyMatrix *mat = DXRUBY_GET_STRUCT( Matrix, self );
    struct DXRubyMatrix *result;
    VALUE vresult;

    vresult = Matrix_allocate( cMatrix );
    result = DXRUBY_GET_STRUCT( Matrix, vresult );
    result->x = result->y = 4;
    D3DXMatrixInverse( (D3DMATRIX *)&result->m, 0, (D3DMATRIX *)&mat->m);

    return vresult;
}


/*********************************************************************
 * Vectorクラス
 *
 * ベクトルを表現する。
 *********************************************************************/

/*--------------------------------------------------------------------
   参照されなくなったときにGCから呼ばれる関数
 ---------------------------------------------------------------------*/
void Vector_release( struct DXRubyVector* vec )
{
    vec->v1 = 0;
    free( vec );
}

#ifdef DXRUBY_USE_TYPEDDATA
const rb_data_type_t Vector_data_type = {
    "Vector",
    {
    0,
    Vector_release,
    0,
    },
    NULL, NULL
};
#endif

/*--------------------------------------------------------------------
   Vectorクラスのallocate。メモリを確保する為にinitialize前に呼ばれる。
 ---------------------------------------------------------------------*/
VALUE Vector_allocate( VALUE klass )
{
    VALUE obj;
    struct DXRubyVector *vec;

    /* DXRubyVectorのメモリ取得＆Vectorオブジェクト生成 */
    vec = malloc( sizeof( struct DXRubyVector ) );
    if( vec == NULL ) rb_raise( eDXRubyError, "メモリの取得に失敗しました - Vector_allocate" );
#ifdef DXRUBY_USE_TYPEDDATA
    obj = TypedData_Wrap_Struct( klass, &Vector_data_type, vec );
#else
    obj = Data_Wrap_Struct( klass, 0, Vector_release, vec );
#endif
    vec->x = 0;
    vec->v1 = 0;
    vec->v2 = 0;
    vec->v3 = 0;
    vec->v4 = 0;

    return obj;
}

/*--------------------------------------------------------------------
   VectorクラスのInitialize
 ---------------------------------------------------------------------*/
static VALUE Vector_initialize( int argc, VALUE *argv, VALUE self )
{
    struct DXRubyVector *vec = DXRUBY_GET_STRUCT( Vector, self );
    VALUE *ary_p;
    int i, count;

    if( argc == 0 )
    {
        return self;
    }

    if( argc == 1 && TYPE( argv[0] ) == T_ARRAY )
    {
        if( RARRAY_LEN( argv[0] ) > 4 || RARRAY_LEN( argv[0] ) < 1 ) rb_raise( eDXRubyError, "配列の要素数が正しくありません。 - Vector_initialize");
        ary_p = RARRAY_PTR( argv[0] );
        count = RARRAY_LEN( argv[0] );
    }
    else
    {
        if( argc > 4 ) rb_raise( eDXRubyError, "引数の数が正しくありません。 - Vector_initialize");
        ary_p = argv;
        count = argc;
    }

    vec->x = count;
    for( i = 0; i < count; i++ )
    {
        vec->v[i] = NUM2FLOAT( ary_p[i] );
    }

    return self;
}

static VALUE Vector_mul( VALUE self, VALUE varg )
{
    struct DXRubyVector *vec = DXRUBY_GET_STRUCT( Vector, self );
    struct DXRubyVector *result;
    VALUE vresult;
    vresult = Vector_allocate( cVector );
    result = DXRUBY_GET_STRUCT( Vector, vresult );
    result->x = vec->x;

    if( FIXNUM_P( varg ) || TYPE( varg ) == T_FLOAT || TYPE( varg ) == T_BIGNUM )
    {
        int i;

        for( i = 0; i < vec->x; i++ )
        {
            result->v[i] = vec->v[i] * NUM2FLOAT( varg );
        }
    }
    else if( DXRUBY_CHECK( Vector, varg ) )
    {
        int i;
        struct DXRubyVector* vec_s = DXRUBY_GET_STRUCT( Vector, varg );

        for( i = 0; i < vec->x; i++ )
        {
            result->v[i] = vec->v[i];
        }

        for( i = 0; i < vec->x && i < vec_s->x; i++ )
        {
            result->v[i] = vec->v[i] * vec_s->v[i];
        }
    }
    else if( DXRUBY_CHECK( Matrix, varg ) )
    {
        struct DXRubyMatrix *mat;
        int i, j;
        float temp[4] = {1.0f,1.0f,1.0f,1.0f};
        DXRUBY_CHECK_TYPE( Matrix, varg );
        mat = DXRUBY_GET_STRUCT( Matrix, varg );

        if( vec->x != mat->y && vec->x != mat->y - 1 ) rb_raise( eDXRubyError, "要素数が一致していません。 - Vector_mul");
        for( i = 0; i < vec->x; i++ )
        {
            temp[i] = vec->v[i];
        }

        for( i = 0; i < mat->x; i++ )
        {
            for( j = 0; j < mat->y; j++)
            {
                result->v[i] += temp[j] * mat->m[j][i];
            }
        }

        for( i = result->x; i < 4; i++)
        {
            result->v[i] = 0.0f;
        }
    }
    else
    {
        rb_raise( eDXRubyError, "引数が異常です - Vector_mul");
    }

    return vresult;
}

static VALUE Vector_add( VALUE self, VALUE varg )
{
    struct DXRubyVector *vec_d = DXRUBY_GET_STRUCT( Vector, self );
    struct DXRubyVector *vec_s;
    struct DXRubyVector *result;
    VALUE vresult;
    int i;

    vresult = Vector_allocate( cVector );
    result = DXRUBY_GET_STRUCT( Vector, vresult );
    result->x = vec_d->x;

    if( FIXNUM_P( varg ) || TYPE( varg ) == T_FLOAT || TYPE( varg ) == T_BIGNUM )
    {
        for( i = 0; i < vec_d->x; i++ )
        {
            result->v[i] = vec_d->v[i] + NUM2FLOAT( varg );
        }
    }
    else
    {
        DXRUBY_CHECK_TYPE( Vector, varg );
        vec_s =  DXRUBY_GET_STRUCT( Vector, varg );

        for( i = 0; i < vec_d->x; i++ )
        {
            result->v[i] = vec_d->v[i];
        }

        for( i = 0; i < vec_d->x && i < vec_s->x; i++ )
        {
            result->v[i] = vec_d->v[i] + vec_s->v[i];
        }
    }

    return vresult;
}

static VALUE Vector_sub( VALUE self, VALUE varg )
{
    struct DXRubyVector *vec_d = DXRUBY_GET_STRUCT( Vector, self );
    struct DXRubyVector *vec_s;
    struct DXRubyVector *result;
    VALUE vresult;
    int i;

    vresult = Vector_allocate( cVector );
    result = DXRUBY_GET_STRUCT( Vector, vresult );
    result->x = vec_d->x;

    if( FIXNUM_P( varg ) || TYPE( varg ) == T_FLOAT || TYPE( varg ) == T_BIGNUM )
    {
        for( i = 0; i < vec_d->x; i++ )
        {
            result->v[i] = vec_d->v[i] - NUM2FLOAT( varg );
        }
    }
    else
    {
        DXRUBY_CHECK_TYPE( Vector, varg );
        vec_s =  DXRUBY_GET_STRUCT( Vector, varg );

        for( i = 0; i < vec_d->x; i++ )
        {
            result->v[i] = vec_d->v[i];
        }

        for( i = 0; i < vec_d->x && i < vec_s->x; i++ )
        {
            result->v[i] = vec_d->v[i] - vec_s->v[i];
        }
    }

    return vresult;
}

static VALUE Vector_div( VALUE self, VALUE varg )
{
    struct DXRubyVector *vec_d = DXRUBY_GET_STRUCT( Vector, self );
    struct DXRubyVector *vec_s;
    struct DXRubyVector *result;
    VALUE vresult;
    int i;

    vresult = Vector_allocate( cVector );
    result = DXRUBY_GET_STRUCT( Vector, vresult );
    result->x = vec_d->x;

    if( FIXNUM_P( varg ) || TYPE( varg ) == T_FLOAT || TYPE( varg ) == T_BIGNUM )
    {
        for( i = 0; i < vec_d->x; i++ )
        {
            result->v[i] = vec_d->v[i] / NUM2FLOAT( varg );
        }
    }
    else
    {
        DXRUBY_CHECK_TYPE( Vector, varg );
        vec_s =  DXRUBY_GET_STRUCT( Vector, varg );

        for( i = 0; i < vec_d->x; i++ )
        {
            result->v[i] = vec_d->v[i];
        }

        for( i = 0; i < vec_d->x && i < vec_s->x; i++ )
        {
            result->v[i] = vec_d->v[i] / vec_s->v[i];
        }
    }

    return vresult;
}

static VALUE Vector_minus( VALUE self )
{
    struct DXRubyVector *vec = DXRUBY_GET_STRUCT( Vector, self );
    struct DXRubyVector *result;
    VALUE vresult;
    int i;

    vresult = Vector_allocate( cVector );
    result = DXRUBY_GET_STRUCT( Vector, vresult );
    result->x = vec->x;

    for( i = 0; i < vec->x; i++ )
    {
        result->v[i] = -vec->v[i];
    }

    return vresult;
}

static VALUE Vector_normalize( VALUE self )
{
    struct DXRubyVector *vec = DXRUBY_GET_STRUCT( Vector, self );
    struct DXRubyVector *result;
    VALUE vresult;
    float magsq = vec->v1*vec->v1 + vec->v2*vec->v2 + vec->v3*vec->v3 + vec->v4*vec->v4;
    int i;

    vresult = Vector_allocate( cVector );
    result = DXRUBY_GET_STRUCT( Vector, vresult );
    result->x = vec->x;

    if( magsq > 0.0f )
    {
        float temp = 1.0f / sqrt( magsq );
        int i;
        for( i = 0; i < vec->x; i++ )
        {
            result->v[i] = temp * vec->v[i];
        }
    }

    return vresult;
}

static VALUE Vector_distance( VALUE klass, VALUE vvec1, VALUE vvec2 )
{
    struct DXRubyVector *vec1;
    struct DXRubyVector *vec2;
    float dx, dy, dz, dw;

    DXRUBY_CHECK_TYPE( Vector, vvec1 );
    DXRUBY_CHECK_TYPE( Vector, vvec2 );
    vec1 = DXRUBY_GET_STRUCT( Vector, vvec1 );
    vec2 = DXRUBY_GET_STRUCT( Vector, vvec2 );

    dx = vec1->v1 - vec2->v1;
    dy = vec1->v2 - vec2->v2;
    dz = vec1->v3 - vec2->v3;
    dw = vec1->v4 - vec2->v4;
    return rb_float_new( sqrt( dx*dx + dy*dy + dz*dz + dw*dw ) );
}

static VALUE Vector_cross_product( VALUE klass, VALUE vvec1, VALUE vvec2 )
{
    struct DXRubyVector *vec1;
    struct DXRubyVector *vec2;
    struct DXRubyVector *result;
    VALUE vresult;

    DXRUBY_CHECK_TYPE( Vector, vvec1 );
    DXRUBY_CHECK_TYPE( Vector, vvec2 );
    vec1 = DXRUBY_GET_STRUCT( Vector, vvec1 );
    vec2 = DXRUBY_GET_STRUCT( Vector, vvec2 );

    vresult = Vector_allocate( cVector );
    result = DXRUBY_GET_STRUCT( Vector, vresult );
    result->x = 3;

    result->v1 = vec1->v2*vec2->v3 - vec1->v3*vec2->v2;
    result->v2 = vec1->v3*vec2->v1 - vec1->v1*vec2->v3;
    result->v3 = vec1->v1*vec2->v2 - vec1->v2*vec2->v1;

    return vresult;
}

static VALUE Vector_dot_product( VALUE klass, VALUE vvec1, VALUE vvec2 )
{
    struct DXRubyVector *vec1;
    struct DXRubyVector *vec2;
    DXRUBY_CHECK_TYPE( Vector, vvec1 );
    DXRUBY_CHECK_TYPE( Vector, vvec2 );
    vec1 = DXRUBY_GET_STRUCT( Vector, vvec1 );
    vec2 = DXRUBY_GET_STRUCT( Vector, vvec2 );
    return rb_float_new( vec1->v1 * vec2->v1 + vec1->v2 * vec2->v2 + vec1->v3 * vec2->v3 + vec1->v4 * vec2->v4 );
}

static VALUE Vector_to_s( VALUE self )
{
    struct DXRubyVector *vec = DXRUBY_GET_STRUCT( Vector, self );
    char buf[1024];
    switch( vec->x ) {
    case 1:
        sprintf( buf, "size = %d (%f)", vec->x, vec->v1 );
        break;
    case 2:
        sprintf( buf, "size = %d (%f, %f)", vec->x, vec->v1, vec->v2 );
        break;
    case 3:
        sprintf( buf, "size = %d (%f, %f, %f)", vec->x, vec->v1, vec->v2, vec->v3 );
        break;
    default:
        sprintf( buf, "size = %d (%f, %f, %f, %f)", vec->x, vec->v1, vec->v2, vec->v3, vec->v4 );
        break;
    }
    return rb_str_new2( buf );
}

static VALUE Vector_get_x( VALUE self )
{
    struct DXRubyVector *vec = DXRUBY_GET_STRUCT( Vector, self );
    return rb_float_new(vec->v1);
}

static VALUE Vector_get_y( VALUE self )
{
    struct DXRubyVector *vec = DXRUBY_GET_STRUCT( Vector, self );
    return rb_float_new(vec->v2);
}

static VALUE Vector_get_z( VALUE self )
{
    struct DXRubyVector *vec = DXRUBY_GET_STRUCT( Vector, self );
    return rb_float_new(vec->v3);
}

static VALUE Vector_get_w( VALUE self )
{
    struct DXRubyVector *vec = DXRUBY_GET_STRUCT( Vector, self );
    return rb_float_new(vec->v4);
}

static VALUE Vector_get_size( VALUE self )
{
    struct DXRubyVector *vec = DXRUBY_GET_STRUCT( Vector, self );
    return rb_float_new(vec->x);
}

static VALUE Vector_get_xy( VALUE self )
{
    struct DXRubyVector *vec = DXRUBY_GET_STRUCT( Vector, self );
    struct DXRubyVector *result;
    VALUE vresult;

    vresult = Vector_allocate( cVector );
    result = DXRUBY_GET_STRUCT( Vector, vresult );
    result->x = 2;

    result->v1 = vec->v1;
    result->v2 = vec->v2;

    return vresult;
}

static VALUE Vector_get_xyz( VALUE self )
{
    struct DXRubyVector *vec = DXRUBY_GET_STRUCT( Vector, self );
    struct DXRubyVector *result;
    VALUE vresult;

    vresult = Vector_allocate( cVector );
    result = DXRUBY_GET_STRUCT( Vector, vresult );
    result->x = 3;

    result->v1 = vec->v1;
    result->v2 = vec->v2;
    result->v3 = vec->v3;

    return vresult;
}

static VALUE Vector_equal( VALUE self, VALUE vvector )
{
    struct DXRubyVector *vec_d = DXRUBY_GET_STRUCT( Vector, self );
    struct DXRubyVector *vec_s;
    int i;

    DXRUBY_CHECK_TYPE( Vector, vvector );
    vec_s =  DXRUBY_GET_STRUCT( Vector, vvector );

    if( vec_d->x != vec_s->x )
    {
        return Qfalse;
    }
    for( i = 0; i < vec_d->x; i++ )
    {
        if( vec_d->v[i] != vec_s->v[i] )
        {
            return Qfalse;
        }
    }

    return Qtrue;
}

static VALUE Vector_translate( int argc, VALUE *argv, VALUE self )
{
    struct DXRubyVector *vec = DXRUBY_GET_STRUCT( Vector, self );
    struct DXRubyVector *result;
    VALUE vresult;
    int i;

    if( argc < 1 || argc > 4 ) rb_raise( eDXRubyError, "引数の数が正しくありません。 - Vector_translate");
    vresult = Vector_allocate( cVector );
    result = DXRUBY_GET_STRUCT( Vector, vresult );
    result->x = vec->x;

    for( i = 0; i < vec->x; i++ )
    {
        result->v[i] = vec->v[i];
    }

    for( i = 0; i < vec->x && i < argc; i++ )
    {
        result->v[i] = vec->v[i] + NUM2FLOAT( argv[i] );
    }

    return vresult;
}

static VALUE Vector_rotate( int argc, VALUE *argv, VALUE obj  )
{
    struct DXRubyVector *vec = DXRUBY_GET_STRUCT( Vector, obj );
    struct DXRubyVector *center = NULL;
    struct DXRubyVector *result;
    VALUE vresult, vangle, vcenter;
    float angle, x, y;

    if( vec->x != 2 && vec->x != 3 ) rb_raise( eDXRubyError, "2D回転できるVectorではありません - Vector_rotate");

    rb_scan_args( argc, argv, "11", &vangle, &vcenter );

    if( vcenter != Qnil )
    {
        center = DXRUBY_GET_STRUCT( Vector, vcenter );
        if( center->x != 2 && center->x != 3 ) rb_raise( eDXRubyError, "回転中心に設定できるVectorではありません - Vector_rotate");
    }

    vresult = Vector_allocate( cVector );
    result = DXRUBY_GET_STRUCT( Vector, vresult );
    result->x = vec->x;
    x = vec->v1;
    y = vec->v2;

    if( center )
    {
        x -= center->v1;
        y -= center->v2;
    }

    angle = pi / 180.0f * NUM2FLOAT( vangle );
    x = cos( angle ) * x - sin( angle ) * y;
    y = sin( angle ) * x + cos( angle ) * y;

    if( center )
    {
        x += center->v1;
        y += center->v2;
    }

    result->v1 = x;
    result->v2 = y;
    result->v3 = vec->v3;

    return vresult;
}

static VALUE Vector_rotate_x( VALUE self, VALUE vangle )
{
    struct DXRubyVector *vec = DXRUBY_GET_STRUCT( Vector, self );
    struct DXRubyVector *result;
    VALUE vresult;
    float angle;

    if( vec->x != 3 && vec->x != 4 ) rb_raise( eDXRubyError, "3D回転できるVectorではありません - Vector_rotate_x");
    vresult = Vector_allocate( cVector );
    result = DXRUBY_GET_STRUCT( Vector, vresult );
    result->x = vec->x;

    angle = pi / 180.0f * NUM2FLOAT( vangle );
    result->v1 = vec->v1;
    result->v2 = -sin( angle ) * vec->v2 + cos( angle ) * vec->v3;
    result->v3 = cos( angle ) * vec->v2 + sin( angle ) * vec->v3;
    result->v4 = vec->v4;

    return vresult;
}

static VALUE Vector_rotate_y( VALUE self, VALUE vangle )
{
    struct DXRubyVector *vec = DXRUBY_GET_STRUCT( Vector, self );
    struct DXRubyVector *result;
    VALUE vresult;
    float angle;

    if( vec->x != 3 && vec->x != 4 ) rb_raise( eDXRubyError, "3D回転できるVectorではありません - Vector_rotate_x");
    vresult = Vector_allocate( cVector );
    result = DXRUBY_GET_STRUCT( Vector, vresult );
    result->x = vec->x;

    angle = pi / 180.0f * NUM2FLOAT( vangle );
    result->v1 = sin( angle ) * vec->v1 + cos( angle ) * vec->v3;
    result->v2 = vec->v2;
    result->v3 = cos( angle ) * vec->v1 - sin( angle ) * vec->v3;
    result->v4 = vec->v4;

    return vresult;
}

static VALUE Vector_rotate_z( VALUE self, VALUE vangle )
{
    struct DXRubyVector *vec = DXRUBY_GET_STRUCT( Vector, self );
    struct DXRubyVector *result;
    VALUE vresult;
    float angle;

    if( vec->x != 3 && vec->x != 4 ) rb_raise( eDXRubyError, "3D回転できるVectorではありません - Vector_rotate_x");
    vresult = Vector_allocate( cVector );
    result = DXRUBY_GET_STRUCT( Vector, vresult );
    result->x = vec->x;

    angle = pi / 180.0f * NUM2FLOAT( vangle );
    result->v1 = cos( angle ) * vec->v1 - sin( angle ) * vec->v2;
    result->v2 = sin( angle ) * vec->v1 + cos( angle ) * vec->v2;
    result->v3 = vec->v3;
    result->v4 = vec->v4;

    return vresult;
}

/* 配列化 */
static VALUE Vector_to_a( VALUE self )
{
    struct DXRubyVector *vec = DXRUBY_GET_STRUCT( Vector, self );
    VALUE vresult;
    int i;

    vresult = rb_ary_new();
    for( i = 0; i < vec->x; i++ )
    {
        rb_ary_push( vresult, rb_float_new( vec->v[i] ) );
    }
    return vresult;
}


static VALUE Vector_angle_to( VALUE self, VALUE vvector )
{
    struct DXRubyVector *vec_d = DXRUBY_GET_STRUCT( Vector, self );
    struct DXRubyVector *vec_s;
    float angle;

    DXRUBY_CHECK_TYPE( Vector, vvector );
    vec_s =  DXRUBY_GET_STRUCT( Vector, vvector );

    angle = atan2(vec_s->v2 - vec_d->v2, vec_s->v1 - vec_d->v1) / pi * 180;

    return rb_float_new(angle);
}


/*
***************************************************************
*
*         Global functions
*
***************************************************************/

void Init_dxruby_Matrix()
{

    /* Matrixクラス定義 */
    cMatrix = rb_define_class_under( mDXRuby, "Matrix", rb_cObject );

    /* Matrixクラスにクラスメソッド登録*/
    rb_define_singleton_method( cMatrix, "look_at", Matrix_look_at, 3 );
    rb_define_singleton_method( cMatrix, "lookAt", Matrix_look_at, 3 );
    rb_define_singleton_method( cMatrix, "projection", Matrix_create_projection, 4 );
    rb_define_singleton_method( cMatrix, "projection_fov", Matrix_create_projection_fov, 4 );
    rb_define_singleton_method( cMatrix, "projectionFov", Matrix_create_projection_fov, 4 );
    rb_define_singleton_method( cMatrix, "projection_ortho", Matrix_create_projection_ortho, 4 );
    rb_define_singleton_method( cMatrix, "projectionOrtho", Matrix_create_projection_ortho, 4 );
    rb_define_singleton_method( cMatrix, "rotation", Matrix_create_rot, 1 );
    rb_define_singleton_method( cMatrix, "rotation_x", Matrix_create_rot_x, 1 );
    rb_define_singleton_method( cMatrix, "rotationX", Matrix_create_rot_x, 1 );
    rb_define_singleton_method( cMatrix, "rotation_y", Matrix_create_rot_y, 1 );
    rb_define_singleton_method( cMatrix, "rotationY", Matrix_create_rot_y, 1 );
    rb_define_singleton_method( cMatrix, "rotation_z", Matrix_create_rot_z, 1 );
    rb_define_singleton_method( cMatrix, "rotationZ", Matrix_create_rot_z, 1 );
    rb_define_singleton_method( cMatrix, "scaling", Matrix_create_scale, -1 );
    rb_define_singleton_method( cMatrix, "translation", Matrix_create_trans, -1 );

    /* Matrixクラスにインスタンスメソッド登録*/
    rb_define_private_method( cMatrix, "initialize", Matrix_initialize, -1 );
    rb_define_method( cMatrix, "*", Matrix_mul, 1 );
    rb_define_method( cMatrix, "to_s", Matrix_to_s, 0 );
    rb_define_method( cMatrix, "to_a", Matrix_to_a, 0 );
    rb_define_method( cMatrix, "inverse", Matrix_inverse, 0 );

    /* Matrixオブジェクトを生成した時にinitializeの前に呼ばれるメモリ割り当て関数登録 */
    rb_define_alloc_func( cMatrix, Matrix_allocate );


    /* Vectorクラス定義 */
    cVector = rb_define_class_under( mDXRuby, "Vector", rb_cObject );

    /* Vectorクラスにクラスメソッド登録*/
    rb_define_singleton_method( cVector, "distance", Vector_distance, 2 );
    rb_define_singleton_method( cVector, "cross_product", Vector_cross_product, 2 );
    rb_define_singleton_method( cVector, "crossProduct", Vector_cross_product, 2 );
    rb_define_singleton_method( cVector, "dot_product", Vector_dot_product, 2 );
    rb_define_singleton_method( cVector, "dotProduct", Vector_dot_product, 2 );

    /* Vectorクラスにインスタンスメソッド登録*/
    rb_define_private_method( cVector, "initialize", Vector_initialize, -1 );
    rb_define_method( cVector, "*", Vector_mul, 1 );
    rb_define_method( cVector, "+", Vector_add, 1 );
    rb_define_method( cVector, "-", Vector_sub, 1 );
    rb_define_method( cVector, "-@", Vector_minus, 0 );
    rb_define_method( cVector, "/", Vector_div, 1 );
    rb_define_method( cVector, "to_s", Vector_to_s, 0 );
    rb_define_method( cVector, "to_a", Vector_to_a, 0 );
    rb_define_method( cVector, "x", Vector_get_x, 0 );
    rb_define_method( cVector, "y", Vector_get_y, 0 );
    rb_define_method( cVector, "z", Vector_get_z, 0 );
    rb_define_method( cVector, "w", Vector_get_w, 0 );
    rb_define_method( cVector, "xy", Vector_get_xy, 0 );
    rb_define_method( cVector, "xyz", Vector_get_xyz, 0 );
    rb_define_method( cVector, "size", Vector_get_size, 0 );
    rb_define_method( cVector, "normalize", Vector_normalize, 0 );
    rb_define_method( cVector, "==", Vector_equal, 1 );
    rb_define_method( cVector, "translate", Vector_translate, -1 );
    rb_define_method( cVector, "rotate", Vector_rotate, -1 );
    rb_define_method( cVector, "rotate_x", Vector_rotate_x, 1 );
    rb_define_method( cVector, "rotateX", Vector_rotate_x, 1 );
    rb_define_method( cVector, "rotate_y", Vector_rotate_y, 1 );
    rb_define_method( cVector, "rotateY", Vector_rotate_y, 1 );
    rb_define_method( cVector, "rotate_z", Vector_rotate_z, 1 );
    rb_define_method( cVector, "rotateZ", Vector_rotate_z, 1 );
    rb_define_method( cVector, "angle_to", Vector_angle_to, 1 );
    rb_define_method( cVector, "angleTo", Vector_angle_to, 1 );

    /* Vectorオブジェクトを生成した時にinitializeの前に呼ばれるメモリ割り当て関数登録 */
    rb_define_alloc_func( cVector, Vector_allocate );

}

