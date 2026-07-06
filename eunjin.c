/* gcc eunjin.c glad.c -o eunjin -I./include -L./lib -lSDL2 -lopengl32 -Wall -Werror -std=c99 */

#define SDL_MAIN_HANDLED
#define STB_IMAGE_IMPLEMENTATION
#define TINYOBJ_LOADER_C_IMPLEMENTATION

#include <SDL2/SDL_video.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include "stb_image.h"
#include <tinyobj_loader_c.h>

typedef struct {
    SDL_Window *window;
    SDL_GLContext gl_context;
    int window_width;
    int window_height;
    bool initialized; // fail check
} AppContext;

AppContext app_context_create(int window_width, int window_height);
void app_context_destroy(AppContext *app);

typedef enum {
    AXIS_X,
    AXIS_Y,
    AXIS_Z
} Axis;

typedef struct {
    float data[16];
} Mat4;

void mat4_identity(Mat4 *out);
void mat4_multiply(Mat4 *out, const Mat4 *a, const Mat4 *b);
void mat4_rotate(Mat4 *out, float angle, Axis axis);
void mat4_perspective(Mat4 *out, float fov, float aspect, float near, float far);
void mat4_translate(Mat4 *out, float x, float y, float z);
void mat4_scale(Mat4 *out, float x, float y, float z);
void mat4_compose_trs(Mat4 *out, float pos[3], float rot[3], float scale[3]);

typedef struct {
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
} VertexBinding;

typedef struct {
    GLuint index;
    GLint size;
    GLenum type; // 데이터 타입 추가 (예: GL_FLOAT, GL_UNSIGNED_BYTE)
    //bool normalized;
    //size_t offset;
} AttribDescriptor;

static inline size_t gl_type_size(GLenum type) {
    switch (type) {
        case GL_FLOAT: return sizeof(float);
        case GL_INT: return sizeof(int);
        case GL_UNSIGNED_INT: return sizeof(unsigned int);
        case GL_SHORT: return sizeof(short);
        case GL_UNSIGNED_SHORT: return sizeof(unsigned short);
        case GL_BYTE: return sizeof(char);
        case GL_UNSIGNED_BYTE: return sizeof(unsigned char);
        default: return sizeof(float); // 기본 가드
    }
}

typedef struct {
    GLenum usage;
    AttribDescriptor attribs[16];
    int attrib_count;
    bool has_index;
} BindingDescriptor;

typedef struct {
    void *vertices;     // 🛠️ 제네릭 데이터 처리를 위해 void*로 변경.
    int vertex_count;
    int stride;  // 🛠️ 정점 1개의 바이트 크기를 저장하는 필드
    GLuint *indices;
    int index_count;

    AttribDescriptor attribs[16];
    int attrib_count;
    GLenum usage;

    VertexBinding binding;
    bool uploaded;
} Mesh;

typedef struct {
    void *vertices;
    size_t vertex_bytes;

    GLuint *indices;
    size_t index_bytes;

    int stride;

    AttribDescriptor attribs[16];
    int attrib_count;

    GLenum usage;
} MeshDescriptor;

VertexBinding vertex_binding_create(Mesh *mesh, BindingDescriptor *desc);
void vertex_binding_destroy(VertexBinding *binding);
Mesh *mesh_create(MeshDescriptor *desc);
void mesh_upload(Mesh *mesh);
void mesh_destroy(Mesh **mesh);

void file_load(void *ctx, const char *filename, int is_mtl, const char *obj_filename, char **buf, size_t *len);
Mesh *load_obj_with_tinyobj(const char *filepath, bool has_normal);

typedef enum {
    U_MODEL,
    U_VIEW,
    U_PROJ,
    U_LIGHT_POS,
    U_LIGHT_COLOR,
    U_OBJ_COLOR,
    U_VIEW_POS,
    U_ASPECT,
    U_DIFFUSE_TEX,
    U_SCREEN_TEX,
    U_TIME,
    U_COUNT // 항상 마지막에 뒤서 배열 크기로 씀
} UniformSlot;

// enum 순서와 1:1 매칭되는 문자열 테이블
// 지정 초기화자 방식(Designated Initializer) C99~
//     [1] = "view",  // 1번 인덱스에 쏙
//     [0] = "model", // 0번 인덱스에 쏙
//     [2] = "proj"   // 2번 인덱스에 쏙
// 예시. [U_TIME] = "time"은 U_TIME이 정수 10이므로
// uniform_names[10]에 "time"을 넣으라는 뜻이 됩니다.
// uniform_names[10] = "time"
static const char *uniform_names[U_COUNT] = {
    [U_MODEL]       = "model",
    [U_VIEW]        = "view",
    [U_PROJ]        = "proj",
    [U_LIGHT_POS]   = "lightPos",
    [U_LIGHT_COLOR] = "lightColor",
    [U_OBJ_COLOR]   = "objectColor",
    [U_VIEW_POS]    = "viewPos",
    [U_ASPECT]      = "aspect",
    [U_DIFFUSE_TEX] = "diffuseTexture",
    [U_SCREEN_TEX]  = "screenTexture",
    [U_TIME]        = "time",
};

typedef struct {
    GLuint id;
    GLint locations[U_COUNT]; // GPU 내부에 있는 유니폼 변수들의 위치(Location) 번호를 보관하는 배열
} ShaderProgram;

void shader_program_resolve_uniforms(ShaderProgram *prog);

typedef enum {
    MESH_QUAD,
    MESH_PLANE,
    MESH_MONKEY,
    MESH_SCREEN_QUAD,
    MESH_SPHERE,

    MESH_COUNT
} MeshID;

typedef enum {
    SHADER_TEXTURE,
    SHADER_PHONG,
    SHADER_POST_INVERT,

    SHADER_COUNT
} ShaderID;

typedef enum {
    TEX_NONE,
    TEX_GIRL,
    TEX_BRICK,

    TEX_COUNT
} TextureID;

typedef enum {
    MAT_TEXTURE,
    MAT_GROUND,
    MAT_MONKEY,

    MAT_COUNT
} MaterialID;

typedef struct {
    ShaderProgram *shader;
    GLuint texture;
    float color[3];
} Material;

typedef struct {
    /* view */
    float position[3];
    float target[3];
    float up[3];

    /* proj */
    float fov;
    float aspect;
    float near_plane;
    float far_plane;

    Mat4 view;
    Mat4 proj;

    bool view_dirty;  // position/target/up 바뀌면 true
    bool proj_dirty;  // fov/aspect/near/far 바뀌면 true
} Camera;


static void vec3_sub(float out[3], const float a[3], const float b[3]);
static float vec3_dot(const float a[3], const float b[3]);
static void vec3_cross(float out[3], const float a[3], const float b[3]);
static void vec3_normalize(float v[3]);
void mat4_look_at(Mat4 *out, const float eye[3], const float target[3], const float world_up[3]);
void camera_update_view(Camera *camera);
void camera_update_proj(Camera *camera);
void camera_init(Camera *camera, float aspect);
void camera_set_target(Camera *camera, float px, float py, float pz);
void camera_set_position(Camera *camera, float px, float py, float pz);
void camera_set_aspect(Camera *camera, float aspect);
void camera_ensure_updated(Camera *camera);

typedef struct {
    float pos[3];
    float rot[3];
    float scale[3];

    Mat4 model;
    bool dirty;
} Transform;

void transform_set_rotation(Transform *t, float x, float y, float z);
void transform_ensure_updated(Transform *t);

typedef struct {
    Camera *camera;
    float light_pos[3];
    float light_color[3];
    float time;
} SceneContext;

typedef struct RenderObject RenderObject;
typedef void (*update_callback_function)(RenderObject *obj, SceneContext *ctx, float dt);
typedef void (*destroy_callback_function)(void *user_data);

typedef struct {
    update_callback_function updates[4]; // 필요한 만큼
    int update_count;
    destroy_callback_function destroy;
} ObjectCallbacks;

struct RenderObject {
    Transform transform;
    Mesh *mesh;
    Material *material;

    ObjectCallbacks *callbacks;
    void *user_data;
    // uinit32 componet_mask; // ecs
};

typedef struct {
    float phase;
    float speed;
} BobData;

void object_rotate(RenderObject *obj, SceneContext *ctx, float dt) {
    obj->transform.rot[1] += dt;
    obj->transform.dirty = true;
}

void object_bob(RenderObject *obj, SceneContext *ctx, float dt) {
    BobData *b = obj->user_data;
    b->phase += dt * b->speed;

    //obj->transform.pos[1]= sinf(ctx->time);
    obj->transform.pos[1] = sinf(b->phase);

    obj->transform.dirty = true;
}

void bob_data_destroy(void *user_data) {
    printf("bob_data_destroy was called: %p\n", user_data);
    BobData *b = (BobData *)user_data;
    free(b);
}

void render_object_draw(RenderObject *obj, SceneContext *ctx);



typedef struct {
    GLuint fbo;
    GLuint rbo;
    GLuint color_tex;
} Framebuffer;

Framebuffer framebuffer_create(int width, int height);
void framebuffer_destroy(Framebuffer *fb);

char *shader_source_load(const char *);
GLuint texture_create(const char *);
void texture_destroy(GLuint *);
GLuint shader_program_create(const char *, const char *);
void shader_program_destroy(GLuint);

// todo
// typedef struct {
//     char key[64];      // 예: "monkey", "girl_diffuse"
//     void *resource;    // Mesh* 또는 GLuint(Texture ID) 등
// } AssetEntry;

// typedef struct {
//     AssetEntry meshes[128];
//     int mesh_count;

//     AssetEntry textures[128];
//     int texture_count;
//
//      ...
// } Assets;
//
// void assets_add_mesh(Assets *assets, const char *key, Mesh *mesh);
// Mesh* assets_get_mesh(Assets *assets, const char *key);

typedef struct {
    Mesh* meshes[MESH_COUNT];
    ShaderProgram shaders[SHADER_COUNT];
    Material materials[MAT_COUNT];
    GLuint textures[TEX_COUNT];
} Assets;

void assets_init(Assets *assets);
void assets_destroy(Assets *assets);

typedef enum {
    SCENE_TITLE,
    SCENE_GAMEPLAY,
    SCENE_PAUSE,
    SCENE_COUNT
} SceneID;

typedef struct {
    RenderObject *objects;
    int object_count;
    SceneContext context;
    // int SceneID;
} Scene;

// 팩토리 패턴: SceneID -> SceneDescriptor 생성 함수 매핑
typedef Scene *(*scene_factory_fn)(Assets *assets, Camera *camera);

typedef struct {
    scene_factory_fn factories[SCENE_COUNT];
} SceneRegistry;

typedef enum { TRANSITION_NONE, TRANSITION_FADE_OUT, TRANSITION_FADE_IN } TransitionState;

typedef struct {
    Scene *current;
    SceneID current_id;
    SceneID pending_id;      // 전환 목표
    TransitionState transition;
    float transition_t;      // 0~1 진행도
} SceneManager;

void scene_manager_request_switch(SceneManager *mgr, SceneID next);
void scene_manager_update(SceneManager *mgr, Assets *assets, Camera *camera, float dt);
Scene *scene_manager_current(SceneManager *mgr);

void scene_update(Scene *scene, float dt);

typedef struct {
    MeshID mesh;
    MaterialID material;
    float position[3];
    float rotation[3];
    float scale[3];

    ObjectCallbacks *callbacks;
    void *user_data;
} ObjectDescriptor;

RenderObject render_object_create(Assets *assets, ObjectDescriptor *desc);

typedef struct {
    ObjectDescriptor *objects;
    int object_count;

    float light_pos[3];
    float light_color[3];
} SceneDescriptor;

// camera는 desc에 안 넣고 따로 받음: 창 크기(aspect)에 따라 만들어지는 거라
// "씬 내용" 설계도랑은 성격이 달라서 분리해두는 게 맞음.
Scene *scene_create(Assets *assets, Camera *camera, SceneDescriptor *desc);
void scene_destroy(Scene **scene);


typedef struct {
    Framebuffer offscreen;  // 씬을 먼저 담을 임시 버퍼
    Mesh *screen_quad;      // 화면 전체를 덮는 사각형 메시
    ShaderProgram *post_shader; // 반전, 블러, 그레이스케일 등 후처리 셰이더
    int width, height;
    bool post_enabled;
} Renderer;

typedef struct {
    int width, height;
    ShaderID post_shader;   // enum, Assets에서 찾아옴
    bool use_msa;
    GLenum internal_format;
} RenderDescriptor;

Renderer *renderer_create(Assets *assets, RenderDescriptor *desc);
void renderer_destroy(Renderer *r);
void renderer_resize(Renderer *r, int width, int height);

void renderer_draw_objects(Renderer *r, RenderObject *objects, int count, SceneContext *ctx);
void renderer_present(Renderer *r, int screen_width, int screen_height); // offscreen -> 기본 프레임버퍼로 post_shader 적용해서 blit

typedef struct {
    RenderObject **opaque;
    int opaque_count;

    RenderObject **transparent;
    int transparent_count;
} RenderQueue;

void render_queue_build(RenderQueue *q, Scene *scene, Camera *camera);
void render_queue_sort_transparent(RenderQueue *q, Camera *camera);

int main(int argc, char *argv[]) {
    AppContext app = app_context_create(640, 480);
    if (!app.initialized) {
        return -1;
    }

    Assets assets = {0};
    assets_init(&assets);

    // 🛠️ 카메라는 창 aspect가 필요하니 씬 데이터보다 먼저 만들어둠.
    Camera main_camera;
    camera_init(&main_camera, (float)app.window_width / (float)app.window_height);
    camera_set_position(&main_camera, 0.0f, 1.5f, 5.0f);
    camera_set_target(&main_camera, 0, 0, 0);

    static ObjectCallbacks movement_callback = {
        .updates = { object_rotate, object_bob },
        .update_count = 2,
        .destroy = bob_data_destroy
    };

    /*
     * 기억할 규칙: malloc으로 만드는 user_data는 오브젝트 1개당 1개씩.
     * 콜백은 공유해도 되지만(함수 포인터라 상태가 없으니까),
     * malloc된 데이터는 절대 공유하면 안 돼요.
     *  "콜백은 타입 단위 공유,
     * user_data는 인스턴스 단위"라는 원칙 그대로예요.
     */
    BobData *data1 = malloc(sizeof(BobData));
    data1->phase = 0;
    data1->speed = 2.0f;

    BobData *data2 = malloc(sizeof(BobData));
    data2->phase = 0;
    data2->speed = 3.0f;



    ObjectDescriptor scene1_objects[] = {
        {
            .mesh = MESH_QUAD,
            .material = MAT_TEXTURE,
            .position = {1.0f, 0.0f, 0.0f},
            .rotation = {0.0f, 0.0f, 0.0f},
            .scale = {2.0f, 2.0f, 1.0f},
            .callbacks = &movement_callback,
            .user_data = data1
        },
        {
            .mesh = MESH_PLANE,
            .material = MAT_GROUND,
            .position = {0.0f, -2.5f, 0.0f},
            .rotation = {0.0f, 0.0f, 0.0f},
            .scale = {1.0f, 1.0f, 1.0f},
        },
        {
            .mesh = MESH_MONKEY,
            .material = MAT_MONKEY,
            .position = {-1.0f, 0.0f, 0.0f},
            .rotation = {0.0f, 0.0f, 0.0f},
            .scale = {1.0f, 1.0f, 1.0f},
            .callbacks = &movement_callback,
            .user_data = data2
        },
        {
            .mesh = MESH_SPHERE,
            .material = MAT_MONKEY,
            .position = {0.0f, 0.0f, -5.0f},
            .rotation = {0.0f, 0.0f, 0.0f},
            .scale = {1.0f, 1.0f, 1.0f},
        },
    };

    SceneDescriptor scene1_desc = {
        .objects = scene1_objects,
        .object_count = sizeof(scene1_objects) / sizeof(scene1_objects[0]),
        .light_pos   = {2.0f, 4.0f, 5.0f},
        .light_color = {1.0f, 1.0f, 1.0f},
    };

    Scene *scene1 = scene_create(&assets, &main_camera, &scene1_desc);
    if (!scene1) {
        printf("Scene creation failed\n");
        assets_destroy(&assets);
        app_context_destroy(&app);
        return -1;
    }

    RenderDescriptor render_desc = {
        .width = app.window_width,
        .height = app.window_height,
        .post_shader = SHADER_POST_INVERT,
    };
    Renderer *renderer = renderer_create(&assets, &render_desc);
    renderer->post_enabled = true; // 시작 상태

    glViewport(0, 0, app.window_width, app.window_height);
    glEnable(GL_DEPTH_TEST);

    bool running = true;
    SDL_Event event;
    float delta_time = 0.033f; // 고정 dt 예시 (실제로는 매 프레임 계산)

    while(running) {
        while(SDL_PollEvent(&event)) {
            if(event.type == SDL_QUIT) {
                running = false;
            } else if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                app.window_width = event.window.data1;
                app.window_height = event.window.data2;
                glViewport(0, 0, app.window_width, app.window_height);
                camera_set_aspect(&main_camera, (float)app.window_width / (float)app.window_height);
                renderer_resize(renderer, app.window_width, app.window_height);
            } else if(event.type == SDL_KEYDOWN) {
                if(event.key.keysym.sym == SDLK_SPACE) {
                    renderer->post_enabled = !renderer->post_enabled;
                    printf("post effect: %s\n", renderer->post_enabled ? "ON" : "OFF");
                }
            }
        }

        scene_update(scene1, delta_time);

        renderer_draw_objects(renderer, scene1->objects, scene1->object_count, &scene1->context);
        renderer_present(renderer, app.window_width, app.window_height);

        SDL_GL_SwapWindow(app.window);
        SDL_Delay(33);
    }

    renderer_destroy(renderer);
    scene_destroy(&scene1);
    assets_destroy(&assets);
    app_context_destroy(&app);

    return 0;
}

AppContext app_context_create(int window_width, int window_height) {
    AppContext app = {NULL, NULL, window_width, window_height, false};

    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL initialization failed: %s", SDL_GetError());
        return app; // 여긴 정리할 게 아직 없으니 goto 없이 바로 반환
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    app.window = SDL_CreateWindow(
        "Eunjin",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        app.window_width, window_height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    if(!app.window) goto fail_window;

    app.gl_context = SDL_GL_CreateContext(app.window);
    if(!app.gl_context) {
        printf("OpenGL context creation failed: %s", SDL_GetError());
        goto fail_context;
    }

    if( !gladLoadGLLoader( (GLADloadproc)SDL_GL_GetProcAddress) ) {
        printf("GLAD Initialization failed\n");
        goto fail_glad;
    }

    app.initialized = true;
    return app;

    // 🛠️ 위에서 만든 순서의 역순으로 정리.
    //    각 라벨은 자기보다 위 단계 것까지 전부 해제하고 아래로 흘러 내려감(fall-through).
fail_glad:
    SDL_GL_DeleteContext(app.gl_context);
    app.gl_context = NULL;
fail_context:
    SDL_DestroyWindow(app.window);
    app.window = NULL;
fail_window:
    SDL_Quit();
    return app;
}

void app_context_destroy(AppContext *app)
{
    if (!app) return;

    if (app->gl_context) {
        SDL_GL_DeleteContext(app->gl_context);
        app->gl_context = NULL;
    }

    if (app->window) {
        SDL_DestroyWindow(app->window);
        app->window = NULL;
    }

    SDL_Quit();
}

void mat4_identity(Mat4 *m) {
    memset(m->data, 0, sizeof(Mat4));
    m->data[0] = m->data[5] = m->data[10]  = m->data[15] = 1.0f;
}

void mat4_multiply(Mat4 *out, const Mat4 *a, const Mat4 *b) {
    Mat4 tmp = {0};

    #pragma GCC unroll 4  /*  GCC/Clang에서 루프를 풀어내도록 유도 */
    for(int col = 0; col < 4; col++) {
        #pragma GCC unroll 4
        for(int row = 0; row < 4; row++) {
            #pragma GCC unroll 4
            for(int k = 0; k < 4; k++) {
                tmp.data[col * 4 + row] += a->data[k * 4 + row] * b->data[col * 4 + k];
            }
        }
    }

    *out = tmp;
}

void mat4_rotate(Mat4 *out, float angle, Axis axis) {
    float c = cosf(angle), s = sinf(angle);
    mat4_identity(out);

    switch(axis) {
        case AXIS_X : out->data[5] = c; out->data[6] = s; out->data[9] = -s; out->data[10] = c; break;
        case AXIS_Y : out->data[0] = c; out->data[2] = -s; out->data[8] = s; out->data[10] = c; break;
        case AXIS_Z : out->data[0] = c; out->data[1] = s; out->data[4] = -s; out->data[5] = c; break;
    }
}

void mat4_perspective(Mat4 *out, float fov, float aspect, float near, float far) {
    memset(out, 0, sizeof(Mat4));
    float f = 1.0f / tanf(fov * 0.5f);
    out->data[0] = f / aspect;
    out->data[5] = f;
    out->data[10] = (far + near) / (near - far);
    out->data[11] = -1.0f;
    out->data[14] = (2.0f * far * near) / (near - far);
}

void mat4_translate(Mat4 *out, float x, float y, float z) {
    mat4_identity(out);
    out->data[12] = x;
    out->data[13] = y;
    out->data[14] = z;
}

void mat4_scale(Mat4 *out, float x, float y, float z) {
    mat4_identity(out);
    out->data[0] = x;
    out->data[5] = y;
    out->data[10] = z;
}

void mat4_compose_trs(Mat4 *out, float pos[3], float rot[3], float scale[3]) {
    Mat4 t, r, s;
    Mat4 rx, ry, rz;
    Mat4 tmp;

    mat4_translate(&t, pos[0], pos[1], pos[2]);
    mat4_rotate(&rx, rot[0], AXIS_X);
    mat4_rotate(&ry, rot[1], AXIS_Y);
    mat4_rotate(&rz, rot[2], AXIS_Z);
    mat4_scale(&s, scale[0], scale[1], scale[2]);

    // r = rz * ry * rx
    mat4_multiply(&tmp, &rz, &ry);
    mat4_multiply(&r, &tmp, &rx);

    // out = t * r * s
    mat4_multiply(&tmp, &r, &s);
    mat4_multiply(out, &t, &tmp);
}

void file_load(void *ctx, const char *filename, int is_mtl, const char *obj_filename, char **buf, size_t *len) {
    FILE *f = fopen(filename, "rb");
    if(!f) { *buf = NULL; *len = 0; return; }

    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    rewind(f);

    *buf = (char *)malloc(file_size + 1);
    size_t read_bytes = fread(*buf, 1, file_size, f);
    (*buf)[read_bytes] = '\0';
    *len = read_bytes;

    fclose(f);
}

Mesh *load_obj_with_tinyobj(const char *filepath, bool has_normal) {
    // 1. Mesh 구조체 동적 할당
    Mesh *new_data = calloc(1, sizeof(Mesh));
    if (!new_data) return NULL;

    new_data->vertex_count = 0;
    new_data->vertices = NULL;
    new_data->indices = NULL;
    new_data->index_count = 0;

    new_data->attrib_count = has_normal ? 3 : 2;
    new_data->attribs[0] = (AttribDescriptor){0, 3, GL_FLOAT}; // pos
    if (has_normal) {
        new_data->attribs[1] = (AttribDescriptor){1, 3, GL_FLOAT}; // normal
        new_data->attribs[2] = (AttribDescriptor){2, 2, GL_FLOAT}; // uv
    } else {
        new_data->attribs[1] = (AttribDescriptor){1, 2, GL_FLOAT}; // uv
    }
    new_data->usage = GL_STATIC_DRAW;
    new_data->uploaded = false;

    tinyobj_attrib_t attrib;
    tinyobj_shape_t* shapes = NULL;
    size_t num_shapes;
    tinyobj_material_t* materials = NULL;
    size_t num_materials;

    // OBJ 파일 파싱
    int result = tinyobj_parse_obj(&attrib, &shapes, &num_shapes, &materials, &num_materials,
                                   filepath, file_load, NULL, TINYOBJ_FLAG_TRIANGULATE);

    if (result != TINYOBJ_SUCCESS) {
        printf("OBJ file not found or failed to parse: %s\n", filepath);
        free(new_data);
        return NULL;
    }

    // face_num_verts 배열 직접 합산하여 전체 정점 수 계산
    size_t total_num_verts = 0;
    for (size_t i = 0; i < attrib.num_face_num_verts; i++) {
        total_num_verts += attrib.face_num_verts[i];
    }

    if (total_num_verts == 0) {
        tinyobj_attrib_free(&attrib);
        free(new_data);
        return NULL;
    }

    // 옵션에 따른 레이아웃 크기 설정 (pos3+norm3+uv2 = 8개 / pos3+uv2 = 5개)
    int floats_per_vert = has_normal ? 8 : 5;
    int vert_size = floats_per_vert * sizeof(float);

    float *vertex_buffer = (float *)malloc(vert_size * total_num_verts);
    if (!vertex_buffer) {
        printf("Memory allocation failed!\n");
        tinyobj_attrib_free(&attrib);
        free(new_data);
        return NULL;
    }

    int v_idx_out = 0; // vertex_buffer를 플랫하게 채울 인덱스 카운터
    size_t face_offset = 0;

    // 데이터 조립 (각 shape -> 각 face -> 각 vertex 순회)
    for (size_t i = 0; i < num_shapes; i++) {
        for (size_t f = 0; f < shapes[i].length; f++) {
            size_t current_face_idx = shapes[i].face_offset + f;
            int num_verts_in_face = attrib.face_num_verts[current_face_idx];

            for (int v = 0; v < num_verts_in_face; v++) {
                tinyobj_vertex_index_t idx = attrib.faces[face_offset + v];

                // 1. Position 복사 (x, y, z)
                if (idx.v_idx >= 0) {
                    vertex_buffer[v_idx_out++] = attrib.vertices[3 * idx.v_idx + 0];
                    vertex_buffer[v_idx_out++] = attrib.vertices[3 * idx.v_idx + 1];
                    vertex_buffer[v_idx_out++] = attrib.vertices[3 * idx.v_idx + 2];
                } else {
                    vertex_buffer[v_idx_out++] = 0.0f;
                    vertex_buffer[v_idx_out++] = 0.0f;
                    vertex_buffer[v_idx_out++] = 0.0f;
                }

                // 2. Normal 복사 (x, y, z) - 옵션
                if (has_normal) {
                    if (idx.vn_idx >= 0) {
                        vertex_buffer[v_idx_out++] = attrib.normals[3 * idx.vn_idx + 0];
                        vertex_buffer[v_idx_out++] = attrib.normals[3 * idx.vn_idx + 1];
                        vertex_buffer[v_idx_out++] = attrib.normals[3 * idx.vn_idx + 2];
                    } else {
                        // 기본 법선 (위쪽 방향)
                        vertex_buffer[v_idx_out++] = 0.0f;
                        vertex_buffer[v_idx_out++] = 1.0f;
                        vertex_buffer[v_idx_out++] = 0.0f;
                    }
                }

                // 3. UV (Texcoord) 복사 (u, v)
                if (idx.vt_idx >= 0) {
                    vertex_buffer[v_idx_out++] = attrib.texcoords[2 * idx.vt_idx + 0];
                    vertex_buffer[v_idx_out++] = attrib.texcoords[2 * idx.vt_idx + 1];
                } else {
                    vertex_buffer[v_idx_out++] = 0.0f;
                    vertex_buffer[v_idx_out++] = 0.0f;
                }
            }
            face_offset += num_verts_in_face;
        }
    }

    // Tinyobj 내부 임시 메모리 해제
    tinyobj_attrib_free(&attrib);
    if (shapes) tinyobj_shapes_free(shapes, num_shapes);
    if (materials) tinyobj_materials_free(materials, num_materials);

    // Mesh 구조체 데이터 할당 완료
    new_data->vertices = vertex_buffer;
    new_data->vertex_count = v_idx_out / floats_per_vert; // ⭐ float 개수가 아닌 '정점 개수'로 저장!
    new_data->stride = vert_size;                    // stride 크기 보존

    return new_data;
}

Mesh *mesh_create(MeshDescriptor *desc) {
    // 🛠️ calloc으로 시작 -> vertices/indices가 처음부터 NULL이라
    //    중간에 실패해도 free(NULL)이 안전하게 아무것도 안 함.
    Mesh *mesh = (Mesh *)calloc(1, sizeof(Mesh));
    if(!mesh) return NULL;

    mesh->stride  = desc->stride;
    mesh->vertex_count = desc->vertex_bytes / desc->stride;
    mesh->index_count  = desc->index_bytes / sizeof(GLuint);
    mesh->uploaded     = false;

    mesh->vertices = malloc(desc->vertex_bytes);
    if(!mesh->vertices) goto fail; // vertex 데이터는 필수라 실패 시 바로 정리

    if(desc->vertices) {
        memcpy(mesh->vertices, desc->vertices, desc->vertex_bytes);
    }

    if(desc->index_bytes > 0 && desc->indices) {
        mesh->indices = (GLuint *)malloc(desc->index_bytes);
        if(!mesh->indices) goto fail; // ⭐ 여기서 mesh/mesh->vertices 누수되던 부분
        memcpy(mesh->indices, desc->indices, desc->index_bytes);
    }
    // else: calloc으로 만들었으므로 mesh->indices는 이미 NULL

    mesh->attrib_count = desc->attrib_count;
    memcpy(mesh->attribs, desc->attribs, sizeof(AttribDescriptor) * desc->attrib_count);
    mesh->usage = desc->usage;

    return mesh;

fail:
    free(mesh->vertices);
    free(mesh->indices);
    free(mesh);
    return NULL;
}

void mesh_upload(Mesh *mesh) {
    if (mesh->uploaded) return; // 중복 업로드 방지

    BindingDescriptor desc = {
        .usage = mesh->usage,
        .has_index = mesh->index_count > 0,
        .attrib_count = mesh->attrib_count,
    };
    memcpy(desc.attribs, mesh->attribs, sizeof(AttribDescriptor) * mesh->attrib_count);

    mesh->binding = vertex_binding_create(mesh, &desc);
    mesh->uploaded = true;
}

void mesh_destroy(Mesh **mesh) {
    if(mesh == NULL || *mesh == NULL) return;

    if((*mesh)->uploaded) {
        vertex_binding_destroy(&(*mesh)->binding);
    }
    if((*mesh)->vertices) free((*mesh)->vertices);
    if((*mesh)->indices) free((*mesh)->indices);

    free(*mesh);
    *mesh = NULL;
}

void shader_program_resolve_uniforms(ShaderProgram *prog) {
    for (int i = 0; i < U_COUNT; i++) {
        prog->locations[i] = glGetUniformLocation(prog->id, uniform_names[i]);
        // 셰이더에 그 uniform이 없으면 -1이 들어감. 정상 동작.
    }
}

VertexBinding vertex_binding_create(Mesh *mesh, BindingDescriptor *desc) {
    GLuint vao, vbo;
    GLuint ebo = 0;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);

    // 1. VBO 바인딩
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    // 2. 내부 속성 정의를 바탕으로 정확한 Stride 자동 계산
    int calculated_stride = 0;
    for(int i = 0; i < desc->attrib_count; i++) {
        calculated_stride += desc->attribs[i].size * gl_type_size(desc->attribs[i].type);
    }

    // 3. [교차 검증] 외부에서 넘겨준 m->stride와 내부 계산 값이 일치하는지 체크!
    // 만약 assets_init() 등에서 구조체 정의를 잘못 적었다면 여기서 즉시 크래시가 나며 원인을 알려줍니다.
    assert(mesh->stride == calculated_stride && "Mesh stride specification mismatch!");

    // 데이터 주입 (검증되었으므로 m->stride나 calculated_stride 아무거나 써도 안전합니다)
    glBufferData(GL_ARRAY_BUFFER, mesh->vertex_count * mesh->stride, mesh->vertices, desc->usage);

    // 4. 인덱스가 있을 때만 EBO 생성
    if(desc->has_index) {
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->index_count * sizeof(GLuint), mesh->indices, desc->usage);
    }

    // 5. 오프셋 누적 및 속성 포인터 지정
    size_t offset = 0;
    for(int i = 0; i < desc->attrib_count; i++) {
        glVertexAttribPointer(
            desc->attribs[i].index,
            desc->attribs[i].size,
            desc->attribs[i].type,
            desc->attribs[i].type == GL_UNSIGNED_BYTE ? GL_TRUE : GL_FALSE,
            mesh->stride, // 검증된 Stride 적용
            (void *)(uintptr_t)offset
        );
        glEnableVertexAttribArray(desc->attribs[i].index);

        // 다음 속성을 위해 바이트 크기만큼 오프셋 누적
        offset += desc->attribs[i].size * gl_type_size(desc->attribs[i].type);
    }

    glBindVertexArray(0);

    return (VertexBinding){vao, vbo, ebo};
}

void vertex_binding_destroy(VertexBinding *binding) {
    glDeleteVertexArrays(1, &binding->vao);
    glDeleteBuffers(1, &binding->vbo);
    if(binding->ebo != 0)
        glDeleteBuffers(1, &binding->ebo);
}

void render_object_draw(RenderObject *obj, SceneContext *ctx) {
    transform_ensure_updated(&obj->transform);

    ShaderProgram *prog = obj->material->shader;
    glUseProgram(prog->id); // obj->material->shader->id

    glUniformMatrix4fv(prog->locations[U_VIEW], 1, GL_FALSE, ctx->camera->view.data);
    glUniformMatrix4fv(prog->locations[U_PROJ], 1, GL_FALSE, ctx->camera->proj.data);
    glUniformMatrix4fv(prog->locations[U_MODEL], 1, GL_FALSE, obj->transform.model.data);

    if(prog->locations[U_OBJ_COLOR] != -1)
        glUniform3fv(prog->locations[U_OBJ_COLOR], 1, obj->material->color);

    if(prog->locations[U_LIGHT_POS] != -1)
        glUniform3fv(prog->locations[U_LIGHT_POS], 1, ctx->light_pos);

    if(prog->locations[U_VIEW_POS] != -1)
        glUniform3fv(prog->locations[U_VIEW_POS], 1, ctx->camera->position);

    if(prog->locations[U_LIGHT_COLOR] != -1)
        glUniform3fv(prog->locations[U_LIGHT_COLOR], 1, ctx->light_color);

    if(prog->locations[U_ASPECT] != -1)
        glUniform1f(prog->locations[U_ASPECT], ctx->camera->aspect);

    if(prog->locations[U_TIME] != -1)
        glUniform1f(prog->locations[U_TIME], ctx->time);

    if(obj->material->texture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, obj->material->texture);
        if(prog->locations[U_DIFFUSE_TEX] != -1)
            glUniform1i(prog->locations[U_DIFFUSE_TEX], 0);
    } else {
        // 💡 텍스처가 없는 오브젝트라면 파이프라인을 깨끗하게 비워줍니다.
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glBindVertexArray(obj->mesh->binding.vao);
    if(obj->mesh->binding.ebo != 0)
        glDrawElements(GL_TRIANGLES, obj->mesh->index_count, GL_UNSIGNED_INT, 0);
    else
        glDrawArrays(GL_TRIANGLES, 0, obj->mesh->vertex_count);
    glBindVertexArray(0);
}

void renderer_draw_objects(Renderer *r, RenderObject *objects, int count, SceneContext *ctx) {
    glBindFramebuffer(GL_FRAMEBUFFER, r->offscreen.fbo);
    glViewport(0, 0, r->width, r->height);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (int i = 0; i < count; i++) {
        render_object_draw(&objects[i], ctx);
    }
}

void renderer_present(Renderer *r, int screen_width, int screen_height) {
    if (!r->post_enabled) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, r->offscreen.fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, r->width, r->height, 0, 0, screen_width, screen_height,
                           GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screen_width, screen_height);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(r->post_shader->id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->offscreen.color_tex);
    if (r->post_shader->locations[U_SCREEN_TEX] != -1)
        glUniform1i(r->post_shader->locations[U_SCREEN_TEX], 0);

    glBindVertexArray(r->screen_quad->binding.vao);
    glDrawElements(GL_TRIANGLES, r->screen_quad->index_count, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}

void scene_update(Scene *scene, float dt) {
    // 1. 시간 업데이트
    scene->context.time += dt;

    // 2. 카메라 상태 업데이트 및 행렬 재계산
    camera_ensure_updated(scene->context.camera);

    for(int i=0;i<scene->object_count;i++)
    {
        RenderObject *obj =
            &scene->objects[i];

        if (obj->callbacks) {
            for (int j = 0; j < obj->callbacks->update_count; j++) {
                if (obj->callbacks->updates[j])
                    obj->callbacks->updates[j](obj, &scene->context, dt);
            }
        }
    }

}

Scene *scene_create(Assets *assets, Camera *camera, SceneDescriptor *desc) {
    Scene *scene = (Scene *)calloc(1, sizeof(Scene));
    if (!scene) return NULL;

    scene->object_count = desc->object_count;
    scene->objects = (RenderObject *)malloc(sizeof(RenderObject) * desc->object_count);
    if (!scene->objects) {
        free(scene);
        return NULL;
    }

    for (int i = 0; i < desc->object_count; i++) {
        scene->objects[i] = render_object_create(assets, &desc->objects[i]);
    }

    scene->context = (SceneContext){
        .camera = camera,
        .time   = 0.0f,
    };
    memcpy(scene->context.light_pos,   desc->light_pos,   sizeof(float) * 3);
    memcpy(scene->context.light_color, desc->light_color, sizeof(float) * 3);

    return scene;
}

void scene_destroy(Scene **scene) {
    if (!scene || !*scene) return;

    for (int i = 0; i < (*scene)->object_count; i++) {
        RenderObject *obj = &(*scene)->objects[i];
        if (obj->callbacks && obj->callbacks->destroy) {
            obj->callbacks->destroy(obj->user_data);
        }
    }

    free((*scene)->objects);
    free(*scene);
    *scene = NULL;
}

Framebuffer framebuffer_create(int width, int height) {
    GLuint fbo, rbo, color_tex;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // color attachment
    glGenTextures(1, &color_tex);
    glBindTexture(GL_TEXTURE_2D, color_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // depth+stencil attachment
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
       printf("Framebuffer incomplete\n");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return (Framebuffer){fbo, rbo, color_tex};
}

void framebuffer_destroy(Framebuffer *fb) {
    if (fb) {
        if (fb->color_tex) glDeleteTextures(1, &fb->color_tex);
        if (fb->rbo)       glDeleteRenderbuffers(1, &fb->rbo);
        if (fb->fbo)       glDeleteFramebuffers(1, &fb->fbo);
        *fb = (Framebuffer){0};
    }
}

GLuint texture_create(const char *file_path) {
    GLuint texture = 0;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_NEAREST_MIPMAP_NEAREST);

    stbi_set_flip_vertically_on_load(1);

    int width, height, nr_channels;

    unsigned char *data =
        stbi_load(file_path, &width, &height, &nr_channels, 0);

    if (data) {

        GLenum format = GL_RGB;

        switch (nr_channels) {
        case 1: format = GL_RED; break;
        case 2: format = GL_RG; break;
        case 3: format = GL_RGB; break;
        case 4: format = GL_RGBA; break;

        default:
            printf("Unsupported channel count: %d\n",
                   nr_channels);
            stbi_image_free(data);
            glDeleteTextures(1, &texture);
            return 0;
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            format,
            width,
            height,
            0,
            format,
            GL_UNSIGNED_BYTE,
            data
        );

        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else {
        printf("Failed to load texture: %s\n", file_path);

        glDeleteTextures(1, &texture);
        return 0;
    }

    stbi_image_free(data);

    return texture;
}

void texture_destroy(GLuint *tex) {
    if (tex && *tex != 0) {
        glDeleteTextures(1, tex);
        *tex = 0; // 🛠️ 안전망: 해제 후 포인터 값을 0(초기화)으로 세팅
    }
}

GLuint shader_program_create(const char *vert_path, const char *frag_path) {
    char *vert_src = shader_source_load(vert_path);
    char *frag_src = shader_source_load(frag_path);

    if (!vert_src || !frag_src) {
        printf("Failed to load shader sources.\n");
        if (vert_src) free(vert_src);
        if (frag_src) free(frag_src);
        return 0;
    }

    int success;
    char info_log[512];

    unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, (const char **)&vert_src, NULL);
    glCompileShader(vertex_shader);

    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(vertex_shader, 512, NULL, info_log);
        printf("Vertex Shader: Compile failed: %s\n", info_log);
    }
    free(vert_src);

    unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, (const char **)&frag_src, NULL);
    glCompileShader(fragment_shader);

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(fragment_shader, 512, NULL, info_log);
        printf("Fragment Shader: Compile failed.\n%s", info_log);
    }
    free(frag_src);

    GLuint shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);

    glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shader_program, 512, NULL, info_log);
        printf("Link failed: %s\n", info_log);
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return shader_program;
}

void shader_program_destroy(GLuint prog) {
    glDeleteProgram(prog);
}

char *shader_source_load(const char *file_path) {
    FILE *f = fopen(file_path, "rb");
    if(!f) {
        printf("shader file not found: %s\n", file_path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    rewind(f);

    char *source = (char *)malloc(length + 1);
    if(!source) {
        fclose(f);
        return NULL;
    }

    long actual_length = fread(source, 1, length, f);
    source[actual_length] = '\0';
    fclose(f);

    return source;
}

RenderObject render_object_create(Assets *assets, ObjectDescriptor *desc) {
    RenderObject obj = {0};
    obj.mesh = assets->meshes[desc->mesh];
    obj.material = &assets->materials[desc->material];

    memcpy(obj.transform.pos,   desc->position, sizeof(float) * 3);
    memcpy(obj.transform.rot,   desc->rotation, sizeof(float) * 3);
    memcpy(obj.transform.scale, desc->scale,    sizeof(float) * 3);
    obj.callbacks = desc->callbacks;
    obj.user_data = desc->user_data;
    obj.transform.dirty = true; // 첫 draw 때 transform_ensure_updated가 계산

    return obj;
}

void camera_update_view(Camera *camera) {
    mat4_look_at(&camera->view, camera->position, camera->target, camera->up);
}

void camera_update_proj(Camera *camera) {
    mat4_perspective(&camera->proj, camera->fov, camera->aspect, camera->near_plane, camera->far_plane);
}

void camera_init(Camera *camera, float aspect) {
    *camera = (Camera){
        /* view 가 바뀌면 camera_update_view() 호출 */
        .position = {0.0f, 2.5f, 5.0f},
        .target = {0.0f, 0.0f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f},

        /* proj 가 바뀌면 camera_update_proj() 호출 */
        .fov = 45.0f * 3.141592f / 180.0f,
        .aspect = aspect,
        .near_plane = 0.1f,
        .far_plane = 100.0f,

        .view_dirty = false,
        .proj_dirty = false
    };

    camera_update_view(camera);
    camera_update_proj(camera);
}

static void vec3_sub(float out[3], const float a[3], const float b[3]) {
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

static float vec3_dot(const float a[3], const float b[3]) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

static void vec3_cross(float out[3], const float a[3], const float b[3]) {
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

static void vec3_normalize(float v[3]) {
    float len = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (len > 0.000001f) {
        v[0] /= len;
        v[1] /= len;
        v[2] /= len;
    }
}

void mat4_look_at(Mat4 *out, const float eye[3], const float target[3], const float world_up[3]) {
    /*
    world_up: 힌트로 주는 "월드 기준 위쪽" (보통 (0,1,0) 고정)
    forward: 카메라가 보는 방향
    right: forward × world_up → 카메라의 오른쪽 축
    cam_up: right × forward → 실제 카메라 좌표계의 위쪽 축
    (world_up과 다를 수 있음, 특히 카메라가 위/아래를 바라볼 때)


     크로스 프로덕트의 방향
     오른손을 펴서, 네 손가락을 a 방향으로 향하게 하고,
     그 상태에서 손가락을 b 방향으로 감아쥐세요(curl).
     그때 엄지손가락이 가리키는 방향이 a × b의 방향이에요.
     */

    float forward[3], right[3], cam_up[3];
    float world_up_n[3] = { world_up[0], world_up[1], world_up[2] };
    // 호출자가 넘긴 원본 world_up 배열은 건드리지 않고 보존하기 위한 용도입니다.

    vec3_sub(forward, target, eye);
    vec3_normalize(forward);

    vec3_normalize(world_up_n);

    // forward와 world_up_n이 같은 방향이면 크로스프로덕트가 0이 되므로.
    // 다른 보조축으로 대체
    float d = fabsf(vec3_dot(forward, world_up_n));
    if (d > 0.999f) {
        world_up_n[0] = 1.0f;
        world_up_n[1] = 0.0f;
        world_up_n[2] = 0.0f;
    }

    vec3_cross(right, forward, world_up_n);
    vec3_normalize(right);

    vec3_cross(cam_up, right, forward);
    vec3_normalize(cam_up);

    mat4_identity(out);

    out->data[0]  = right[0];
    out->data[4]  = right[1];
    out->data[8]  = right[2];
    out->data[12] = -vec3_dot(right, eye);

    out->data[1]  = cam_up[0];
    out->data[5]  = cam_up[1];
    out->data[9]  = cam_up[2];
    out->data[13] = -vec3_dot(cam_up, eye);

    out->data[2]  = -forward[0];
    out->data[6]  = -forward[1];
    out->data[10] = -forward[2];
    out->data[14] =  vec3_dot(forward, eye);
}

void camera_set_target(Camera *camera, float px, float py, float pz) {
    camera->target[0] = px;
    camera->target[1] = py;
    camera->target[2] = pz;
    camera->view_dirty = true;
}

void camera_set_position(Camera *camera, float px, float py, float pz) {
    camera->position[0] = px;
    camera->position[1] = py;
    camera->position[2] = pz;
    camera->view_dirty = true;
}

void camera_set_aspect(Camera *camera, float aspect) {
    camera->aspect = aspect;
    camera->proj_dirty = true;
}

void camera_ensure_updated(Camera *camera) {
    if (camera->view_dirty) {
        //mat4_look_at(&cam->view, cam->position, cam->target, cam->up);
        camera_update_view(camera);
        camera->view_dirty = false;
    }
    if (camera->proj_dirty) {
        //mat4_perspective(&camera->proj, camera->fov, camera->aspect, camera->near_plane, camera->far_plane);
        camera_update_proj(camera);
        camera->proj_dirty = false;
    }
}

void transform_set_rotation(Transform *t, float x, float y, float z) {
    t->rot[0] = x; t->rot[1] = y; t->rot[2] = z;
    t->dirty = true;
}

void transform_ensure_updated(Transform *t) {
    if (t->dirty) {
        mat4_compose_trs(&t->model, t->pos, t->rot, t->scale); // 기존 함수 재사용
        t->dirty = false;
    }
}

/* ---- 하드코딩 primitive 데이터 ---- */
static float quad_verts[] = {
    0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
    0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
    -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
    -0.5f,  0.5f, 0.0f, 0.0f, 1.0f
};
static GLuint quad_indices[] = {0, 3, 2, 0, 2, 1};

static float ground_verts[] = {
     10.0f,  0.0f, -10.0f,  0.0f, 1.0f, 0.0f,
     10.0f,  0.0f,  10.0f,  0.0f, 1.0f, 0.0f,
    -10.0f,  0.0f,  10.0f,  0.0f, 1.0f, 0.0f,
    -10.0f,  0.0f,  10.0f,  0.0f, 1.0f, 0.0f,
    -10.0f,  0.0f, -10.0f,  0.0f, 1.0f, 0.0f,
     10.0f,  0.0f, -10.0f,  0.0f, 1.0f, 0.0f,
};

static float screen_quad_verts[] = {
     1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
     1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
    -1.0f,  1.0f, 0.0f, 0.0f, 1.0f
};
static GLuint screen_quad_indices[] = {0, 3, 2, 0, 2, 1};

void assets_init(Assets *assets) {

    MeshDescriptor quad_desc = {
        .vertices     = quad_verts,
        .vertex_bytes = sizeof(quad_verts),
        .stride       = sizeof(float) * 5,
        .indices      = quad_indices,
        .index_bytes  = sizeof(quad_indices),
        .attribs      = { {0,3,GL_FLOAT}, {1,2,GL_FLOAT} },
        .attrib_count = 2,
        .usage        = GL_STATIC_DRAW,
    };
    assets->meshes[MESH_QUAD] = mesh_create(&quad_desc);

    MeshDescriptor ground_desc = {
        .vertices     = ground_verts,
        .vertex_bytes = sizeof(ground_verts),
        .stride       = sizeof(float) * 6,
        .indices      = NULL,
        .index_bytes  = 0,
        .attribs      = { {0,3,GL_FLOAT}, {1,3,GL_FLOAT} },
        .attrib_count = 2,
        .usage        = GL_STATIC_DRAW,
    };
    assets->meshes[MESH_PLANE] = mesh_create(&ground_desc);

    assets->meshes[MESH_MONKEY] = load_obj_with_tinyobj("monkey.obj", true);

    assets->meshes[MESH_SPHERE] = load_obj_with_tinyobj("sphere.obj", true);

    MeshDescriptor screen_quad_desc = {
        .vertices = screen_quad_verts, .vertex_bytes = sizeof(screen_quad_verts),
        .stride = sizeof(float) * 5,
        .indices = screen_quad_indices, .index_bytes = sizeof(screen_quad_indices),
        .attribs = { {0,3,GL_FLOAT}, {1,2,GL_FLOAT} }, .attrib_count = 2,
        .usage = GL_STATIC_DRAW,
    };
    assets->meshes[MESH_SCREEN_QUAD] = mesh_create(&screen_quad_desc);

    for (int i = 0; i < MESH_COUNT; i++) {
        if (assets->meshes[i]) mesh_upload(assets->meshes[i]);
        else printf("Warning: mesh %d failed to load\n", i);
    }

    /* ---- Textures ---- */
    assets->textures[TEX_GIRL]  = texture_create("girl2.jpg");
    //assets->textures[TEX_BRICK] = texture_create("brick.jpg");

    /* ---- Shaders ---- */
    assets->shaders[SHADER_TEXTURE].id = shader_program_create("texture1.vert", "texture1.frag");
    shader_program_resolve_uniforms(&assets->shaders[SHADER_TEXTURE]);

    assets->shaders[SHADER_PHONG].id = shader_program_create("phong.vert", "phong.frag");
    shader_program_resolve_uniforms(&assets->shaders[SHADER_PHONG]);

    assets->shaders[SHADER_POST_INVERT].id = shader_program_create("post.vert", "post.frag");
    shader_program_resolve_uniforms(&assets->shaders[SHADER_POST_INVERT]);

    /* ---- Materials ---- */
    assets->materials[MAT_TEXTURE] = (Material){
        .shader = &assets->shaders[SHADER_TEXTURE],
        .texture = assets->textures[TEX_GIRL],
        .color = {0.5f, 0.2f, 0.7f}
    };
    assets->materials[MAT_GROUND] = (Material){
        .shader = &assets->shaders[SHADER_PHONG],
        .texture = 0,
        .color = {0.7f, 0.7f, 0.7f}
    };
    assets->materials[MAT_MONKEY] = (Material){
        .shader = &assets->shaders[SHADER_PHONG],
        .texture = 0,
        .color = {0.2f, 0.3f, 0.3f}
    };
}

void assets_destroy(Assets *assets) {
    for (int i = 0; i < MESH_COUNT; i++) {
        if (assets->meshes[i]) mesh_destroy(&assets->meshes[i]);
    }
    for (int i = 0; i < TEX_COUNT; i++) {
        if (assets->textures[i]) texture_destroy(&assets->textures[i]);
    }
    for (int i = 0; i < SHADER_COUNT; i++) {
        if (assets->shaders[i].id) shader_program_destroy(assets->shaders[i].id);
    }
}

/*
void assets_add_mesh(Assets *assets, const char *key, Mesh *mesh) {
    strcpy(assets->meshes[assets->mesh_count], key);
    assets->meshes[assets->mesh_count].resource = mesh;
    assets->mesh_count++;
}

Mesh *assets_get_mesh(Assets *assets, const char *key) {
    for (int i = 0; i < assets->mesh_count; i++) {
        if (strcmp(assets->meshes[i], key) == 0) return assets->meshes[i].resource;
    }
    return NULL;
}

asset.txt
* [type] [key] [filepath] [options]
* MESH monkey monkey.obj has_normal=true
* TEXTURE girl girl.png filter=linear
* SHADER phong phong #vert/frag

void assets_load_from_manifest(Assets *assets, const char *manifest_path) {
    FILE *file = fopen(manifest_path, "r");
    if (!file) return;

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char type[16], key[64], filepath[256], options[256];
        sscanf(line, "%15s %63s %255s %255s", type, key, filepath, options);
        if(type == MESH) {
            Mesh *mesh = load_obj_.....
            assets_add_mesh(assets, key, mesh);
        } else if(type == TEXTURE) {
            GLuint t = texture_create(filepath);
            assets_add_texture(assets, key, t);
        } else if(type == SHADER) {.....
        }
    }
    fclose(file);
}

*/

Renderer *renderer_create(Assets *assets, RenderDescriptor *desc) {
    Renderer *r = calloc(1, sizeof(Renderer));
    r->offscreen = framebuffer_create(desc->width, desc->height);
    r->post_shader = &assets->shaders[desc->post_shader];
    r->screen_quad = assets->meshes[MESH_SCREEN_QUAD];
    r->width = desc->width;
    r->height = desc->height;
    return r;
}

void renderer_resize(Renderer *r, int width, int height) {
    if (r->width == width && r->height == height) return; // 크기 안 바뀌었으면 스킵

    framebuffer_destroy(&r->offscreen);
    r->offscreen = framebuffer_create(width, height);
    r->width = width;
    r->height = height;
}

void renderer_destroy(Renderer *r) {
    if (!r) return;
    framebuffer_destroy(&r->offscreen);
    free(r);
}
