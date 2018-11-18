struct DXRubyCollision
{
    /* 衝突判定用 */
    int x1, y1, x2, y2; /* AABBボリューム */
    VALUE vsprite;
    float bx1, by1, bx2, by2; /* 回転前・相対座標での矩形判定範囲（省略時もここに設定する） */
    float angle, base_x, base_y, scale_x, scale_y, center_x, center_y;
    int rotation_flg, scaling_flg;
    VALUE vcollision;
};

struct DXRubyCollisionGroup
{
    int index;
    VALUE vsprite;
    int count;
    int x1, y1, x2, y2; /* AABBボリューム */
};

int check( struct DXRubyCollisionGroup *o, struct DXRubyCollisionGroup *d );
int check_sub( struct DXRubyCollision *o, struct DXRubyCollision *d );
int make_volume_ary( VALUE vary, struct DXRubyCollisionGroup *collision );
int make_volume( VALUE vsprite, struct DXRubyCollisionGroup *collision );
void make_volume_sub( VALUE vsprite, VALUE vcol, struct DXRubyCollision *collision );
void collision_init(void);
int get_volume_count( VALUE vary );


#define check_box_box(b1, b2) ( (b1)->x1 < (b2)->x2 && \
                                (b1)->y1 < (b2)->y2 && \
                                (b2)->x1 < (b1)->x2 && \
                                (b2)->y1 < (b1)->y2 )

#define check_point_box(x, y, x1, y1, x2, y2) ( (x) >= (x1) && \
                                                (y) >= (y1) && \
                                                (x) < (x2) && \
                                                (y) < (y2))

#define check_circle_circle(ox, oy, or, dx, dy, dr) ((or+dr) * (or+dr) >= (ox-dx) * (ox-dx) + (oy-dy) * (oy-dy))

#define check_circle_point(cx, cy, cr, px, py) ((cr*cr) >= (cx-px) * (cx-px) + (cy-py) * (cy-py))

#define check_line_line(x1, y1, x2, y2, x3, y3, x4, y4) (!((((x1 - x2) * (y3 - y1) + (y1 - y2) * (x1 - x3)) * \
                                                            ((x1 - x2) * (y4 - y1) + (y1 - y2) * (x1 - x4)) > 0.0) || \
                                                           (((x3 - x4) * (y1 - y3) + (y3 - y4) * (x3 - x1)) * \
                                                            ((x3 - x4) * (y2 - y3) + (y3 - y4) * (x3 - x2)) > 0.0 )))

