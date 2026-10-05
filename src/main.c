#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <vita2d.h>
#include "p5ck.h"
#include "fp_resource.h"
#include "fp_model.h"
#include "fp_geometry.h"
#include "fp_vif.h"
#include "fp_vu.h"
#include "fp_gif.h"

#define DATA_PATH "ux0:data/TimeSplitters/PAK/CHR.PAK"
#define BOOT_PATH "ux0:data/TimeSplitters/SLED_530.66"
#define MAX_TRI_VERTICES 24000
#define MAX_GIF_VERTICES 2048

static TsFpGifVertex scene_vertices[MAX_TRI_VERTICES];
static size_t scene_vertex_count;
static uint32_t scene_submeshes;
static uint32_t scene_input_vertices;
static uint32_t scene_gif_vertices;
static uint32_t scene_failures;
static uint32_t scene_unsupported;
static uint32_t scene_xgkicks;
static float scene_angle;

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static size_t append_tri(TsFpGifVertex *dst, size_t count,
                         const TsFpGifVertex *a, const TsFpGifVertex *b,
                         const TsFpGifVertex *c) {
    if (count + 3 > MAX_TRI_VERTICES) return count;
    dst[count++] = *a;
    dst[count++] = *b;
    dst[count++] = *c;
    return count;
}

static size_t append_gif_geometry(const TsFpGifVertex *v, size_t n,
                                  uint32_t primitive, size_t count) {
    uint32_t prim = primitive & 7u;
    size_t i;

    if (prim == 3u) {
        for (i = 0; i + 2 < n; i += 3)
            count = append_tri(scene_vertices, count, &v[i], &v[i + 1], &v[i + 2]);
    } else if (prim == 4u) {
        for (i = 2; i < n; ++i) {
            const TsFpGifVertex *a = &v[i - 2];
            const TsFpGifVertex *b = &v[i - 1];
            const TsFpGifVertex *c = &v[i];
            if ((i & 1u) != 0)
                count = append_tri(scene_vertices, count, b, a, c);
            else
                count = append_tri(scene_vertices, count, a, b, c);
        }
    } else if (prim == 5u) {
        for (i = 2; i < n; ++i)
            count = append_tri(scene_vertices, count, &v[0], &v[i - 1], &v[i]);
    } else if (prim == 6u) {
        for (i = 0; i + 1 < n; i += 2) {
            TsFpGifVertex a = v[i], b = v[i + 1];
            TsFpGifVertex c = a, d = b;
            c.x = a.x; c.y = b.y;
            d.x = b.x; d.y = a.y;
            count = append_tri(scene_vertices, count, &a, &d, &c);
            count = append_tri(scene_vertices, count, &c, &d, &b);
        }
    } else {
        /* Points/lines are not useful for the model preview; preserve triangles. */
        for (i = 0; i + 2 < n; i += 3)
            count = append_tri(scene_vertices, count, &v[i], &v[i + 1], &v[i + 2]);
    }
    return count;
}

static int execute_submesh(const uint8_t *vif_data, size_t vif_size,
                           const uint8_t *micro, size_t micro_size,
                           uint8_t *vu_mem, size_t vu_size,
                           uint8_t *gif_mem, size_t gif_size) {
    TsFpVifSummary vif;
    TsFpVifMemorySummary vm;
    TsFpVuState vs;
    TsFpGifSummary gif;
    TsFpGifVertex verts[MAX_GIF_VERTICES];

    if (tsfp_vif_scan(vif_data, vif_size, &vif) != 0 || !vif.mscal_count)
        return -1;
    if (tsfp_vif_unpack_memory(vif_data, vif_size, vu_mem, vu_size, &vm) != 0)
        return -2;

    memset(&gif, 0, sizeof(gif));
    memset(verts, 0, sizeof(verts));
    tsfp_vu_state_init(&vs, vu_mem, vu_size, gif_mem, gif_size);
    if (tsfp_vu_execute(micro, micro_size, vm.mscal_address,
                        &vs, 16384) != 0)
        return -3;
    scene_unsupported += vs.unsupported;
    if (vs.xgkick_pc == UINT32_MAX || vs.gif_used == 0)
        return -4;
    scene_xgkicks++;

    if (tsfp_gif_parse(gif_mem, vs.gif_used, &gif, verts, MAX_GIF_VERTICES) != 0)
        return -5;
    scene_gif_vertices += gif.vertices;
    scene_vertex_count = append_gif_geometry(verts,
                                              gif.vertices < MAX_GIF_VERTICES ?
                                              gif.vertices : MAX_GIF_VERTICES,
                                              gif.primitive, scene_vertex_count);
    return 0;
}

static int load_scene(TsP5ckInfo *info, TsP5ckEntry *entry,
                      TsFpResourceSummary *resource, TsFpModelHeader *model,
                      TsFpGeometrySummary *geometry, TsFpVifSummary *vif_summary,
                      TsFpGifSummary *gif_summary) {
    FILE *fp = NULL;
    FILE *elf_fp = NULL;
    uint8_t *entry_data = NULL;
    uint8_t *elf = NULL;
    uint8_t *vu_mem = NULL;
    uint8_t *gif_mem = NULL;
    size_t entry_size, elf_size, off, len;
    long end;
    int result = -1;

    fp = fopen(DATA_PATH, "rb");
    if (!fp) return -10;

    if (ts_p5ck_read_info(fp, info) != 0 ||
        info->entry_count == 0 ||
        ts_p5ck_read_entry(fp, info, 0, entry) != 0)
        goto done;

    entry_size = entry->compressed_length ? entry->compressed_length : entry->length;
    if (!entry_size || entry_size > 64u * 1024u * 1024u)
        goto done;
    if (fseek(fp, (long)entry->offset, SEEK_SET) != 0)
        goto done;

    entry_data = (uint8_t *)malloc(entry_size);
    if (!entry_data || fread(entry_data, 1, entry_size, fp) != entry_size)
        goto done;

    if (tsfp_resource_probe(entry_data, entry_size, resource) == 0)
        goto done;
    if (tsfp_model_probe(entry_data, entry_size, model) != 0)
        goto done;
    if (tsfp_geometry_probe(entry_data, entry_size, model->mesh_table_offset,
                            model->mesh_count, geometry) != 0)
        goto done;

    elf_fp = fopen(BOOT_PATH, "rb");
    if (!elf_fp || fseek(elf_fp, 0, SEEK_END) != 0)
        goto done;
    end = ftell(elf_fp);
    if (end <= 0 || end > 32L * 1024L * 1024L)
        goto done;
    elf_size = (size_t)end;
    if (fseek(elf_fp, 0, SEEK_SET) != 0)
        goto done;
    elf = (uint8_t *)malloc(elf_size);
    if (!elf || fread(elf, 1, elf_size, elf_fp) != elf_size)
        goto done;
    if (tsfp_vu_find_vutext(elf, elf_size, &off, &len) != 0)
        goto done;

    vu_mem = (uint8_t *)malloc(16u * 1024u);
    gif_mem = (uint8_t *)malloc(64u * 1024u);
    if (!vu_mem || !gif_mem)
        goto done;

    scene_vertex_count = 0;
    scene_submeshes = 0;
    scene_input_vertices = 0;
    scene_gif_vertices = 0;
    scene_failures = 0;
    scene_unsupported = 0;
    scene_xgkicks = 0;
    memset(vif_summary, 0, sizeof(*vif_summary));
    memset(gif_summary, 0, sizeof(*gif_summary));

    for (uint32_t mi = 0; mi < model->mesh_count; ++mi) {
        uint32_t ptr = rd32(entry_data + model->mesh_table_offset + mi * 4u);
        uint32_t next = (uint32_t)entry_size;

        if (!ptr) continue;
        if (ptr >= entry_size || (ptr & 3u)) {
            scene_failures++;
            continue;
        }

        for (uint32_t j = mi + 1; j < model->mesh_count; ++j) {
            uint32_t candidate = rd32(entry_data + model->mesh_table_offset + j * 4u);
            if (candidate > ptr && candidate < next)
                next = candidate;
        }
        if (next == entry_size)
            next = ptr + 8u;
        if (next <= ptr || (next - ptr) % 8u) {
            scene_failures++;
            continue;
        }

        for (uint32_t q = ptr; q + 8u <= next; q += 8u) {
            uint32_t data_offset = rd32(entry_data + q);
            uint16_t vertex_count = (uint16_t)entry_data[q + 4] |
                                    ((uint16_t)entry_data[q + 5] << 8);
            size_t vif_size;

            if (!vertex_count)
                continue;
            if (data_offset >= entry_size ||
                vertex_count > (entry_size - data_offset) / 16u) {
                scene_failures++;
                continue;
            }

            vif_size = (size_t)vertex_count * 16u;
            scene_input_vertices += vertex_count;
            memset(vu_mem, 0, 16u * 1024u);
            memset(gif_mem, 0, 64u * 1024u);

            if (execute_submesh(entry_data + data_offset, vif_size,
                                elf + off, len, vu_mem, 16u * 1024u,
                                gif_mem, 64u * 1024u) == 0) {
                scene_submeshes++;
            } else {
                scene_failures++;
            }
        }
    }

    result = scene_submeshes ? 0 : -20;
done:
    if (elf_fp) fclose(elf_fp);
    if (fp) fclose(fp);
    free(gif_mem);
    free(vu_mem);
    free(elf);
    free(entry_data);
    return result;
}

static void rebuild_screen_vertices(vita2d_color_vertex *out, float angle) {
    float minx = 1e30f, miny = 1e30f, maxx = -1e30f, maxy = -1e30f;
    float sx, sy, scale, cx, cy;
    const float ox = 480.0f, oy = 310.0f;

    for (size_t i = 0; i < scene_vertex_count; ++i) {
        if (scene_vertices[i].x < minx) minx = scene_vertices[i].x;
        if (scene_vertices[i].x > maxx) maxx = scene_vertices[i].x;
        if (scene_vertices[i].y < miny) miny = scene_vertices[i].y;
        if (scene_vertices[i].y > maxy) maxy = scene_vertices[i].y;
    }

    if (scene_vertex_count == 0) return;
    cx = (minx + maxx) * 0.5f;
    cy = (miny + maxy) * 0.5f;
    sx = (maxx - minx) > 0.0001f ? 780.0f / (maxx - minx) : 1.0f;
    sy = (maxy - miny) > 0.0001f ? 400.0f / (maxy - miny) : 1.0f;
    scale = sx < sy ? sx : sy;

    for (size_t i = 0; i < scene_vertex_count; ++i) {
        float x = (scene_vertices[i].x - cx) * scale;
        float y = (scene_vertices[i].y - cy) * scale;
        float rx = x * cosf(angle) - y * sinf(angle);
        float ry = x * sinf(angle) + y * cosf(angle);
        out[i].x = ox + rx;
        out[i].y = oy + ry;
        out[i].z = 0.5f;
        out[i].color = ((unsigned)scene_vertices[i].a << 24) |
                       ((unsigned)scene_vertices[i].b << 16) |
                       ((unsigned)scene_vertices[i].g << 8) |
                       scene_vertices[i].r;
    }
}

static void draw_status(int result, const TsP5ckInfo *info,
                        const TsP5ckEntry *entry, const TsFpModelHeader *model,
                        const TsFpGeometrySummary *geometry) {
    unsigned status = result == 0 ? 0xFF20C060 : 0xFFE03030;
    float subw = scene_submeshes > 94 ? 800.0f :
                 (float)scene_submeshes * 800.0f / 94.0f;
    float vertw = scene_gif_vertices > 6000 ? 800.0f :
                  (float)scene_gif_vertices * 800.0f / 6000.0f;

    vita2d_draw_rectangle(30, 24, 900, 62, 0xFF151515);
    vita2d_draw_rectangle(50, 40, 840, 16, status);
    vita2d_draw_rectangle(50, 82, 840, 8, 0xFF303030);
    vita2d_draw_rectangle(50, 82, subw, 8, 0xFF50C0FF);
    vita2d_draw_rectangle(50, 96, 840, 8, 0xFF303030);
    vita2d_draw_rectangle(50, 96, vertw, 8, 0xFFC050FF);

    (void)info;
    (void)entry;
    (void)model;
    (void)geometry;
}

int main(void) {
    SceCtrlData pad;
    TsP5ckInfo info;
    TsP5ckEntry entry;
    TsFpResourceSummary resource;
    TsFpModelHeader model;
    TsFpGeometrySummary geometry;
    TsFpVifSummary vif_summary;
    TsFpGifSummary gif_summary;
    vita2d_color_vertex *screen = NULL;
    int result;

    memset(&pad, 0, sizeof(pad));
    memset(&info, 0, sizeof(info));
    memset(&entry, 0, sizeof(entry));
    memset(&resource, 0, sizeof(resource));
    memset(&model, 0, sizeof(model));
    memset(&geometry, 0, sizeof(geometry));
    memset(&vif_summary, 0, sizeof(vif_summary));
    memset(&gif_summary, 0, sizeof(gif_summary));

    result = load_scene(&info, &entry, &resource, &model, &geometry,
                        &vif_summary, &gif_summary);

    screen = (vita2d_color_vertex *)malloc(sizeof(vita2d_color_vertex) *
                                            MAX_TRI_VERTICES);
    if (!screen) result = -30;

    vita2d_init();

    for (;;) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        if (pad.buttons & SCE_CTRL_START) break;
        if (pad.buttons & SCE_CTRL_LEFT) scene_angle -= 0.035f;
        if (pad.buttons & SCE_CTRL_RIGHT) scene_angle += 0.035f;

        vita2d_start_drawing();
        vita2d_clear_screen();
        vita2d_draw_rectangle(0, 0, 960, 544, 0xFF080A10);
        draw_status(result, &info, &entry, &model, &geometry);

        if (screen && scene_vertex_count) {
            rebuild_screen_vertices(screen, scene_angle);
            vita2d_draw_array(SCE_GXM_PRIMITIVE_TRIANGLES, screen,
                              scene_vertex_count);
        }

        vita2d_end_drawing();
        vita2d_swap_buffers();
        sceDisplayWaitVblankStart();
    }

    vita2d_fini();
    free(screen);
    sceKernelExitProcess(0);
    return 0;
}
