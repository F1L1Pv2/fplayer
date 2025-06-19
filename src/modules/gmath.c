#include "gmath.h"

#include "math.h"

mat4 ortho2D(float width, float height){
    float left = -width/2;
    float right = width/2;
    float top = height/2;
    float bottom = -height/2;

    return (mat4){
    2 / (right - left),0                 , 0, -(right + left) / (right - left),
          0           ,2 / (top - bottom), 0, -(top + bottom) / (top - bottom),
          0           ,     0            ,-1,                 0,
          0           ,     0            , 0,                 1,
    };
}

mat4 perspective(float fov, float aspect, float zNear, float zFar){
    float S = 1.0f / tanf(fov*PI/180/2);
    return (mat4){
        S/aspect,  0,             0                 ,     0,
            0   , -S,             0                 ,     0,
            0   ,  0,    -zFar / (zFar - zNear)     ,    -1,
            0   ,  0, -zFar * zNear / (zFar - zNear),     0,
    };
}

mat4 mat4mul(mat4 *a, mat4 *b) {
    mat4 result;

    // Column-major multiplication: result[i][j] = sum_k a[k][j] * b[i][k]
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                // a is row × k, b is k × col
                float a_elem = a->v[k * 4 + row]; // a[row][k]
                float b_elem = b->v[col * 4 + k]; // b[k][col]
                sum += a_elem * b_elem;
            }
            result.v[col * 4 + row] = sum; // result[row][col]
        }
    }

    return result;
}

mat4 mat4transpose(mat4 *a) {
    return (mat4){
        a->v[ 0],a->v[ 4],a->v[ 8],a->v[12],
        a->v[ 1],a->v[ 5],a->v[ 9],a->v[13],
        a->v[ 2],a->v[ 6],a->v[10],a->v[14],
        a->v[ 3],a->v[ 7],a->v[11],a->v[15],
    };
}