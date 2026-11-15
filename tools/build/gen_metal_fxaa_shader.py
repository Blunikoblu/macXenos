#!/usr/bin/env python3
"""Regenerates src/xenia/gpu/metal/metal_fxaa_shader.h.

The Metal backend compiles its swap shaders from Metal Shading Language
strings at run time, so it cannot include third_party/fxaa/FXAA3_11.h the way
fxaa.cs.hlsl does. This runs the C preprocessor over that header with the
porting macros set for Metal (the "FXAA Quality - PC" path, the same presets
and thresholds as fxaa.cs.hlsl and fxaa_extreme.cs.hlsl) and writes the
result as two string constants.

Luma: the D3D12 and Vulkan paths store perceptual luma in the alpha of the
gamma pass's output. Here the texture fetch macro computes it from the
fetched colour instead, with the same weights, so the gamma shader (which
conformance pins) is untouched.

Usage: python3 tools/build/gen_metal_fxaa_shader.py
"""

import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FXAA = os.path.join(ROOT, "third_party", "fxaa", "FXAA3_11.h")
OUT = os.path.join(ROOT, "src", "xenia", "gpu", "metal", "metal_fxaa_shader.h")

PRELUDE = r"""
#define FXAA_PC 1
#define FXAA_GREEN_AS_LUMA 0
#define FXAA_GATHER4_ALPHA 0
#define FXAA_FAST_PIXEL_OFFSET 0
#define FxaaInt2 int2
#define FxaaTex XeFxaaTex
#define FxaaTexTop(t, p) xe_fxaa_fetch(t, p)
#define FxaaTexOff(t, p, o, r) xe_fxaa_fetch(t, (p) + float2(o) * (r))
"""

HEAD = r"""#include <metal_stdlib>
using namespace metal;
struct XeFxaaTex { texture2d<float> tex; sampler smpl; };
// Colour with perceptual luma in alpha (the input is already gamma-encoded).
static inline float4 xe_fxaa_fetch(XeFxaaTex t, float2 p) {
  float4 c = t.tex.sample(t.smpl, p, level(0.0));
  c.w = dot(c.rgb, float3(0.299, 0.587, 0.114));
  return c;
}
"""

TAIL = r"""
struct XeFxaaVOut { float4 pos [[position]]; };
vertex XeFxaaVOut xe_fxaa_vs(uint vid [[vertex_id]]) {
  XeFxaaVOut o;
  float2 t = float2(float((vid << 1) & 2), float(vid & 2));
  o.pos = float4(t * 2.0 - 1.0, 0.0, 1.0);
  return o;
}
fragment float4 xe_fxaa_fs(XeFxaaVOut in [[stage_in]],
                           texture2d<float> src [[texture(0)]],
                           sampler smp [[sampler(0)]]) {
  float2 size_inv =
      1.0 / float2(float(src.get_width()), float(src.get_height()));
  XeFxaaTex t;
  t.tex = src;
  t.smpl = smp;
  // Alpha is forced to 1: nothing downstream needs the luma.
  return float4(
      FxaaPixelShader(in.pos.xy * size_inv, float4(0.0), t, t, t, size_inv,
                      float4(0.0), float4(0.0), float4(0.0), %(subpix)s,
                      %(edge)s, %(edge_min)s, 8.0, 0.125, 0.05, float4(0.0))
          .rgb,
      1.0);
}
"""

# (constant name, preset, subpix, edge threshold, edge threshold min): the
# values of fxaa.cs.hlsl and fxaa_extreme.cs.hlsl.
VARIANTS = [
    ("kMetalFxaaShader", 12, "0.75", "0.166", "0.0833"),
    ("kMetalFxaaExtremeShader", 39, "1.0", "0.063", "0.0312"),
]


def preprocess(preset):
    source = PRELUDE + "#define FXAA_QUALITY__PRESET %d\n" % preset
    source += '#include "%s"\n' % FXAA
    result = subprocess.run(
        ["clang", "-E", "-P", "-x", "c", "-"],
        input=source, capture_output=True, text=True, check=True)
    lines = [line.rstrip() for line in result.stdout.splitlines()]
    return "\n".join(line for line in lines if line.strip()) + "\n"


def license_notice():
    """The copyright notice and license terms at the top of FXAA3_11.h."""
    lines = []
    with open(FXAA) as f:
        for line in f:
            if not line.startswith("//"):
                break
            if line.startswith("//---") or "File:" in line or "SDK Version" in line:
                continue
            if "Email:" in line or "Site:" in line:
                continue
            lines.append(line.rstrip() + "\n")
    while lines and lines[0].strip() == "//":
        lines.pop(0)
    while lines and lines[-1].strip() == "//":
        lines.pop()
    return "".join(lines)


def main():
    parts = []
    for name, preset, subpix, edge, edge_min in VARIANTS:
        body = HEAD + preprocess(preset) + TAIL % {
            "subpix": subpix, "edge": edge, "edge_min": edge_min}
        assert ')METAL"' not in body
        parts.append('static NSString* const %s = @R"METAL(\n%s)METAL";\n' %
                     (name, body))
    with open(OUT, "w") as f:
        f.write(HEADER_TOP % {"notice": license_notice()})
        f.write("\n".join(parts))
        f.write(HEADER_BOTTOM)
    return 0


HEADER_TOP = r"""/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_GPU_METAL_METAL_FXAA_SHADER_H_
#define XENIA_GPU_METAL_METAL_FXAA_SHADER_H_

// GENERATED by tools/build/gen_metal_fxaa_shader.py from
// third_party/fxaa/FXAA3_11.h -- do not edit by hand.
//
// NVIDIA FXAA 3.11 by Timothy Lottes, "FXAA Quality - PC", preprocessed for
// the Metal Shading Language. Its notice, from that file:
//
%(notice)s//
// kMetalFxaaShader is quality preset 12 (fxaa.cs.hlsl) and
// kMetalFxaaExtremeShader preset 39 (fxaa_extreme.cs.hlsl). Each has the
// entry points xe_fxaa_vs and xe_fxaa_fs: a full-target triangle that reads
// the gamma-encoded guest output from texture 0 through a linear clamp
// sampler at sampler 0.

#ifndef __OBJC__
#error "metal_fxaa_shader.h is Objective-C only; include it from .mm"
#endif

#import <Foundation/Foundation.h>

namespace xe {
namespace gpu {
namespace metal {

"""

HEADER_BOTTOM = r"""
}  // namespace metal
}  // namespace gpu
}  // namespace xe

#endif  // XENIA_GPU_METAL_METAL_FXAA_SHADER_H_
"""

if __name__ == "__main__":
    sys.exit(main())
