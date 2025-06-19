#define NOB_STRIP_PREFIX
#include "nob.h"

#include "objParser.h"

typedef struct{
    vec3* items;
    size_t count;
    size_t capacity;
} vec3s;


void wavefrontParse(char* data, Vertices* verticesOut){
    char* current_pos = data;
    while ((current_pos = strchr(current_pos, '\r'))) {
        memmove(current_pos, current_pos + 1, strlen(current_pos + 1) + 1);
    }

    vec3s positions = {0};
    vec3s normals = {0};

    char* line = strtok(data, "\n");
    while (line != NULL) {
        if (line[0] == 'v' && line[1] == ' ') {
            vec3 position;
            if (sscanf(line, "v %f %f %f", &position.x, &position.y, &position.z) == 3) {
                da_append(&positions, position);
            }
        } else if(line[0] == 'v' && line[1] == 'n'){
            vec3 normal;
            if (sscanf(line, "vn %f %f %f", &normal.x, &normal.y, &normal.z) == 3) {
                da_append(&normals, normal);
            }
        }else if (line[0] == 'f' && line[1] == ' ') {
            size_t a, b, c;
            size_t na, nb, nc;
            // Try full format first
            if (sscanf(line, "f %zu/%*zu/%zu %zu/%*zu/%zu %zu/%*zu/%zu", &a, &na, &b, &nb, &c, &nc) == 6 ||
                sscanf(line, "f %zu//%zu %zu//%zu %zu//%zu", &a, &na, &b, &nb, &c, &nc) == 6){
                da_append(verticesOut, ((Vertex){
                    .position = positions.items[a-1],
                    .normal = normals.items[na-1],
                }));
                da_append(verticesOut, ((Vertex){
                    .position = positions.items[c-1],
                    .normal = normals.items[nc-1],
                }));
                da_append(verticesOut, ((Vertex){
                    .position = positions.items[b-1],
                    .normal = normals.items[nb-1],
                }));
            }
        }

        line = strtok(NULL, "\n");
    }

    da_free(positions);
    da_free(normals);

    return;
}