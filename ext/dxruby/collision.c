#define WINVER 0x0500                                  /* バージョン定義 Windows2000以上 */
#define _WIN32_WINNT WINVER

#include "ruby.h"
#ifndef RUBY_ST_H
#include "st.h"
#endif

#define DXRUBY_EXTERN 1
#include "dxruby.h"
#include "sprite.h"
#include "image.h"
#include "collision.h"

static int g_volume_count;
static int g_volume_allocate_count;
static struct DXRubyCollision *g_volume_pointer;

#ifdef DXRUBY_USE_TYPEDDATA
extern rb_data_type_t Image_data_type;
extern rb_data_type_t RenderTarget_data_type;
extern rb_data_type_t Sprite_data_type;
#endif

/*********************************************************************
 * 衝突判定処理
 *
 *********************************************************************/

/* 境界ボリューム作成 */
#define volume_box( count, tx, ty, collision ) \
{\
    int i;\
    (collision)->x1 = (collision)->x2 = (int)tx[0];\
    (collision)->y1 = (collision)->y2 = (int)ty[0];\
\
    for( i = 1; i < count; i++ )\
    {\
        if( (collision)->x1 > (int)tx[i] )\
        {\
            (collision)->x1 = (int)tx[i];\
        }\
        if( (collision)->x2 < (int)tx[i] )\
        {\
            (collision)->x2 = (int)tx[i];\
        }\
        if( (collision)->y1 > (int)ty[i] )\
        {\
            (collision)->y1 = (int)ty[i];\
        }\
        if( (collision)->y2 < (int)ty[i] )\
        {\
            (collision)->y2 = (int)ty[i];\
        }\
    }\
}

/* 中心点算出 */
#define set_center( sprite, collision )\
{\
    struct DXRubyImage *image;\
    if( sprite->vcenter_x == Qnil || sprite->vcenter_y == Qnil )\
    {\
        DXRUBY_CHECK_IMAGE( sprite->vimage );\
        image = DXRUBY_GET_STRUCT( Image, sprite->vimage );\
        collision->center_x = sprite->vcenter_x == Qnil ? image->width / 2 : NUM2FLOAT(sprite->vcenter_x);\
        collision->center_y = sprite->vcenter_y == Qnil ? image->height / 2 : NUM2FLOAT(sprite->vcenter_y);\
    }\
    else\
    {\
        collision->center_x = NUM2FLOAT(sprite->vcenter_x);\
        collision->center_y = NUM2FLOAT(sprite->vcenter_y);\
    }\
}

/* 点の回転 */
#define rotation_point( collision, tx, ty, bx, by ) \
{\
    float angle = 3.141592653589793115997963468544185161590576171875f / 180.0f * collision->angle;\
    float sina = sin( angle );\
    float cosa = cos( angle );\
    float data1x = collision->scale_x * cosa;\
    float data1y = collision->scale_y * sina;\
    float data2x = collision->scale_x * sina;\
    float data2y = collision->scale_y * cosa;\
    float tmpx = (bx), tmpy = (by);\
\
    tx = (tmpx - collision->center_x) * data1x - (tmpy - collision->center_y) * data1y + collision->center_x + collision->base_x;\
    ty = (tmpx - collision->center_x) * data2x + (tmpy - collision->center_y) * data2y + collision->center_y + collision->base_y;\
}

/* 点の回転(中心指定・スケーリングなし) */
#define rotation_point_out( centerx, centery, angle, x, y ) \
{\
    float rbangle = 3.141592653589793115997963468544185161590576171875f / 180.0f * (angle);\
    float sina = sin( rbangle );\
    float cosa = cos( rbangle );\
    float rbx = (x), rby = (y);\
\
    x = (rbx - (centerx)) * cosa - (rby - (centery)) * sina + (centerx);\
    y = (rbx - (centerx)) * sina + (rby - (centery)) * cosa + (centery);\
}

/* 矩形それ自身の回転(スケーリングあり) */
#define rotation_box( collision, tx, ty ) \
{\
    float angle = 3.141592653589793115997963468544185161590576171875f / 180.0f * collision->angle;\
    float sina = sin( angle );\
    float cosa = cos( angle );\
    float data1x = collision->scale_x * cosa;\
    float data1y = collision->scale_y * sina;\
    float data2x = collision->scale_x * sina;\
    float data2y = collision->scale_y * cosa;\
\
    tx[0] = (collision->bx1 - collision->center_x) * data1x - (collision->by1 - collision->center_y) * data1y + collision->center_x + collision->base_x;\
    ty[0] = (collision->bx1 - collision->center_x) * data2x + (collision->by1 - collision->center_y) * data2y + collision->center_y + collision->base_y;\
    tx[1] = (collision->bx2 - collision->center_x) * data1x - (collision->by1 - collision->center_y) * data1y + collision->center_x + collision->base_x;\
    ty[1] = (collision->bx2 - collision->center_x) * data2x + (collision->by1 - collision->center_y) * data2y + collision->center_y + collision->base_y;\
    tx[2] = (collision->bx2 - collision->center_x) * data1x - (collision->by2 - collision->center_y) * data1y + collision->center_x + collision->base_x;\
    ty[2] = (collision->bx2 - collision->center_x) * data2x + (collision->by2 - collision->center_y) * data2y + collision->center_y + collision->base_y;\
    tx[3] = (collision->bx1 - collision->center_x) * data1x - (collision->by2 - collision->center_y) * data1y + collision->center_x + collision->base_x;\
    ty[3] = (collision->bx1 - collision->center_x) * data2x + (collision->by2 - collision->center_y) * data2y + collision->center_y + collision->base_y;\
}

/* 矩形の回転（中心指定・スケーリングなし） */
#define rotation_box_out( centerx, centery, angle, x, y ) \
{\
    float rbangle = 3.141592653589793115997963468544185161590576171875f / 180.0f * angle;\
    float sina = sin( rbangle );\
    float cosa = cos( rbangle );\
    float rbx1 = x[0], rby1 = y[0], rbx2 = x[1], rby2 = y[1], rbx3 = x[2], rby3 = y[2], rbx4 = x[3], rby4 = y[3];\
\
    x[0] = (rbx1 - centerx) * cosa - (rby1 - centery) * sina + centerx;\
    y[0] = (rbx1 - centerx) * sina + (rby1 - centery) * cosa + centery;\
    x[1] = (rbx2 - centerx) * cosa - (rby2 - centery) * sina + centerx;\
    y[1] = (rbx2 - centerx) * sina + (rby2 - centery) * cosa + centery;\
    x[2] = (rbx3 - centerx) * cosa - (rby3 - centery) * sina + centerx;\
    y[2] = (rbx3 - centerx) * sina + (rby3 - centery) * cosa + centery;\
    x[3] = (rbx4 - centerx) * cosa - (rby4 - centery) * sina + centerx;\
    y[3] = (rbx4 - centerx) * sina + (rby4 - centery) * cosa + centery;\
}

/* 矩形の拡大・縮小（回転なし） */
#define scaling_box( collision, x, y ) \
{\
    x[0] = x[3] = (collision->bx1 - collision->center_x) * collision->scale_x + collision->center_x + collision->base_x;\
    y[0] = y[1] = (collision->by1 - collision->center_y) * collision->scale_y + collision->center_y + collision->base_y;\
    x[1] = x[2] = (collision->bx2 - collision->center_x) * collision->scale_x + collision->center_x + collision->base_x;\
    y[2] = y[3] = (collision->by2 - collision->center_y) * collision->scale_y + collision->center_y + collision->base_y;\
}

/* 三角形それ自身の回転(スケーリングあり) */
#define rotation_triangle( collision, x, y, tx, ty ) \
{\
    float angle = 3.141592653589793115997963468544185161590576171875f / 180.0f * collision->angle;\
    float sina = sin( angle );\
    float cosa = cos( angle );\
    float data1x = collision->scale_x * cosa;\
    float data1y = collision->scale_y * sina;\
    float data2x = collision->scale_x * sina;\
    float data2y = collision->scale_y * cosa;\
\
    tx[0] = (x[0] - collision->center_x) * data1x - (y[0] - collision->center_y) * data1y + collision->center_x + collision->base_x;\
    ty[0] = (x[0] - collision->center_x) * data2x + (y[0] - collision->center_y) * data2y + collision->center_y + collision->base_y;\
    tx[1] = (x[1] - collision->center_x) * data1x - (y[1] - collision->center_y) * data1y + collision->center_x + collision->base_x;\
    ty[1] = (x[1] - collision->center_x) * data2x + (y[1] - collision->center_y) * data2y + collision->center_y + collision->base_y;\
    tx[2] = (x[2] - collision->center_x) * data1x - (y[2] - collision->center_y) * data1y + collision->center_x + collision->base_x;\
    ty[2] = (x[2] - collision->center_x) * data2x + (y[2] - collision->center_y) * data2y + collision->center_y + collision->base_y;\
}

/* 三角形の回転（中心指定・スケーリングなし） */
#define rotation_triangle_out( centerx, centery, angle, x, y ) \
{\
    float rbangle = 3.141592653589793115997963468544185161590576171875f / 180.0f * angle;\
    float sina = sin( rbangle );\
    float cosa = cos( rbangle );\
    float rbx1 = x[0], rby1 = y[0], rbx2 = x[1], rby2 = y[1], rbx3 = x[2], rby3 = y[2];\
\
    x[0] = (rbx1 - centerx) * cosa - (rby1 - centery) * sina + centerx;\
    y[0] = (rbx1 - centerx) * sina + (rby1 - centery) * cosa + centery;\
    x[1] = (rbx2 - centerx) * cosa - (rby2 - centery) * sina + centerx;\
    y[1] = (rbx2 - centerx) * sina + (rby2 - centery) * cosa + centery;\
    x[2] = (rbx3 - centerx) * cosa - (rby3 - centery) * sina + centerx;\
    y[2] = (rbx3 - centerx) * sina + (rby3 - centery) * cosa + centery;\
}


#define intersect(x1, y1, x2, y2, x3, y3, x4, y4) ( ((x1 - x2) * (y3 - y1) + (y1 - y2) * (x1 - x3)) * \
                                                    ((x1 - x2) * (y4 - y1) + (y1 - y2) * (x1 - x4)) )

struct Vector {
    float x;
    float y;
};


/*--------------------------------------------------------------------
    (内部処理用)三角と点の判定
 ---------------------------------------------------------------------*/
/* 右回りの三角と点 */
static int checktriangle( float x, float y, float x1, float y1, float x2, float y2, float x3, float y3 )
{
    float cx, cy;

    if( (x1 - x3) * (y1 - y2) == (x1 - x2) * (y1 - y3) )
    {
        return 0;
    }

    cx = (x1 + x2 + x3) / 3; /* 中心点x */
    cy = (y1 + y2 + y3) / 3; /* 中心点y */

    if( intersect( x1, y1, x2, y2, x, y, cx, cy ) < 0.0f ||
        intersect( x2, y2, x3, y3, x, y, cx, cy ) < 0.0f ||
        intersect( x3, y3, x1, y1, x, y, cx, cy ) < 0.0f )
    {
        return 0;
    }
    return -1;
}


/*--------------------------------------------------------------------
    (内部処理用)円と線分の判定
 ---------------------------------------------------------------------*/
/* 円と線分の判定 */
static int checkCircleLine( float x, float y, float r, float x1, float y1, float x2, float y2 )
{
    float n1, n2, n3;
    /* vは線分始点から終点 */
    /* cは線分始点から円中心 */
    struct Vector v = {x2 - x1, y2 - y1};
    struct Vector c = {x - x1, y - y1};

    if( v.x == 0.0f && v.y == 0.0f )
    {
        return check_circle_point(x, y, r, x1, y1);
    }

    /* 二つのベクトルの内積を求める */
    n1 = v.x * c.x + v.y * c.y;

    if( n1 < 0 )
    {
        /* cの長さが円の半径より小さい場合は交差している */
        return c.x*c.x + c.y*c.y < r * r ? -1 : 0;
    }

    n2 = v.x * v.x + v.y * v.y;

    if( n1 > n2 )
    {
        float len;
        /* 線分の終点と円の中心の距離の二乗を求める */
        len = (x2 - x)*(x2 - x) + (y2 - y)*(y2 - y);
        /* 円の半径の二乗よりも小さい場合は交差している */
        return  len < r * r ? -1 : 0;
    }
    else
    {
        n3 = c.x * c.x + c.y * c.y;
        return ( n3-(n1/n2)*n1 < r * r ) ? -1 : 0;
    }
    return 0;
}


/* 楕円構造体 */
struct ELLIPSE
{
   float fRad_X; /* X軸径 */
   float fRad_Y; /* Y軸径 */
   float fAngle; /* 回転角度 */
   float fCx; /* 制御点X座標 */
   float fCy; /* 制御点Y座標 */
};

/* 楕円衝突判定関数 */
/* http://marupeke296.com/COL_2D_No7_EllipseVsEllipse.html */
int CollisionEllipse( struct ELLIPSE E1, struct ELLIPSE E2 )
{
   /* STEP1 : E2を単位円にする変換をE1に施す */
   float DefAng = E1.fAngle-E2.fAngle;
   float Cos = cos( DefAng );
   float Sin = sin( DefAng );
   float nx = E2.fRad_X * Cos;
   float ny = -E2.fRad_X * Sin;
   float px = E2.fRad_Y * Sin;
   float py = E2.fRad_Y * Cos;
   float ox = cos( E1.fAngle )*(E2.fCx-E1.fCx) + sin(E1.fAngle)*(E2.fCy-E1.fCy);
   float oy = -sin( E1.fAngle )*(E2.fCx-E1.fCx) + cos(E1.fAngle)*(E2.fCy-E1.fCy);

   /* STEP2 : 一般式A～Gの算出 */
   float rx_pow2 = 1/(E1.fRad_X*E1.fRad_X);
   float ry_pow2 = 1/(E1.fRad_Y*E1.fRad_Y);
   float A = rx_pow2*nx*nx + ry_pow2*ny*ny;
   float B = rx_pow2*px*px + ry_pow2*py*py;
   float D = 2*rx_pow2*nx*px + 2*ry_pow2*ny*py;
   float E = 2*rx_pow2*nx*ox + 2*ry_pow2*ny*oy;
   float F = 2*rx_pow2*px*ox + 2*ry_pow2*py*oy;
   float G = (ox/E1.fRad_X)*(ox/E1.fRad_X) + (oy/E1.fRad_Y)*(oy/E1.fRad_Y) - 1;

   /* STEP3 : 平行移動量(h,k)及び回転角度θの算出 */
   float tmp1 = 1/(D*D-4*A*B);
   float h = (F*D-2*E*B)*tmp1;
   float k = (E*D-2*A*F)*tmp1;
   float Th = (B-A)==0?0:atan( D/(B-A) ) * 0.5f;

   /* STEP4 : +1楕円を元に戻した式で当たり判定 */
   float CosTh = cos(Th);
   float SinTh = sin(Th);
   float A_tt = A*CosTh*CosTh + B*SinTh*SinTh - D*CosTh*SinTh;
   float B_tt = A*SinTh*SinTh + B*CosTh*CosTh + D*CosTh*SinTh;
   float KK = A*h*h + B*k*k + D*h*k - E*h - F*k + G > 0 ? 0 : A*h*h + B*k*k + D*h*k - E*h - F*k + G;
   float Rx_tt = 1+sqrt(-KK/A_tt);
   float Ry_tt = 1+sqrt(-KK/B_tt);
   float x_tt = CosTh*h-SinTh*k;
   float y_tt = SinTh*h+CosTh*k;
   float JudgeValue = x_tt*x_tt/(Rx_tt*Rx_tt) + y_tt*y_tt/(Ry_tt*Ry_tt);

   if( JudgeValue <= 1 )
      return TRUE; /* 衝突 */

   return FALSE;
}


int check( struct DXRubyCollisionGroup *o, struct DXRubyCollisionGroup *d )
{
    int i, j;

    if( o->count == 1 && d->count == 1 )
    {
        struct DXRubyCollision *co = g_volume_pointer + o->index;
        struct DXRubyCollision *cd = g_volume_pointer + d->index;

        /* 詳細チェック */
        if( check_sub( co, cd ) )
        {
            return TRUE;
        }
    }
    else
    {
        for( i = 0; i < o->count; i++ )
        {
            for( j = 0; j < d->count; j++ )
            {
                struct DXRubyCollision *co = g_volume_pointer + o->index + i;
                struct DXRubyCollision *cd = g_volume_pointer + d->index + j;

                /* 境界ボリュームチェック＆詳細チェック */
                if( check_box_box( co, cd ) && check_sub( co, cd ) )
                {
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}


int check_sub( struct DXRubyCollision *o, struct DXRubyCollision *d )
{
    struct DXRubySprite *o_sprite = DXRUBY_GET_STRUCT( Sprite, o->vsprite );
    struct DXRubySprite *d_sprite = DXRUBY_GET_STRUCT( Sprite, d->vsprite );
    int o_type;
    int d_type;

    /* collision省略時 */
    if( o->vcollision != Qnil )
    {
        o_type = RARRAY_LEN( o->vcollision );
    }
    else
    {
        o_type = 4;
    }

    if( d->vcollision != Qnil )
    {
        d_type = RARRAY_LEN( d->vcollision );
    }
    else
    {
        d_type = 4;
    }

    if( o_type == d_type )
    {/* 同じ形の比較 */
        switch ( o_type )
        {
        case 4: /* 矩形 */
                /* 分離軸判定で回転を処理する */

            if( o->rotation_flg || o->scaling_flg ) /* o側が回転している */
            { /* dをo中心に回転して境界ボリュームを作り比較する。当たっていなければ当たっていない。 */
                float ox[4], oy[4];
                float dx[4], dy[4];
                struct DXRubyCollision o_collision, d_collision;
                float centerx, centery;

                if( d->rotation_flg || d->scaling_flg ) /* d側が回転している */
                {
                    /* d側を自分の姿勢に回転 */
                    rotation_box( d, dx, dy );
                }
                else /* d側は回転していなかった */
                {
                    dx[0] = dx[3] = (float)d->x1;
                    dy[0] = dy[1] = (float)d->y1;
                    dx[1] = dx[2] = (float)d->x2;
                    dy[2] = dy[3] = (float)d->y2;
                }

                /* o側中心点 */
                centerx = o->center_x + o->base_x;
                centery = o->center_y + o->base_y;

                /* o側中心点を中心にd側回転 */
                rotation_box_out( centerx, centery, -o->angle, dx, dy )

                /* d側境界ボリューム作成 */
                volume_box( 4, dx, dy, &d_collision );

                /* o側のスケーリング */
                scaling_box( o, ox, oy );

                /* o側境界ボリューム作成 */
                volume_box( 4, ox, oy, &o_collision );

                if( !check_box_box( &o_collision, &d_collision ) )
                {
                    return FALSE; /* 当たっていなかった */
                }
            }

            if( d->rotation_flg || d->scaling_flg ) /* d側が回転している */
            { /* oをd中心に回転して境界ボリュームを作り比較する。当たっていなければ当たっていない。 */
                float ox[4], oy[4];
                float dx[4], dy[4];
                struct DXRubyCollision o_collision, d_collision;
                float centerx, centery;

                if( o->rotation_flg || o->scaling_flg ) /* o側が回転している */
                {
                    /* o側を自分の姿勢に回転 */
                    rotation_box( o, ox, oy );
                }
                else /* o側は回転していなかった */
                {
                    ox[0] = ox[3] = (float)o->x1;
                    oy[0] = oy[1] = (float)o->y1;
                    ox[1] = ox[2] = (float)o->x2;
                    oy[2] = oy[3] = (float)o->y2;
                }

                /* d側中心点 */
                centerx = d->center_x + d->base_x;
                centery = d->center_y + d->base_y;

                /* d側中心点を中心にo側回転 */
                rotation_box_out( centerx, centery, -d->angle, ox, oy )

                /* o側境界ボリューム作成 */
                volume_box( 4, ox, oy, &o_collision );

                /* d側のスケーリング */
                scaling_box( d, dx, dy );

                /* d側境界ボリューム作成 */
                volume_box( 4, dx, dy, &d_collision );

                if( !check_box_box( &o_collision, &d_collision ) )
                {
                    return FALSE; /* 当たっていなかった */
                }
            }
            return TRUE; /* ここに来た時点で当たっている */
            break;
        case 3: /* 円同士 */
        {

            if( (o->scale_x != o->scale_y && RTEST(o_sprite->vcollision_sync)) || /* o側が楕円 */
                (d->scale_x != d->scale_y && RTEST(d_sprite->vcollision_sync)) )  /* d側が楕円 */
            { /* どっちかが楕円なら楕円方程式での衝突判定をする */
                struct ELLIPSE e1, e2;
                float ox, oy, or, dx, dy, dr;
                if( RTEST(o_sprite->vcollision_sync) )
                {
                    rotation_point( o, ox, oy, NUM2FLOAT(RARRAY_AREF(o->vcollision, 0)), NUM2FLOAT(RARRAY_AREF(o->vcollision, 1)) );
                }
                else
                {
                    ox = o->base_x + NUM2INT(RARRAY_AREF(o->vcollision, 0));
                    oy = o->base_y + NUM2INT(RARRAY_AREF(o->vcollision, 1));
                }
                or = NUM2FLOAT(RARRAY_AREF(o->vcollision, 2));

                e1.fCx = ox;
                e1.fCy = oy;
                if( RTEST(o_sprite->vcollision_sync) )
                {
                    e1.fRad_X = o->scale_x * or * 1;
                    e1.fRad_Y = o->scale_y * or * 1;
                    e1.fAngle = 3.141592653589793115997963468544185161590576171875f / 180.0f * o->angle;
                }
                else
                {
                    e1.fRad_X = or * 1;
                    e1.fRad_Y = or * 1;
                    e1.fAngle = 0;
                }

                if( RTEST(d_sprite->vcollision_sync) )
                {
                    rotation_point( d, dx, dy, NUM2FLOAT(RARRAY_AREF(d->vcollision, 0)), NUM2FLOAT(RARRAY_AREF(d->vcollision, 1)) );
                }
                else
                {
                    dx = d->base_x + NUM2INT(RARRAY_AREF(d->vcollision, 0));
                    dy = d->base_y + NUM2INT(RARRAY_AREF(d->vcollision, 1));
                }
                dr = NUM2FLOAT(RARRAY_AREF(d->vcollision, 2));

                e2.fCx = dx;
                e2.fCy = dy;
                if( RTEST(d_sprite->vcollision_sync) )
                {
                    e2.fRad_X = d->scale_x * dr * 1;
                    e2.fRad_Y = d->scale_y * dr * 1;
                    e2.fAngle = 3.141592653589793115997963468544185161590576171875f / 180.0f * d->angle;
                }
                else
                {
                    e2.fRad_X = dr * 1;
                    e2.fRad_Y = dr * 1;
                    e2.fAngle = 0;
                }

                return CollisionEllipse( e1, e2 );
            }
            else
            { /* 真円同士 */
                float ox, oy, or, dx, dy, dr;

                if( o->rotation_flg ) /* o側が回転している */
                {
                    rotation_point( o, ox, oy, NUM2FLOAT(RARRAY_AREF(o->vcollision, 0)), NUM2FLOAT(RARRAY_AREF(o->vcollision, 1)) );
                }
                else
                {
                    ox = o->base_x + NUM2INT(RARRAY_AREF(o->vcollision, 0));
                    oy = o->base_y + NUM2INT(RARRAY_AREF(o->vcollision, 1));
                }
                or = NUM2FLOAT(RARRAY_AREF(o->vcollision, 2)) * o->scale_x;

                if( d->rotation_flg ) /* d側が回転している */
                {
                    rotation_point( d, dx, dy, NUM2FLOAT(RARRAY_AREF(d->vcollision, 0)), NUM2FLOAT(RARRAY_AREF(d->vcollision, 1)) );
                }
                else
                {
                    dx = d->base_x + NUM2INT(RARRAY_AREF(d->vcollision, 0));
                    dy = d->base_y + NUM2INT(RARRAY_AREF(d->vcollision, 1));
                }
                dr = NUM2FLOAT(RARRAY_AREF(d->vcollision, 2)) * d->scale_x;

                return check_circle_circle( ox, oy, or, dx, dy, dr );
            }
        }
            break;
        case 6: /* 三角 */
            {
                float ox[3], oy[3];
                float dx[3], dy[3];
                float x[3], y[3];

                if( o->rotation_flg || o->scaling_flg ) /* o側が回転している */
                {
                    x[0] = NUM2INT(RARRAY_AREF(o->vcollision, 0)) + 0.5f;
                    y[0] = NUM2INT(RARRAY_AREF(o->vcollision, 1)) + 0.5f;
                    x[1] = NUM2INT(RARRAY_AREF(o->vcollision, 2)) + 0.5f;
                    y[1] = NUM2INT(RARRAY_AREF(o->vcollision, 3)) + 0.5f;
                    x[2] = NUM2INT(RARRAY_AREF(o->vcollision, 4)) + 0.5f;
                    y[2] = NUM2INT(RARRAY_AREF(o->vcollision, 5)) + 0.5f;

                    set_center( o_sprite, o );
                    rotation_triangle( o, x, y, ox, oy );
                }
                else
                {
                    ox[0] = NUM2INT(RARRAY_AREF(o->vcollision, 0)) + 0.5f + o->base_x;
                    oy[0] = NUM2INT(RARRAY_AREF(o->vcollision, 1)) + 0.5f + o->base_y;
                    ox[1] = NUM2INT(RARRAY_AREF(o->vcollision, 2)) + 0.5f + o->base_x;
                    oy[1] = NUM2INT(RARRAY_AREF(o->vcollision, 3)) + 0.5f + o->base_y;
                    ox[2] = NUM2INT(RARRAY_AREF(o->vcollision, 4)) + 0.5f + o->base_x;
                    oy[2] = NUM2INT(RARRAY_AREF(o->vcollision, 5)) + 0.5f + o->base_y;
                }

                if( d->rotation_flg || d->scaling_flg ) /* d側が回転している */
                {
                    x[0] = NUM2INT(RARRAY_AREF(d->vcollision, 0)) + 0.5f;
                    y[0] = NUM2INT(RARRAY_AREF(d->vcollision, 1)) + 0.5f;
                    x[1] = NUM2INT(RARRAY_AREF(d->vcollision, 2)) + 0.5f;
                    y[1] = NUM2INT(RARRAY_AREF(d->vcollision, 3)) + 0.5f;
                    x[2] = NUM2INT(RARRAY_AREF(d->vcollision, 4)) + 0.5f;
                    y[2] = NUM2INT(RARRAY_AREF(d->vcollision, 5)) + 0.5f;

                    set_center( d_sprite, d );
                    rotation_triangle( d, x, y, dx, dy );
                }
                else
                {
                    dx[0] = NUM2INT(RARRAY_AREF(d->vcollision, 0)) + 0.5f + d->base_x;
                    dy[0] = NUM2INT(RARRAY_AREF(d->vcollision, 1)) + 0.5f + d->base_y;
                    dx[1] = NUM2INT(RARRAY_AREF(d->vcollision, 2)) + 0.5f + d->base_x;
                    dy[1] = NUM2INT(RARRAY_AREF(d->vcollision, 3)) + 0.5f + d->base_y;
                    dx[2] = NUM2INT(RARRAY_AREF(d->vcollision, 4)) + 0.5f + d->base_x;
                    dy[2] = NUM2INT(RARRAY_AREF(d->vcollision, 5)) + 0.5f + d->base_y;
                }

                return check_line_line(ox[0], oy[0], ox[1], oy[1], dx[1], dy[1], dx[2], dy[2]) ||
                       check_line_line(ox[0], oy[0], ox[1], oy[1], dx[2], dy[2], dx[0], dy[0]) ||
                       check_line_line(ox[1], oy[1], ox[2], oy[2], dx[0], dy[0], dx[1], dy[1]) ||
                       check_line_line(ox[1], oy[1], ox[2], oy[2], dx[2], dy[2], dx[0], dy[0]) ||
                       check_line_line(ox[2], oy[2], ox[0], oy[0], dx[0], dy[0], dx[1], dy[1]) ||
                       check_line_line(ox[2], oy[2], ox[0], oy[0], dx[1], dy[1], dx[2], dy[2]) ||
                       checktriangle(ox[0], oy[0], dx[0], dy[0], dx[1], dy[1], dx[2], dy[2]) ||
                       checktriangle(dx[0], dy[0], ox[0], oy[0], ox[1], oy[1], ox[2], oy[2]);
            }
            break;
        case 2: /* 点 */
            return TRUE; /* ここに来た時点で当たっている */
            break;
        default:
            rb_raise( eDXRubyError, "Internal error" );
        }
    }
    else
    {/* 違う形の比較 */
        if( o_type > d_type )
        {/* oのほうを小さく入れ替え */
            struct DXRubyCollision *ctemp;
            int itemp;
            ctemp = o;
            o = d;
            d = ctemp;
            itemp = o_type;
            o_type = d_type;
            d_type = itemp;
        }

        switch( o_type )
        {
        case 2: /* 点 */
            {
                struct DXRubyCollision *point_collision = o;

                switch( d_type)
                {
                case 3: /* 点と円 */
                    {
                        struct DXRubyCollision *circle_collision = d;
                        float cx, cy, cr;
                        float px, py;
                        float center_x, center_y;
                        center_x = circle_collision->center_x + circle_collision->base_x;
                        center_y = circle_collision->center_y + circle_collision->base_y;

                        rotation_point( point_collision, px, py, point_collision->bx1 + 0.5f, point_collision->by1 + 0.5f );

                        if( circle_collision->rotation_flg ) /* 円が回転している */
                        {   /* 点を円回転中心ベースで回転させる */
                            rotation_point_out( center_x, center_y, -circle_collision->angle, px, py );
                        }

                        cx = NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 0)) + circle_collision->base_x;
                        cy = NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 1)) + circle_collision->base_y;
                        cr = NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 2));

                        if( circle_collision->scaling_flg ) /* 円側が変形している */
                        {   /* 円が真円になるように点の座標を変形させる */
                            px = (px - center_x) / circle_collision->scale_x + center_x;
                            py = (py - center_y) / circle_collision->scale_y + center_y;
                        }

                        return check_circle_point( cx, cy, cr, px, py );
                    }
                    break;
                case 4: /* 点と矩形 */
                    {
                        struct DXRubyCollision *box_collision = d;

                        if( box_collision->rotation_flg || box_collision->scaling_flg ) /* 矩形が回転している */
                        {/* 点を矩形中心に回転して比較する */
                            float px[4], py[4];
                            float bx[4], by[4];
                            struct DXRubyCollision p_collision, b_collision;
                            float centerx, centery;

                            px[0] = px[3] = (float)point_collision->x1;
                            py[0] = py[1] = (float)point_collision->y1;
                            px[1] = px[2] = (float)point_collision->x2;
                            py[2] = py[3] = (float)point_collision->y2;

                            /* 矩形側変形中心点 */
                            centerx = box_collision->center_x + box_collision->base_x;
                            centery = box_collision->center_y + box_collision->base_y;

                            /* 矩形側変形中心点を中心に点側回転 */
                            rotation_box_out( centerx, centery, -box_collision->angle, px, py )

                            /* 点側境界ボリューム作成 */
                            volume_box( 4, px, py, &p_collision );

                            /* 矩形側の拡大・縮小 */
                            scaling_box( box_collision, bx, by );

                            /* 矩形側境界ボリューム作成 */
                            volume_box( 4, bx, by, &b_collision );

                            if( !check_box_box( &p_collision, &b_collision ) )
                            {
                                return FALSE; /* 当たっていなかった */
                            }
                        }
                        return TRUE; /* ここに来た時点で当たっている */
                    }
                    break;
                case 6: /* 点と三角 */
                    {
                        struct DXRubyCollision *tri_collision = d;
                        float x[3], y[3], tri_x[3], tri_y[3]; /* 三角座標 */

                        if( tri_collision->rotation_flg || tri_collision->scaling_flg ) /* 三角側が回転・変形している */
                        {
                            /* 三角側を自分の姿勢に回転・変形 */
                            x[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 0)) + 0.5f;
                            y[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 1)) + 0.5f;
                            x[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 2)) + 0.5f;
                            y[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 3)) + 0.5f;
                            x[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 4)) + 0.5f;
                            y[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 5)) + 0.5f;

                            rotation_triangle( tri_collision, x, y, tri_x, tri_y );
                        }
                        else /* 三角側は回転・変形していなかった */
                        {
                            tri_x[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 0)) + 0.5f + tri_collision->base_x;
                            tri_y[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 1)) + 0.5f + tri_collision->base_y;
                            tri_x[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 2)) + 0.5f + tri_collision->base_x;
                            tri_y[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 3)) + 0.5f + tri_collision->base_y;
                            tri_x[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 4)) + 0.5f + tri_collision->base_x;
                            tri_y[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 5)) + 0.5f + tri_collision->base_y;
                        }
                        return checktriangle(o->x1 + 0.5f, o->y1 + 0.5f, tri_x[0], tri_y[0], tri_x[1], tri_y[1], tri_x[2], tri_y[2]);
                    }
                    break;
                default:
                    rb_raise( eDXRubyError, "Internal error" );
                    break;
                }
                rb_raise( eDXRubyError, "Internal error" );
            }
            break;

        case 3: /* 円 */
            {
                struct DXRubyCollision *circle_collision = o;

                switch( d_type )
                {
                case 4: /* 円と矩形 */
                    {
                        struct DXRubyCollision *box_collision = d;
                        float box_x[4], box_y[4]; /* 矩形座標 */
                        float circle_x, circle_y, circle_r;
                        float center_x, center_y;

                        if( !circle_collision->scaling_flg || circle_collision->scale_x == circle_collision->scale_y ) /* 円が変形していない、もしくは縦横同率 */
                        {
                            if( box_collision->scaling_flg ) /* 矩形側が変形している */
                            {   /* 変形させる */
                                scaling_box( box_collision, box_x, box_y );
                                if( box_collision->scale_x < 0 )
                                {
                                    float temp;
                                    temp = box_x[0];
                                    box_x[0] = box_x[1];
                                    box_x[1] = temp;
                                    temp = box_x[2];
                                    box_x[2] = box_x[3];
                                    box_x[3] = temp;
                                    temp = box_y[0];
                                    box_y[0] = box_y[1];
                                    box_y[1] = temp;
                                    temp = box_y[2];
                                    box_y[2] = box_y[3];
                                    box_y[3] = temp;
                                }
                                if( box_collision->scale_y < 0 )
                                {
                                    float temp;
                                    temp = box_x[0];
                                    box_x[0] = box_x[3];
                                    box_x[3] = temp;
                                    temp = box_x[2];
                                    box_x[2] = box_x[1];
                                    box_x[1] = temp;
                                    temp = box_y[0];
                                    box_y[0] = box_y[3];
                                    box_y[3] = temp;
                                    temp = box_y[2];
                                    box_y[2] = box_y[1];
                                    box_y[1] = temp;
                                }
                            }
                            else
                            {
                                box_x[0] = box_x[3] = box_collision->bx1 + box_collision->base_x;
                                box_y[0] = box_y[1] = box_collision->by1 + box_collision->base_y;
                                box_x[1] = box_x[2] = box_collision->bx2 + box_collision->base_x;
                                box_y[2] = box_y[3] = box_collision->by2 + box_collision->base_y;
                            }

                            if( circle_collision->rotation_flg || circle_collision->scaling_flg ) /* 円が回転している */
                            {
                                /* 円を自分の中心ベースで回転させる */
                                rotation_point( circle_collision, circle_x, circle_y, NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 0)), NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 1)) );
                            }
                            else
                            {
                                circle_x = NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 0)) + circle_collision->base_x;
                                circle_y = NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 1)) + circle_collision->base_y;
                            }

                            if( box_collision->rotation_flg ) /* 矩形側が回転している */
                            {   /* 円の中心点を矩形中心ベースで回転させる */
                                rotation_point_out( box_collision->center_x + box_collision->base_x, box_collision->center_y + box_collision->base_y, -box_collision->angle, circle_x, circle_y );
                            }

                            circle_r = NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 2)) * circle_collision->scale_x;

                            /* あとは円と回転していない矩形の判定でいける。 */
                            return check_point_box(circle_x, circle_y, box_x[0] - circle_r, box_y[0], box_x[2] + circle_r, box_y[2]) ||
                                   check_point_box(circle_x, circle_y, box_x[0], box_y[0] - circle_r, box_x[2], box_y[2] + circle_r) ||
                                   check_circle_point(box_x[0], box_y[0], circle_r, circle_x, circle_y) ||
                                   check_circle_point(box_x[1], box_y[1], circle_r, circle_x, circle_y) ||
                                   check_circle_point(box_x[2], box_y[2], circle_r, circle_x, circle_y) ||
                                   check_circle_point(box_x[3], box_y[3], circle_r, circle_x, circle_y);
                        }

                        if( box_collision->rotation_flg || box_collision->scaling_flg ) /* 矩形側が回転・変形している */
                        {
                            /* 矩形側を自分の姿勢に回転・変形 */
                            rotation_box( box_collision, box_x, box_y );
                        }
                        else /* 矩形側は回転・変形していなかった */
                        {
                            box_x[0] = box_x[3] = (float)box_collision->x1;
                            box_y[0] = box_y[1] = (float)box_collision->y1;
                            box_x[1] = box_x[2] = (float)box_collision->x2;
                            box_y[2] = box_y[3] = (float)box_collision->y2;
                        }

                        center_x = circle_collision->center_x + circle_collision->base_x;
                        center_y = circle_collision->center_y + circle_collision->base_y;

                        if( circle_collision->rotation_flg ) /* 円が回転している */
                        {
                            /* 円中心点を中心に矩形回転 */
                            rotation_box_out( center_x, center_y, -circle_collision->angle, box_x, box_y )
                        }
                        circle_r = NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 2));

                        circle_x = circle_collision->base_x + NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 0));
                        circle_y = circle_collision->base_y + NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 1));
                        if( circle_collision->scaling_flg ) /* 円側が変形している */
                        {   /* 円が真円になるように矩形を変形させる */
                            box_x[0] = (box_x[0] - center_x) / circle_collision->scale_x + center_x;
                            box_y[0] = (box_y[0] - center_y) / circle_collision->scale_y + center_y;
                            box_x[1] = (box_x[1] - center_x) / circle_collision->scale_x + center_x;
                            box_y[1] = (box_y[1] - center_y) / circle_collision->scale_y + center_y;
                            box_x[2] = (box_x[2] - center_x) / circle_collision->scale_x + center_x;
                            box_y[2] = (box_y[2] - center_y) / circle_collision->scale_y + center_y;
                            box_x[3] = (box_x[3] - center_x) / circle_collision->scale_x + center_x;
                            box_y[3] = (box_y[3] - center_y) / circle_collision->scale_y + center_y;
                        }

                        /* 判定 */
                        return checktriangle(circle_x, circle_y, box_x[0], box_y[0], box_x[1], box_y[1], box_x[2], box_y[2]) || 
                               checktriangle(circle_x, circle_y, box_x[0], box_y[0], box_x[2], box_y[2], box_x[3], box_y[3]) || 
                               checkCircleLine(circle_x, circle_y, circle_r, box_x[0], box_y[0], box_x[1], box_y[1]) ||
                               checkCircleLine(circle_x, circle_y, circle_r, box_x[1], box_y[1], box_x[2], box_y[2]) ||
                               checkCircleLine(circle_x, circle_y, circle_r, box_x[2], box_y[2], box_x[3], box_y[3]) ||
                               checkCircleLine(circle_x, circle_y, circle_r, box_x[3], box_y[3], box_x[0], box_y[0]);
                    }
                    break;
                case 6: /* 円と三角 */
                    {
                        struct DXRubyCollision *tri_collision = d;
                        float x[3], y[3], tri_x[3], tri_y[3]; /* 三角座標 */
                        float circle_x, circle_y, circle_r;
                        float center_x, center_y;

                        if( tri_collision->rotation_flg || tri_collision->scaling_flg ) /* 三角側が回転・変形している */
                        {
                            /* 三角側を自分の姿勢に回転・変形 */
                            x[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 0)) + 0.5f;
                            y[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 1)) + 0.5f;
                            x[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 2)) + 0.5f;
                            y[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 3)) + 0.5f;
                            x[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 4)) + 0.5f;
                            y[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 5)) + 0.5f;

                            rotation_triangle( tri_collision, x, y, tri_x, tri_y );
                        }
                        else /* 三角側は回転・変形していなかった */
                        {
                            tri_x[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 0)) + 0.5f + tri_collision->base_x;
                            tri_y[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 1)) + 0.5f + tri_collision->base_y;
                            tri_x[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 2)) + 0.5f + tri_collision->base_x;
                            tri_y[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 3)) + 0.5f + tri_collision->base_y;
                            tri_x[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 4)) + 0.5f + tri_collision->base_x;
                            tri_y[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 5)) + 0.5f + tri_collision->base_y;
                        }

                        /* 円の回転中心 */
                        center_x = circle_collision->center_x + circle_collision->base_x;
                        center_y = circle_collision->center_y + circle_collision->base_y;

                        if( circle_collision->rotation_flg ) /* 円が回転している */
                        {
                            /* 円中心点を中心に三角回転 */
                            rotation_triangle_out( center_x, center_y, -circle_collision->angle, tri_x, tri_y )
                        }
                        circle_r = NUM2FLOAT(RARRAY_AREF(circle_collision->vcollision, 2));

                        circle_x = circle_collision->base_x + NUM2INT(RARRAY_AREF(circle_collision->vcollision, 0));
                        circle_y = circle_collision->base_y + NUM2INT(RARRAY_AREF(circle_collision->vcollision, 1));
                        if( circle_collision->scaling_flg ) /* 円側が変形している */
                        {   /* 円が真円になるように三角を変形させる */
                            tri_x[0] = (tri_x[0] - center_x) / circle_collision->scale_x + center_x;
                            tri_y[0] = (tri_y[0] - center_y) / circle_collision->scale_y + center_y;
                            tri_x[1] = (tri_x[1] - center_x) / circle_collision->scale_x + center_x;
                            tri_y[1] = (tri_y[1] - center_y) / circle_collision->scale_y + center_y;
                            tri_x[2] = (tri_x[2] - center_x) / circle_collision->scale_x + center_x;
                            tri_y[2] = (tri_y[2] - center_y) / circle_collision->scale_y + center_y;
                        }

                        /* 判定 */
                        return checktriangle(circle_x, circle_y, tri_x[0], tri_y[0], tri_x[1], tri_y[1], tri_x[2], tri_y[2]) || 
                               checkCircleLine(circle_x, circle_y, circle_r, tri_x[0], tri_y[0], tri_x[1], tri_y[1]) ||
                               checkCircleLine(circle_x, circle_y, circle_r, tri_x[1], tri_y[1], tri_x[2], tri_y[2]) ||
                               checkCircleLine(circle_x, circle_y, circle_r, tri_x[2], tri_y[2], tri_x[0], tri_y[0]);
                    }
                    break;
                default:
                    rb_raise( eDXRubyError, "Internal error" );
                    break;
                }
            }
        case 4: /* 矩形 */
            {
                struct DXRubyCollision *box_collision = o;

                switch( d_type )
                {
                case 6: /* 矩形と三角 */
                    {
                        struct DXRubyCollision *tri_collision = d;
                        float box_x[4], box_y[4]; /* 矩形座標 */
                        float tri_x[3], tri_y[3]; /* 三角座標 */
                        float x[3], y[3];

                        if( box_collision->rotation_flg || box_collision->scaling_flg ) /* 矩形が回転している */
                        {
                            rotation_box( box_collision, box_x, box_y );
                        }
                        else
                        {
                            box_x[0] = box_x[3] = (float)box_collision->x1;
                            box_y[0] = box_y[1] = (float)box_collision->y1;
                            box_x[1] = box_x[2] = (float)box_collision->x2;
                            box_y[2] = box_y[3] = (float)box_collision->y2;
                        }

                        if( tri_collision->rotation_flg || tri_collision->scaling_flg ) /* 三角が回転している */
                        {
                            x[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 0)) + 0.5f;
                            y[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 1)) + 0.5f;
                            x[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 2)) + 0.5f;
                            y[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 3)) + 0.5f;
                            x[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 4)) + 0.5f;
                            y[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 5)) + 0.5f;

                            rotation_triangle( d, x, y, tri_x, tri_y );
                        }
                        else
                        {
                            tri_x[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 0)) + 0.5f + tri_collision->base_x;
                            tri_y[0] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 1)) + 0.5f + tri_collision->base_y;
                            tri_x[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 2)) + 0.5f + tri_collision->base_x;
                            tri_y[1] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 3)) + 0.5f + tri_collision->base_y;
                            tri_x[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 4)) + 0.5f + tri_collision->base_x;
                            tri_y[2] = NUM2INT(RARRAY_AREF(tri_collision->vcollision, 5)) + 0.5f + tri_collision->base_y;
                        }

                        return check_line_line(box_x[0], box_y[0], box_x[1], box_y[1], tri_x[0], tri_y[0], tri_x[1], tri_y[1]) ||
                               check_line_line(box_x[0], box_y[0], box_x[1], box_y[1], tri_x[1], tri_y[1], tri_x[2], tri_y[2]) ||
                               check_line_line(box_x[0], box_y[0], box_x[1], box_y[1], tri_x[2], tri_y[2], tri_x[0], tri_y[0]) ||
                               check_line_line(box_x[1], box_y[1], box_x[2], box_y[2], tri_x[0], tri_y[0], tri_x[1], tri_y[1]) ||
                               check_line_line(box_x[1], box_y[1], box_x[2], box_y[2], tri_x[1], tri_y[1], tri_x[2], tri_y[2]) ||
                               check_line_line(box_x[1], box_y[1], box_x[2], box_y[2], tri_x[2], tri_y[2], tri_x[0], tri_y[0]) ||
                               check_line_line(box_x[2], box_y[2], box_x[3], box_y[3], tri_x[0], tri_y[0], tri_x[1], tri_y[1]) ||
                               check_line_line(box_x[2], box_y[2], box_x[3], box_y[3], tri_x[1], tri_y[1], tri_x[2], tri_y[2]) ||
                               check_line_line(box_x[2], box_y[2], box_x[3], box_y[3], tri_x[2], tri_y[2], tri_x[0], tri_y[0]) ||
                               check_line_line(box_x[3], box_y[3], box_x[0], box_y[0], tri_x[0], tri_y[0], tri_x[1], tri_y[1]) ||
                               check_line_line(box_x[3], box_y[3], box_x[0], box_y[0], tri_x[1], tri_y[1], tri_x[2], tri_y[2]) ||
                               check_line_line(box_x[3], box_y[3], box_x[0], box_y[0], tri_x[2], tri_y[2], tri_x[0], tri_y[0]) ||
                               checktriangle(tri_x[0], tri_y[0], box_x[0], box_y[0], box_x[1], box_y[1], box_x[2], box_y[2]) || 
                               checktriangle(tri_x[0], tri_y[0], box_x[0], box_y[0], box_x[2], box_y[2], box_x[3], box_y[3]) || 
                               checktriangle(box_x[0], box_y[0], tri_x[0], tri_y[0], tri_x[1], tri_y[1], tri_x[2], tri_y[2]);
                    }
                    break;
                default:
                    rb_raise( eDXRubyError, "Internal error" );
                    break;
                }
            }
        default:
            rb_raise( eDXRubyError, "Internal error" );
            break;
        }
        rb_raise( eDXRubyError, "Internal error" );
    }
    rb_raise( eDXRubyError, "Internal error" );
}

/* 衝突判定配列のカウント取得 */
int get_volume_count( VALUE vary )
{
    int p, count = 0;

    for( p = 0; p < RARRAY_LEN(vary); p++ )
    {
        VALUE vsprite = RARRAY_AREF( vary, p );

        if( TYPE(vsprite) == T_ARRAY )
        {
            count += get_volume_count( vsprite );
        }
        else
        {
            count++;
        }
    }

    return count;
}


/* AABB境界ボリューム情報の明細領域 */
static int alloc_volume( int count )
{
    while( g_volume_allocate_count < g_volume_count + count )
    {
        g_volume_allocate_count = g_volume_allocate_count * 3 / 2; /* 1.5倍にする */
        g_volume_pointer = realloc( g_volume_pointer, sizeof( struct DXRubyCollision ) * g_volume_allocate_count );
    }

    g_volume_count += count;

    return g_volume_count - count;
}

/* 配列の衝突判定用AABB境界ボリューム作成。領域は上の関数で確保して渡される */
int make_volume_ary( VALUE vary, struct DXRubyCollisionGroup *group )
{
    int p, count = 0;

    /* 配列の数だけボリューム作成する */
    for( p = 0; p < RARRAY_LEN(vary); p++ )
    {
        int tmp;
        VALUE vsprite = RARRAY_AREF( vary, p );

        if( TYPE(vsprite) == T_ARRAY )
        {
            tmp = make_volume_ary( vsprite, group );
        }
        else
        {
            tmp = make_volume( vsprite, group );
        }
        count += tmp;
        group += tmp;
    }

    return count;
}


/* 単体の衝突判定用AABB境界ボリューム作成 */
int make_volume( VALUE vsprite, struct DXRubyCollisionGroup *group )
{
    struct DXRubySprite *sprite;

    /* Spriteじゃなければ無視 */
    if( !DXRUBY_CHECK( Sprite, vsprite ) )
    {
        return 0;
    }

    /* 衝突判定が有効でない場合は無視 */
    sprite = DXRUBY_GET_STRUCT( Sprite, vsprite );
#ifdef DXRUBY15
    if( !RTEST(sprite->vvisible) || !RTEST(sprite->vcollision_enable) || sprite->vanish )
#else
    if( !RTEST(sprite->vcollision_enable) || sprite->vanish )
#endif
    {
        return 0;
    }

    /* 衝突判定範囲が設定されていなければ1個だけ明細情報を生成する */
    if( sprite->vcollision == Qnil )
    {
        /* collisionもimageも設定されていなければ無視 */
        if( sprite->vimage == Qnil )
        {
            return 0;
        }

        group->vsprite = vsprite;
        group->index = alloc_volume( 1 );
        group->count = 1;
        make_volume_sub( vsprite, sprite->vcollision, g_volume_pointer + group->index );
        group->x1 = (g_volume_pointer + group->index)->x1;
        group->y1 = (g_volume_pointer + group->index)->y1;
        group->x2 = (g_volume_pointer + group->index)->x2;
        group->y2 = (g_volume_pointer + group->index)->y2;

        return 1;
    }

    /* 衝突判定範囲が配列じゃなければ無視 */
    Check_Type( sprite->vcollision, T_ARRAY );
    if( RARRAY_LEN(sprite->vcollision) == 0 )
    {
        return 0;
    }

    /* 衝突判定範囲配列の中に配列が入っていた場合は複数の範囲を用いることができる */
    if( TYPE(RARRAY_AREF(sprite->vcollision, 0)) == T_ARRAY )
    {
        int p2;

        group->vsprite = vsprite;
        group->index = alloc_volume( RARRAY_LEN(sprite->vcollision) );
        group->count = RARRAY_LEN(sprite->vcollision);

        for( p2 = 0; p2 < RARRAY_LEN(sprite->vcollision); p2++ )
        {
            Check_Type( RARRAY_AREF(sprite->vcollision, p2), T_ARRAY );
            make_volume_sub( vsprite, RARRAY_AREF(sprite->vcollision, p2), g_volume_pointer + group->index + p2 );
        }

        /* グループのAABB境界をすべて含む最小のAABB境界を生成する */
        group->x1 = (g_volume_pointer + group->index)->x1;
        group->y1 = (g_volume_pointer + group->index)->y1;
        group->x2 = (g_volume_pointer + group->index)->x2;
        group->y2 = (g_volume_pointer + group->index)->y2;
        for( p2 = 1; p2 < RARRAY_LEN(sprite->vcollision); p2++ )
        {
            if( group->x1 > (g_volume_pointer + group->index + p2)->x1 )
            {
                group->x1 = (g_volume_pointer + group->index + p2)->x1;
            }
            if( group->x2 < (g_volume_pointer + group->index + p2)->x2 )
            {
                group->x2 = (g_volume_pointer + group->index + p2)->x2;
            }
            if( group->y1 > (g_volume_pointer + group->index + p2)->y1 )
            {
                group->y1 = (g_volume_pointer + group->index + p2)->y1;
            }
            if( group->y2 < (g_volume_pointer + group->index + p2)->y2 )
            {
                group->y2 = (g_volume_pointer + group->index + p2)->y2;
            }
        }
        
    }
    else
    { /* 配列じゃない場合 */
        group->vsprite = vsprite;
        group->index = alloc_volume( 1 );
        group->count = 1;
        make_volume_sub( vsprite, sprite->vcollision, g_volume_pointer + group->index );
        group->x1 = (g_volume_pointer + group->index)->x1;
        group->y1 = (g_volume_pointer + group->index)->y1;
        group->x2 = (g_volume_pointer + group->index)->x2;
        group->y2 = (g_volume_pointer + group->index)->y2;
    }

    return 1;
}


/* 衝突判定用AABB境界ボリューム作成sub */
void make_volume_sub( VALUE vsprite, VALUE vcol, struct DXRubyCollision *collision )
{
    struct DXRubySprite *sprite;
    sprite = DXRUBY_GET_STRUCT( Sprite, vsprite );

    collision->vsprite = vsprite;
    collision->base_x = NUM2FLOAT(sprite->vx);
    collision->base_y = NUM2FLOAT(sprite->vy);
    if( RTEST(sprite->voffset_sync) )
    {
        struct DXRubyImage *image;
        if( sprite->vcenter_x == Qnil || sprite->vcenter_y == Qnil )
        {
            DXRUBY_CHECK_IMAGE( sprite->vimage );
            image = DXRUBY_GET_STRUCT( Image, sprite->vimage );
            collision->base_x -= sprite->vcenter_x == Qnil ? image->width / 2 : NUM2FLOAT(sprite->vcenter_x);
            collision->base_y -= sprite->vcenter_y == Qnil ? image->height / 2 : NUM2FLOAT(sprite->vcenter_y);
        }
        else
        {
            collision->base_x -= NUM2FLOAT(sprite->vcenter_x);
            collision->base_y -= NUM2FLOAT(sprite->vcenter_y);
        }
    }
    collision->angle = NUM2FLOAT(sprite->vangle);
    collision->scale_x = NUM2FLOAT(sprite->vscale_x);
    collision->scale_y = NUM2FLOAT(sprite->vscale_y);
    collision->vcollision = vcol;
    if( RTEST(sprite->vcollision_sync) ) /* 回転・スケーリングのフラグ */
    {
        if( collision->angle != 0.0f )
        {
            collision->rotation_flg = TRUE;
        }
        else
        {
            collision->rotation_flg = FALSE;
        }
        if( collision->scale_x != 1.0f || collision->scale_y != 1.0f )
        {
            collision->scaling_flg = TRUE;
        }
        else
        {
            collision->scaling_flg = FALSE;
        }
    }
    else
    {
        collision->rotation_flg = FALSE;
        collision->scaling_flg = FALSE;
    }

    if( vcol != Qnil )
    {
        Check_Type( vcol, T_ARRAY );

        switch (RARRAY_LEN(vcol))
        {
        case 2: /* 点 */
            if( !collision->rotation_flg && !collision->scaling_flg )
            {
                collision->bx1 = NUM2FLOAT(RARRAY_AREF(vcol, 0));
                collision->by1 = NUM2FLOAT(RARRAY_AREF(vcol, 1));
                collision->bx2 = NUM2FLOAT(RARRAY_AREF(vcol, 0)) + 1;
                collision->by2 = NUM2FLOAT(RARRAY_AREF(vcol, 1)) + 1;
                collision->x1 = (int)(collision->base_x + collision->bx1);
                collision->y1 = (int)(collision->base_y + collision->by1);
                collision->x2 = (int)(collision->base_x + collision->bx2);
                collision->y2 = (int)(collision->base_y + collision->by2);
            }
            else /* 回転した点 */
            {
                float tx,ty;
                collision->bx1 = NUM2FLOAT(RARRAY_AREF(vcol, 0));
                collision->by1 = NUM2FLOAT(RARRAY_AREF(vcol, 1));
                collision->bx2 = NUM2FLOAT(RARRAY_AREF(vcol, 0)) + 1;
                collision->by2 = NUM2FLOAT(RARRAY_AREF(vcol, 1)) + 1;

                set_center( sprite, collision );
                rotation_point( collision, tx, ty, collision->bx1 + 0.5f, collision->by1 + 0.5f );

                collision->x1 = (int)tx;
                collision->y1 = (int)ty;
                collision->x2 = (int)tx + 1;
                collision->y2 = (int)ty + 1;
            }
            break;
        case 3: /* 円 */
        {
            float tempx = NUM2FLOAT(RARRAY_AREF(vcol, 0));
            float tempy = NUM2FLOAT(RARRAY_AREF(vcol, 1));
            float tempr = NUM2FLOAT(RARRAY_AREF(vcol, 2));

            if( !collision->rotation_flg && !collision->scaling_flg )
            {
                collision->x1 = (int)(collision->base_x + tempx - tempr);
                collision->y1 = (int)(collision->base_y + tempy - tempr);
                collision->x2 = (int)(collision->base_x + tempx + tempr);
                collision->y2 = (int)(collision->base_y + tempy + tempr);
            }
            else /* 回転した円 */
            {
                float tx,ty;

                if( collision->scale_x != collision->scale_y ) /* 楕円。難しいので暫定として境界ボリュームを回転して境界ボリュームを作る */
                {
                    float x[4], y[4];
                    collision->bx1 = tempx - tempr;
                    collision->by1 = tempy - tempr;
                    collision->bx2 = tempx + tempr;
                    collision->by2 = tempy + tempr;

                    set_center( sprite, collision );

                    rotation_box( collision, x, y );
                    volume_box( 4, x, y, collision );
                    collision->x2++;
                    collision->y2++;
                }
                else /* 真円 */
                {
                    set_center( sprite, collision );
                    rotation_point( collision, tx, ty, tempx, tempy );
                    collision->x1 = (int)(tx - tempr * collision->scale_x);
                    collision->y1 = (int)(ty - tempr * collision->scale_x);
                    collision->x2 = (int)(tx + tempr * collision->scale_x);
                    collision->y2 = (int)(ty + tempr * collision->scale_x);
                }
            }
            break;
        }
        case 4: /* 矩形 */
            if( !collision->rotation_flg && !collision->scaling_flg )
            {
                collision->bx1 = NUM2FLOAT(RARRAY_AREF(vcol, 0));
                collision->by1 = NUM2FLOAT(RARRAY_AREF(vcol, 1));
                collision->bx2 = NUM2FLOAT(RARRAY_AREF(vcol, 2)) + 1;
                collision->by2 = NUM2FLOAT(RARRAY_AREF(vcol, 3)) + 1;
                collision->x1 = (int)(collision->base_x + collision->bx1);
                collision->y1 = (int)(collision->base_y + collision->by1);
                collision->x2 = (int)(collision->base_x + collision->bx2);
                collision->y2 = (int)(collision->base_y + collision->by2);
            }
            else /* 回転した矩形 */
            {
                float tx[4], ty[4];

                collision->bx1 = NUM2FLOAT(RARRAY_AREF(vcol, 0));
                collision->by1 = NUM2FLOAT(RARRAY_AREF(vcol, 1));
                collision->bx2 = NUM2FLOAT(RARRAY_AREF(vcol, 2)) + 1;
                collision->by2 = NUM2FLOAT(RARRAY_AREF(vcol, 3)) + 1;
                set_center( sprite, collision );

                rotation_box( collision, tx, ty );

                volume_box( 4, tx, ty, collision );
                collision->x2++;
                collision->y2++;
            }
            break;
        case 6: /* 三角形 */
        {
            float tx[3], ty[3];
            int i;
            if( !collision->rotation_flg && !collision->scaling_flg )
            {
                tx[0] = collision->base_x + NUM2INT(RARRAY_AREF(vcol, 0)) + 0.5f;
                ty[0] = collision->base_y + NUM2INT(RARRAY_AREF(vcol, 1)) + 0.5f;
                tx[1] = collision->base_x + NUM2INT(RARRAY_AREF(vcol, 2)) + 0.5f;
                ty[1] = collision->base_y + NUM2INT(RARRAY_AREF(vcol, 3)) + 0.5f;
                tx[2] = collision->base_x + NUM2INT(RARRAY_AREF(vcol, 4)) + 0.5f;
                ty[2] = collision->base_y + NUM2INT(RARRAY_AREF(vcol, 5)) + 0.5f;
            }
            else /* 回転した三角形 */
            {
                float x[3], y[3];

                x[0] = NUM2INT(RARRAY_AREF(vcol, 0)) + 0.5f;
                y[0] = NUM2INT(RARRAY_AREF(vcol, 1)) + 0.5f;
                x[1] = NUM2INT(RARRAY_AREF(vcol, 2)) + 0.5f;
                y[1] = NUM2INT(RARRAY_AREF(vcol, 3)) + 0.5f;
                x[2] = NUM2INT(RARRAY_AREF(vcol, 4)) + 0.5f;
                y[2] = NUM2INT(RARRAY_AREF(vcol, 5)) + 0.5f;

                set_center( sprite, collision );

                rotation_triangle( collision, x, y, tx, ty );
            }
            volume_box( 3, tx, ty, collision );
            collision->x2++;
            collision->y2++;
        }
            break;
        default:
            rb_raise( eDXRubyError, "collisionの設定が不正です - Sprite_make_volume" );
            break;
        }
    }
    else /* 衝突判定範囲省略時は画像サイズの矩形とみなす */
    { /* 回転していない矩形 */
        struct DXRubyImage *image;
        DXRUBY_CHECK_IMAGE( sprite->vimage );
        image = DXRUBY_GET_STRUCT( Image, sprite->vimage );
        if( !collision->rotation_flg && !collision->scaling_flg )
        {
            collision->bx1 = 0;
            collision->by1 = 0;
            collision->bx2 = (float)image->width;
            collision->by2 = (float)image->height;
            collision->x1 = (int)collision->base_x;
            collision->y1 = (int)collision->base_y;
            collision->x2 = (int)(collision->base_x + image->width);
            collision->y2 = (int)(collision->base_y + image->height);
        }
        else /* 回転した矩形 */
        {
            float tx[4], ty[4];

            collision->bx1 = 0;
            collision->by1 = 0;
            collision->bx2 = (float)image->width;
            collision->by2 = (float)image->height;
            collision->center_x = sprite->vcenter_x == Qnil ? image->width / 2 : NUM2FLOAT(sprite->vcenter_x);
            collision->center_y = sprite->vcenter_y == Qnil ? image->height / 2 : NUM2FLOAT(sprite->vcenter_y);

            rotation_box( collision, tx, ty );

            volume_box( 4, tx, ty, collision );
            collision->x2++;
            collision->y2++;
        }
    }
}

void collision_init(void)
{
    g_volume_count = 0;
    g_volume_allocate_count = 16;
    g_volume_pointer = malloc( sizeof(struct DXRubyCollision) * 16 );
}

void collision_clear(void)
{
    g_volume_count = 0;
}

