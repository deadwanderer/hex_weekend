#ifndef HEX_H
#define HEX_H

#include "HandmadeMath/HandmadeMath.h"
#include "sokol_gfx.h"
#include "shaders.glsl.h"
#include <stdlib.h>

/*  ====  METRICS  ==== */
typedef struct _Metrics {
  const float OuterRadius = 10.0f;
  const float InnerRadius = OuterRadius * 0.866025404f;
} _Metrics;
static _Metrics Metrics;

static hmm_vec3 Corners[7] = {
    HMM_Vec3(0.0f, 0.0f, Metrics.OuterRadius),
    HMM_Vec3(Metrics.InnerRadius, 0.0f, 0.5f * Metrics.OuterRadius),
    HMM_Vec3(Metrics.InnerRadius, 0.0f, -0.5f * Metrics.OuterRadius),
    HMM_Vec3(0.0f, 0.0f, -Metrics.OuterRadius),
    HMM_Vec3(-Metrics.InnerRadius, 0.0f, -0.5f * Metrics.OuterRadius),
    HMM_Vec3(-Metrics.InnerRadius, 0.0f, 0.5f * Metrics.OuterRadius),
    HMM_Vec3(0.0f, 0.0f, Metrics.OuterRadius)};

/*  ====  CELLS  ==== */
typedef struct _HexCell {
  hmm_vec3 Position;
} HexCell;

/*  ====  GRID  ==== */
typedef struct _HexGrid {
  const int Width = 6;
  const int Height = 6;
  HexCell* Cells;
} HexGrid;

typedef struct _GridRender {
} GridRender;

void create_cell(HexGrid* grid, int x, int z, int i) {
  grid->Cells[i].Position = HMM_Vec3(x * 10.0f, 0.0f, z * 10.0f);
}

void grid_initialize(HexGrid* grid) {
  grid->Cells = (HexCell*)malloc(grid->Width * grid->Height * sizeof(HexCell));
  for (int z = 0, i = 0; z < grid->Height; z++) {
    for (int x = 0; x < grid->Width; x++) {
      create_cell(grid, x, z, i++);
    }
  }
}

/*  ====  RENDERING  ==== */
#endif  // HEX_H