/* 行列 */
struct DXRubyMatrix {
    int x;
    int y;
    union {
        struct {
            float m11;float m12; float m13; float m14;
            float m21;float m22; float m23; float m24;
            float m31;float m32; float m33; float m34;
            float m41;float m42; float m43; float m44;
        };
        float m[4][4];
    };
};

/* ベクトル */
struct DXRubyVector {
    int x;
    union {
        struct {
            float v1;float v2; float v3; float v4;
        };
        float v[4];
    };
};

void Init_dxruby_Matrix(void);
VALUE Vector_allocate( VALUE klass );
void Matrix_release( struct DXRubyMatrix* mat );
void Vector_release( struct DXRubyVector* vec );
