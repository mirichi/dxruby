struct DXRubySprite {
    VALUE vx;
    VALUE vy;
    VALUE vz;
    VALUE vimage;
    VALUE vtarget;
    VALUE vangle;
    VALUE vscale_x;
    VALUE vscale_y;
    VALUE vcenter_x;
    VALUE vcenter_y;
    VALUE valpha;
    VALUE vblend;
    VALUE vvisible;
    VALUE vshader;
    VALUE vcollision;
    VALUE vcollision_enable;
    VALUE vcollision_sync;
#ifdef DXRUBY15
    VALUE vxy;
    VALUE vxyz;
#endif
    VALUE voffset_sync;
    int vanish;
};

void Sprite_release( struct DXRubySprite *sprite );
void Sprite_internal_draw( VALUE self, VALUE vrt );

