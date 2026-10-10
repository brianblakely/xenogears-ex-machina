#ifndef MENU_NODE_H
#define MENU_NODE_H

#include "common.h"
#include "psyq/libgte.h"
#include "resident/model.h"
#include "display.h"

/* The 3D scene graph (arena_scene_graph_and_opponent 800898BC-8008A2B8, 8008A3E0-8008AC0C,
 * 8008AF6C-8008B5FC, 8008B730-8008BA2C, 8008BCC8-8008C7C0): nodes with
 * typed payloads (models, model sets, lights, instances), their animation
 * players, the light rigs that view them, and the mesh packets of the
 * instanced models. */

/* Scene node (0x9C bytes): a typed payload with its own transform, linked
 * into a tree of children. */
typedef struct Node {
    s32 type;          /* 1 model, 2 model set, 3, 4, 5, 6 */
    void *data;        /* type-specific payload */
    void (*callback)(struct Node *node); /* 0x08: run before each update */
    MATRIX view;       /* 0x0C: local-to-screen */
    SVECTOR offset;    /* 0x2C: offset from the parent (0/1 nodes) */
    VECTOR position;   /* 0x34 */
    SVECTOR angles;    /* 0x44: rotation angles */
    MATRIX unk4C;      /* 0x4C: local-to-world rotation */
    MATRIX unk6C;      /* 0x6C: light matrix */
    struct Node *parent; /* 0x8C */
    struct Node *next;   /* 0x90: next sibling */
    struct Node *child;  /* 0x94: first child */
    s32 unk98;
} Node;

/* Model payload (type 1, 0x20 bytes): a loaded sprite model, its morph
 * state and its packets. */
typedef struct {
    u32 flags;
    s32 unk4;
    void *packets[2];  /* 0x08: per display buffer (one block) */
    MorphState *morph; /* 0x10 */
    SpriteModel *file; /* 0x14 */
    u8 r, g, b;        /* 0x18 */
    u8 unk1B;
    s32 unk1C;
} NodeModel;

/* Primitive record of a model's packet buffers (0x14 bytes). */
typedef struct {
    u32 tag;
    u32 colour;        /* 0x04 */
    u8 unk8[0xC];
} ModelPrim;

/* Packet buffers built for a model's mesh (a resident sprite model's
 * vertices and primitive groups). */
typedef struct {
    s16 vertices;      /* 0x00 */
    s16 count;         /* 0x02: primitives */
    void *vertexData;  /* 0x04 */
    u8 *work;          /* 0x08: 8 bytes per vertex */
    SpriteModel *mesh; /* 0x0C */
    ModelPrim *prims[2]; /* 0x10: per display buffer */
} ModelPrims;

/* Instance payload (type 5, 0x10 bytes): draws another node's model. */
typedef struct {
    s32 type;          /* the source node's type */
    Node *source;      /* 0x04 */
    s32 unk8;
    ModelPrims *prims; /* 0x0C: own packet buffers for model sources */
} Instance;

/* Light payload (0x14 bytes), which the resident loads into the GTE light
 * matrix (80030a30): its ModelLight (resident/model.h) with signed
 * colours, which the menu halves as signed values (80083dcc). */
typedef struct Light {
    s32 direction[3];
    s16 colour[3];     /* 0x0C: 0x1000 = full */
    s16 unk12;
} Light;

#define NODE_LIGHT(node) ((Light *)(node)->data)

/* Animation record: what it drives and its value or stream offset. */
typedef struct {
    u8 kind;           /* & 0x7F: 3-5 node angle x/y/z, 6-8 node 0x2C x/y/z */
    u8 node;           /* index into the model set's nodes */
    u16 value;         /* key value, or channel stream offset */
} AnimRecord;

typedef struct {
    s16 frames;
    s16 unk2;
    s16 keys;          /* 0x04 */
    s16 channels;      /* 0x06 */
    AnimRecord records[1]; /* 0x08: keys, then channels */
} AnimHeader;

/* Constant key of a player (8 bytes). */
typedef struct {
    s16 *target;
    s16 value;         /* 0x04 */
    s16 angular;       /* 0x06: eased the short way round */
} Key;

/* Animation channel of a player (0x14 bytes). */
typedef struct {
    u8 *start;
    u8 *current;       /* 0x04: position in the stream */
    s16 *target;       /* 0x08 */
    u16 hold;          /* 0x0C: frames left at the current delta */
    s16 value;         /* 0x0E */
    s16 delta;         /* 0x10 */
    s16 angular;       /* 0x12 */
} Channel;

/* Animation player. */
typedef struct {
    AnimHeader *header;
    Key *keys;         /* 0x04 */
    Channel *channels; /* 0x08 */
    s32 unkC;
    s16 unk10;
    s16 frame;         /* 0x12 */
} Player;

/* Hierarchy record of a model set file (0x10 bytes). */
typedef struct {
    s16 parent;        /* -1: the set's root */
    s16 model;         /* -1: none; else index of a 0x38-byte model */
    struct { s16 vx, vy, vz; } angle; /* 0x04: no vector padding */
    s16 offset[3];     /* 0x0A: initial 0x2C components */
} HierarchyRecord;

/* Model set payload (type 2, 0x1C bytes): a node per hierarchy record and
 * an animation player per animation. */
typedef struct {
    s16 nodeCount;
    s16 count;         /* 0x02: players */
    struct Node **nodes; /* 0x04 */
    Player *players;   /* 0x08 */
    s32 unkC;
    s16 scale[3];      /* 0x10: 4096 = 1.0 */
    s16 unk16;
    HierarchyRecord *records; /* 0x18 */
} ModelSet;

/* An actor's model file, loaded as one block: a model set (hierarchy,
 * models, animations), the actor's header, move slots and images. Its
 * pointers are relative to the address it was built at (0x1C) until
 * arena_node_relocate_model_file relocates them. */
typedef struct ModelFile {
    u32 *hierarchy;    /* count, then that many HierarchyRecords */
    u8 *models;        /* 0x04: the model group */
    u32 *animations;   /* 0x08: count, then that many animation pointers or null */
    u8 *target;        /* 0x0C: texture/CLUT target */
    struct SceneHeader *header; /* 0x10 */
    s32 unk14;
    struct MoveSlot *slots; /* 0x18: one per combo number */
    u8 *base;          /* 0x1C */
    u8 *image;         /* 0x20: palette and emblem pixels */
    u8 *unk24;
} ModelFile;

/* Three-light rig with an ambient colour (0x28C bytes): the 3D view's
 * camera node, its lights and its drawing layer. */
typedef struct {
    s32 unk0;
    Node *camera;      /* 0x04 */
    Node *lights[3];   /* 0x08 */
    Node storage[4];   /* 0x14 */
    OtPair *layer;     /* 0x284 */
    u8 r, g, b;        /* 0x288 */
    u8 unk28B;
} LightRig;

extern MATRIX arena_identity_matrix;       /* identity */
extern s32 arena_node_players_share_keys;  /* nonzero: model set players do not own their keys */
extern s32 arena_node_compose_parent_view; /* nonzero: model sets compose with their parent's view */
extern VECTOR arena_view_origin;           /* the scene origin: the focus the view was last aimed at */
extern VECTOR arena_look_at_axis_x;        /* look-at work: third axis */
extern VECTOR arena_look_at_forward;       /* look-at work: forward */
extern VECTOR arena_mesh_light_direction;  /* mesh light direction */
extern MATRIX arena_display_unread_identity;
extern VECTOR arena_look_at_axis_y;        /* look-at work: up */

void arena_look_at_build_matrix(MATRIX *m, SVECTOR *eye, SVECTOR *at, SVECTOR *up);
void arena_node_aim_rig_camera(LightRig *view, VECTOR *position, VECTOR *focus);
Node *arena_node_reset(Node *node);
Node *arena_node_alloc(void);
void arena_node_add_child(Node *parent, Node *child);
void arena_node_free_tree(Node *node);
void arena_node_set_model(Node *node, NodeModel *model);
void arena_node_set_model_set(Node *node, ModelSet *set);
void arena_node_set_light(Node *node, void *data);
ModelSet *arena_node_alloc_model_set(void);
void arena_node_free_model_set(ModelSet *set);
NodeModel *arena_node_reset_model(NodeModel *model);
NodeModel *arena_node_alloc_model(void);
void arena_node_free_model(NodeModel *model);
void arena_node_set_tpage_override(s16 x, s16 y);
void arena_node_set_clut_override(s16 x, s16 y);
void arena_node_set_texture_overrides(s16 tx, s16 ty, s16 cx, s16 cy);
void arena_node_clear_texture_overrides(void);
void arena_node_init_model(NodeModel *model, SpriteModel *file);
Light *arena_node_alloc_light(void);
LightRig *arena_node_alloc_light_rig(OtPair *layer);
void arena_node_free_light_rig(LightRig *rig);
void arena_node_disable_color_overrides(void);
void arena_node_draw_model(NodeModel *model);
void arena_node_color_instance(Node *node);
void arena_node_draw_tree(Node *node);
void arena_node_load_rig_lights(Node **lights);
ModelFile *arena_node_relocate_model_file(ModelFile *file);
void arena_node_rewind_anim_player(Player *player);
void arena_node_bind_animation(AnimRecord *record, Player *player, Node *root);
Node *arena_node_build_model_set(ModelFile *file);
s32 arena_node_step_anim_player(Player *player, s32 frames, s32 steps);
void arena_mesh_set_light_and_project_shadow(SpriteModel *mesh, u8 *work);
void arena_mesh_draw_groups(SpriteModel *mesh, ModelPrim *prims, u32 *ot, u8 *work);
void arena_mesh_build_packets(ModelPrims *prims, SpriteModel *mesh);
void arena_node_free_instance(Instance *instance);
Node *arena_node_copy_subtree_as_instances(Node *source, Node *parent);
Node *arena_node_copy_tree_as_instances(Node *source);
Node *arena_node_copy_root_as_instances(Node *source);
void arena_node_draw_instances(Node *node);
/* Project toward arena_mesh_light_direction onto y=0; writes work.vx/vz, preserving vy/pad.
 * count must be positive. Reads the next vertex even on the last iteration. */
void arena_mesh_project_shadow(void *vertices, u8 *work, s32 count);
/* Mesh packet builders consume eight-byte u16 index records and preload one
 * beyond count. They advance model_current_packet past culled packet slots too, and
 * prepend accepted packets to model_ot without a depth sort. */
void arena_mesh_draw_flat_triangles(u8 *prims, s32 count); /* triangles */
void arena_mesh_draw_flat_quads(u8 *prims, s32 count); /* quads */

#endif
