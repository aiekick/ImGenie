#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "ImGenie.h"

#include <cfloat>
#include <cmath>
#include <cstring>

// ---------- Current context ----------

static ImGenieContext* s_ctx = nullptr;

// ---------- Lattice helpers ----------

// Compute a tangent-like interpolation point between two ImVec2.
// Returns a point whose x = v0.x and y is lerped by aPercent along the y delta.
// Used to build Bezier control points for the vertical Coons patch S-curve.
static ImVec2 s_tg(const ImVec2& arV0, const ImVec2& arV1, float aPercent) { return (arV1 - arV0) * ImVec2(0.0f, aPercent) + arV0; }

// Evaluate a cubic Bezier curve at parameter aT in [0,1].
// p1..p4 are the 4 control points (scalar, not 2D).
// Uses double precision internally to avoid precision loss on large values.
static float s_bezierCubicCalc(float aP1, float aP2, float aP3, float aP4, float aT) {
    const auto u = 1.0 - static_cast<double>(aT);
    const auto w1 = u * u * u;
    const auto w2 = 3.0 * u * u * aT;
    const auto w3 = 3.0 * u * aT * aT;
    const auto w4 = static_cast<double>(aT) * aT * aT;
    return static_cast<float>(w1 * aP1 + w2 * aP2 + w3 * aP3 + w4 * aP4);
}

// Draw a textured Coons patch mesh with animated UV mapping.
//
// The mesh is defined by 4 corner points (p00=TL, p10=TR, p01=BL, p11=BR).
// Edges are interpolated using cubic Bezier S-curves (endpoints used as control points).
// The interior is bilinearly blended between the 4 edge curves (Coons patch).
//
// aStartAnimT / aEndAnimT: visible portion of the mesh along the primary axis [0..1].
//   Only the rows/columns within this range are generated. This is how the genie "tail"
//   disappears as the animation progresses.
//
// aAnimMode controls UV mapping:
//   - Compress: UV remapped to [0..1] within visible range (texture shrinks with mesh)
//   - Sliding: UV slides through the texture (texture scrolls away)
//
// aHorizontal: if true, primary axis is horizontal (Left/Right genie).
//   Columns are iterated left-to-right instead of rows top-to-bottom.
//
// aFlipPrimaryUv: reverses UV along primary axis (for Bottom/Right sides where
//   the trailing edge is at the end of the texture).
static void s_drawTexturedCoonsMeshPrimAnim(ImDrawList* apDrawList,
                                            const ImTextureRef& arTexture,
                                            const ImVec2& arP00,
                                            const ImVec2& arP10,
                                            const ImVec2& arP01,
                                            const ImVec2& arP11,
                                            int32_t aCellsH,
                                            int32_t aCellsV,
                                            float aStartAnimT,
                                            float aEndAnimT,
                                            ImGenieAnimMode aAnimMode,
                                            bool aHorizontal = false,
                                            bool aFlipPrimaryUv = false,
                                            bool aFlipV = false) {
    if ((!apDrawList) || (aCellsV < 1) || (aCellsH < 1) || (aStartAnimT > aEndAnimT)) { return; }
    const auto tint = IM_COL32(255, 255, 255, 255);

    // ==================== Horizontal mode (Left/Right genie) ====================
    // Primary axis = columns (left to right), secondary = rows (top to bottom).
    if (aHorizontal) {
        const auto visibleCols = static_cast<uint32_t>(ImMax(std::ceil((aEndAnimT - aStartAnimT) * static_cast<float>(aCellsH)), 1.0f));
        const auto vertsPerCol = static_cast<uint32_t>(aCellsV + 1);
        const auto colCount = visibleCols + 1U;
        const auto vertexCount = colCount * vertsPerCol;
        const auto indexCount = visibleCols * static_cast<uint32_t>(aCellsV) * 6U;
        apDrawList->PushTexture(arTexture);
        apDrawList->PrimReserve(indexCount, vertexCount);
        ImDrawIdx prevColStart = 0;
        auto currColStart = static_cast<ImDrawIdx>(apDrawList->_VtxCurrentIdx);
        for (uint32_t colIdx = 0; colIdx < colCount; ++colIdx) {
            const auto colFactor = (colCount > 1) ? (static_cast<float>(colIdx) / static_cast<float>(colCount - 1)) : 0.0f;
            const auto patchParam = aStartAnimT + (aEndAnimT - aStartAnimT) * colFactor;
            // S-curve Bezier for y at top edge (p00->p10) and bottom edge (p01->p11)
            const auto tpt = s_bezierCubicCalc(arP00.y, arP00.y, arP10.y, arP10.y, patchParam);
            const auto bpt = s_bezierCubicCalc(arP01.y, arP01.y, arP11.y, arP11.y, patchParam);
            auto uvPrimary = patchParam - aStartAnimT;
            if (aAnimMode == ImGenieAnimMode_Compress) {
                uvPrimary /= (aEndAnimT - aStartAnimT);
            } else if (aAnimMode == ImGenieAnimMode_Sliding) {
                uvPrimary /= aEndAnimT;
            }
            if (aFlipPrimaryUv) { uvPrimary = 1.0f - uvPrimary; }
            for (uint32_t rowIdx = 0; rowIdx < vertsPerCol; ++rowIdx) {
                const auto v = (vertsPerCol > 1) ? (static_cast<float>(rowIdx) / static_cast<float>(vertsPerCol - 1)) : 0.0f;
                const auto y = tpt + (bpt - tpt) * v;
                // S-curve Bezier for x at left/right edges, then bilinear blend
                const auto leftX = s_bezierCubicCalc(arP00.x, arP00.x, arP01.x, arP01.x, v);
                const auto rightX = s_bezierCubicCalc(arP10.x, arP10.x, arP11.x, arP11.x, v);
                const auto x = leftX + (rightX - leftX) * patchParam;
                const auto finalV = aFlipV ? (1.0f - v) : v;
                apDrawList->PrimWriteVtx(ImVec2(x, y), ImVec2(uvPrimary, finalV), tint);
            }
            // Emit 2 triangles per cell between this column and the previous one
            if (colIdx > 0) {
                for (int32_t rowIdx = 0; rowIdx < aCellsV; ++rowIdx) {
                    const auto tl = prevColStart + static_cast<ImDrawIdx>(rowIdx);
                    const auto bl = tl + 1;
                    const auto tr = currColStart + static_cast<ImDrawIdx>(rowIdx);
                    const auto br = tr + 1;
                    apDrawList->PrimWriteIdx(tl);
                    apDrawList->PrimWriteIdx(tr);
                    apDrawList->PrimWriteIdx(br);
                    apDrawList->PrimWriteIdx(tl);
                    apDrawList->PrimWriteIdx(br);
                    apDrawList->PrimWriteIdx(bl);
                }
            }
            prevColStart = currColStart;
            currColStart = static_cast<ImDrawIdx>(currColStart + vertsPerCol);
        }
        apDrawList->PopTexture();
        return;
    }
    // ==================== Vertical mode (Top/Bottom genie) ====================
    // Primary axis = rows (top to bottom), secondary = columns (left to right).
    const auto visibleCellsV = static_cast<uint32_t>(ImMax(std::ceil((aEndAnimT - aStartAnimT) * static_cast<float>(aCellsV)), 1.0f));
    const auto vertsPerRow = static_cast<uint32_t>(aCellsH + 1);
    const auto vertsPerCol = visibleCellsV + 1U;
    const auto vertexCount = vertsPerRow * vertsPerCol;
    const auto indexCount = visibleCellsV * static_cast<uint32_t>(aCellsH) * 6U;
    apDrawList->PushTexture(arTexture);
    apDrawList->PrimReserve(indexCount, vertexCount);
    // Bezier tangent control points for left/right edges (gives the S-curve shape)
    const auto bezierTangentPercent = 0.4f;
    const auto lxC1 = s_tg(arP00, arP01, bezierTangentPercent).x;
    const auto lxC2 = s_tg(arP01, arP00, bezierTangentPercent).x;
    const auto rxC1 = s_tg(arP10, arP11, bezierTangentPercent).x;
    const auto rxC2 = s_tg(arP11, arP10, bezierTangentPercent).x;
    const auto tyC1 = arP00.y, tyC2 = arP10.y;
    const auto byC1 = arP01.y, byC2 = arP11.y;
    ImDrawIdx prevRowStart = 0;
    auto currRowStart = static_cast<ImDrawIdx>(apDrawList->_VtxCurrentIdx);
    for (uint32_t rowIdx = 0; rowIdx < vertsPerCol; ++rowIdx) {
        const auto rowFactor = (vertsPerCol > 1) ? (static_cast<float>(rowIdx) / static_cast<float>(vertsPerCol - 1)) : 0.0f;
        const auto patchParam = aStartAnimT + (aEndAnimT - aStartAnimT) * rowFactor;
        // Bezier-interpolated x at left and right edges for this row
        const auto lpt = s_bezierCubicCalc(arP00.x, lxC1, lxC2, arP01.x, patchParam);
        const auto rpt = s_bezierCubicCalc(arP10.x, rxC1, rxC2, arP11.x, patchParam);
        auto uvY = patchParam - aStartAnimT;
        if (aAnimMode == ImGenieAnimMode_Compress) {
            uvY /= aEndAnimT - aStartAnimT;
        } else if (aAnimMode == ImGenieAnimMode_Sliding) {
            uvY /= aEndAnimT;
        }
        if (aFlipPrimaryUv) { uvY = 1.0f - uvY; }
        for (uint32_t colIdx = 0; colIdx < vertsPerRow; ++colIdx) {
            const auto u = (vertsPerRow > 1) ? (static_cast<float>(colIdx) / static_cast<float>(vertsPerRow - 1)) : 0.0f;
            // Bilinear blend between left/right edge x, then Coons patch blend for y
            const auto x = lpt + (rpt - lpt) * u;
            const auto topY = s_bezierCubicCalc(arP00.y, tyC1, tyC2, arP10.y, u);
            const auto botY = s_bezierCubicCalc(arP01.y, byC1, byC2, arP11.y, u);
            const auto y = topY + (botY - topY) * patchParam;
            const auto finalUvY = aFlipV ? (1.0f - uvY) : uvY;
            apDrawList->PrimWriteVtx(ImVec2(x, y), ImVec2(u, finalUvY), tint);
        }
        // Emit 2 triangles per cell between this row and the previous one
        if (rowIdx > 0) {
            for (int32_t colIdx = 0; colIdx < aCellsH; ++colIdx) {
                const auto tl = prevRowStart + static_cast<ImDrawIdx>(colIdx);
                const auto tr = tl + 1;
                const auto bl = currRowStart + static_cast<ImDrawIdx>(colIdx);
                const auto br = bl + 1;
                apDrawList->PrimWriteIdx(tl);
                apDrawList->PrimWriteIdx(tr);
                apDrawList->PrimWriteIdx(br);
                apDrawList->PrimWriteIdx(tl);
                apDrawList->PrimWriteIdx(br);
                apDrawList->PrimWriteIdx(bl);
            }
        }
        prevRowStart = currRowStart;
        currRowStart = static_cast<ImDrawIdx>(currRowStart + vertsPerRow);
    }
    apDrawList->PopTexture();
}

// Draw full texture on 4 deformed corners (used for wobbly move rendering).
// Simply calls the Coons mesh with anim range [0..1] = full texture visible.
static void s_latticeDraw(ImDrawList* apDrawList,
                          const ImTextureRef& arTexture,
                          const ImVec2& arP00,
                          const ImVec2& arP10,
                          const ImVec2& arP01,
                          const ImVec2& arP11,
                          int32_t aCellsH,
                          int32_t aCellsV,
                          ImGenieAnimMode aAnimMode,
                          bool aFlipV = false) {
    if ((!apDrawList) || (arTexture._TexID == 0)) { return; }
    s_drawTexturedCoonsMeshPrimAnim(apDrawList, arTexture, arP00, arP10, arP01, arP11, aCellsH, aCellsV, 0.0f, 1.0f, aAnimMode, false, false, aFlipV);
}

// Page curl / scroll-unroll effect.
// A straight fold line sweeps diagonally from BL to TR.
// Vertices in front of the fold: flat (normal position, visible).
// Vertices past the fold: projected onto the fold line (rolled up).
// This simulates a scroll/roll being unrolled across the window.
//
// ---------- Simple quad transitions (Fade, Scale, Slide) ----------

// Helper: draw a single textured quad (4 verts, 6 indices)
static void s_drawTexturedQuad(ImDrawList* apDrawList,
                               const ImTextureRef& arTexture,
                               const ImVec2& arCapturedSize,
                               const ImVec2& arTL, const ImVec2& arTR,
                               const ImVec2& arBL, const ImVec2& arBR,
                               ImU32 aTint,
                               bool aFlipV = false) {
    const auto uvTL = ImVec2(0.0f, aFlipV ? 1.0f : 0.0f);
    const auto uvTR = ImVec2(1.0f, aFlipV ? 1.0f : 0.0f);
    const auto uvBL = ImVec2(0.0f, aFlipV ? 0.0f : 1.0f);
    const auto uvBR = ImVec2(1.0f, aFlipV ? 0.0f : 1.0f);
    apDrawList->PushTexture(arTexture);
    apDrawList->PrimReserve(6, 4);
    apDrawList->PrimQuadUV(arTL, arTR, arBR, arBL, uvTL, uvTR, uvBR, uvBL, aTint);
    apDrawList->PopTexture();
}

// Fade: alpha transition. aAnimT: 0 = invisible, 1 = fully visible.
static void s_fadeAnimate(ImDrawList* apDrawList,
                          const ImTextureRef& arTexture,
                          const ImVec2& arCapturedSize,
                          float aAnimT,
                          const ImRect& arSource,
                          bool aFlipV = false) {
    if (!apDrawList || arTexture._TexID == 0 || aAnimT <= 0.0f) { return; }
    const auto t = ImClamp(aAnimT, 0.0f, 1.0f);
    const auto eased = t * t * (3.0f - 2.0f * t);
    const auto alpha = static_cast<ImU8>(eased * 255.0f);
    const auto tint = IM_COL32(255, 255, 255, alpha);
    s_drawTexturedQuad(apDrawList, arTexture, arCapturedSize,
                       arSource.Min, ImVec2(arSource.Max.x, arSource.Min.y),
                       ImVec2(arSource.Min.x, arSource.Max.y), arSource.Max,
                       tint, aFlipV);
}

// Scale: zoom from/to center. aAnimT: 0 = collapsed to center, 1 = fully visible.
static void s_scaleAnimate(ImDrawList* apDrawList,
                            const ImTextureRef& arTexture,
                            const ImVec2& arCapturedSize,
                            float aAnimT,
                            const ImRect& arSource,
                            bool aFlipV = false) {
    if (!apDrawList || arTexture._TexID == 0 || aAnimT <= 0.0f) { return; }
    const auto t = ImClamp(aAnimT, 0.0f, 1.0f);
    const auto eased = t * t * (3.0f - 2.0f * t);
    const auto center = arSource.GetCenter();
    const auto halfW = arSource.GetWidth() * 0.5f * eased;
    const auto halfH = arSource.GetHeight() * 0.5f * eased;
    const ImVec2 tl(center.x - halfW, center.y - halfH);
    const ImVec2 tr(center.x + halfW, center.y - halfH);
    const ImVec2 bl(center.x - halfW, center.y + halfH);
    const ImVec2 br(center.x + halfW, center.y + halfH);
    const auto tint = IM_COL32(255, 255, 255, 255);
    s_drawTexturedQuad(apDrawList, arTexture, arCapturedSize, tl, tr, bl, br, tint, aFlipV);
}

// Resolve auto slide directions to a concrete edge or corner.
static ImGenieSlideDir s_resolveSlideDir(const ImRect& arSource, ImGenieSlideDir aDir = ImGenieSlideDir_Auto) {
    const auto& displaySize = ImGui::GetIO().DisplaySize;
    const auto center = arSource.GetCenter();
    const float distLeft = center.x;
    const float distRight = displaySize.x - center.x;
    const float distTop = center.y;
    const float distBottom = displaySize.y - center.y;
    if (aDir == ImGenieSlideDir_AutoEdge || aDir == ImGenieSlideDir_Auto) {
        const float minEdge = ImMin(ImMin(distLeft, distRight), ImMin(distTop, distBottom));
        ImGenieSlideDir bestEdge = ImGenieSlideDir_Down;
        if (minEdge == distLeft) bestEdge = ImGenieSlideDir_Left;
        else if (minEdge == distRight) bestEdge = ImGenieSlideDir_Right;
        else if (minEdge == distTop) bestEdge = ImGenieSlideDir_Up;
        if (aDir == ImGenieSlideDir_AutoEdge) return bestEdge;
        // Auto: pick whichever is closer — edge or corner
        const float minCorner = ImMin(ImMin(distLeft + distTop, distRight + distTop), ImMin(distLeft + distBottom, distRight + distBottom));
        if (minEdge <= minCorner) return bestEdge;
        // Fall through to corner resolution
    }
    // AutoCorner or Auto fallback
    if (aDir == ImGenieSlideDir_AutoCorner || aDir == ImGenieSlideDir_Auto) {
        const float dTL = distLeft + distTop;
        const float dTR = distRight + distTop;
        const float dBL = distLeft + distBottom;
        const float dBR = distRight + distBottom;
        const float minC = ImMin(ImMin(dTL, dTR), ImMin(dBL, dBR));
        if (minC == dTL) return ImGenieSlideDir_TopLeft;
        if (minC == dTR) return ImGenieSlideDir_TopRight;
        if (minC == dBL) return ImGenieSlideDir_BottomLeft;
        return ImGenieSlideDir_BottomRight;
    }
    return aDir;  // Already concrete
}

// Slide: translate off-screen. aAnimT: 0 = fully off-screen, 1 = fully visible.
// aDir: ImGenieSlideDir — Auto picks the closest viewport edge.
// The window slides completely outside the viewport (display) bounds.
static void s_slideAnimate(ImDrawList* apDrawList,
                           const ImTextureRef& arTexture,
                           const ImVec2& arCapturedSize,
                           float aAnimT,
                           const ImRect& arSource,
                           const ImGenieSlideParams& arSlideParams,
                           bool aFlipV = false) {
    if (!apDrawList || arTexture._TexID == 0 || aAnimT <= 0.0f) { return; }
    const auto dir = s_resolveSlideDir(arSource, arSlideParams.dir);
    const auto t = ImClamp(aAnimT, 0.0f, 1.0f);
    const auto eased = t * t * (3.0f - 2.0f * t);
    const auto& displaySize = ImGui::GetIO().DisplaySize;
    // Full distance to push window off-screen (per axis)
    float fullDistX = 0.0f, fullDistY = 0.0f;
    if (dir == ImGenieSlideDir_Left || dir == ImGenieSlideDir_TopLeft || dir == ImGenieSlideDir_BottomLeft)
        fullDistX = -arSource.Max.x;
    else if (dir == ImGenieSlideDir_Right || dir == ImGenieSlideDir_TopRight || dir == ImGenieSlideDir_BottomRight)
        fullDistX = displaySize.x - arSource.Min.x;
    if (dir == ImGenieSlideDir_Up || dir == ImGenieSlideDir_TopLeft || dir == ImGenieSlideDir_TopRight)
        fullDistY = -arSource.Max.y;
    else if (dir == ImGenieSlideDir_Down || dir == ImGenieSlideDir_BottomLeft || dir == ImGenieSlideDir_BottomRight)
        fullDistY = displaySize.y - arSource.Min.y;
    const ImVec2 fullOff(fullDistX * (1.0f - eased), fullDistY * (1.0f - eased));
    // p00=TL, p10=TR, p01=BL, p11=BR
    ImVec2 p00 = arSource.Min;
    ImVec2 p10 = ImVec2(arSource.Max.x, arSource.Min.y);
    ImVec2 p01 = ImVec2(arSource.Min.x, arSource.Max.y);
    ImVec2 p11 = arSource.Max;
    if (arSlideParams.wobbly) {
        // Leading (pinned) corners move, trailing corners stay — Coons Bezier S-curve handles the elastic stretch
        bool pinned[4] = {false, false, false, false};  // TL, TR, BL, BR
        switch (dir) {
            case ImGenieSlideDir_Left:        pinned[0] = pinned[2] = true; break;
            case ImGenieSlideDir_Right:       pinned[1] = pinned[3] = true; break;
            case ImGenieSlideDir_Up:          pinned[0] = pinned[1] = true; break;
            case ImGenieSlideDir_Down:        pinned[2] = pinned[3] = true; break;
            case ImGenieSlideDir_TopLeft:     pinned[0] = true; break;
            case ImGenieSlideDir_TopRight:    pinned[1] = true; break;
            case ImGenieSlideDir_BottomLeft:  pinned[2] = true; break;
            case ImGenieSlideDir_BottomRight: pinned[3] = true; break;
            default: break;
        }
        ImVec2* corners[4] = {&p00, &p10, &p01, &p11};
        for (int32_t i = 0; i < 4; ++i) {
            if (pinned[i]) *corners[i] = *corners[i] + fullOff;
        }
    } else {
        // Uniform: all 4 corners move together
        p00 = p00 + fullOff; p10 = p10 + fullOff; p01 = p01 + fullOff; p11 = p11 + fullOff;
    }
    const auto cellsH = arSlideParams.wobbly && fullDistX != 0.0f ? ImMax(arSlideParams.spring.cellsH, 1) : 1;
    const auto cellsV = arSlideParams.wobbly && fullDistY != 0.0f ? ImMax(arSlideParams.spring.cellsV, 1) : 1;
    s_latticeDraw(apDrawList, arTexture, p00, p10, p01, p11, cellsH, cellsV, ImGenieAnimMode_Sliding, aFlipV);
}

// ---------- Page Curl transition ----------

// aAnimT: 0 = fully rolled (everything on aOrigin point), 1 = fully flat (visible).
static void s_pageCurlAnimate(ImDrawList* apDrawList,
                              const ImTextureRef& arTexture,
                              const ImVec2& arCapturedSize,
                              float aAnimT,
                              const ImRect& arSource,
                              int32_t aCellsH,
                              int32_t aCellsV,
                              ImGeniePageCurlOrigin aOrigin = ImGeniePageCurlOrigin_BottomLeft,
                              bool aFlipV = false) {
    if (!apDrawList || arTexture._TexID == 0 || aAnimT <= 0.0f) { return; }
    const auto winW = arSource.GetWidth();
    const auto winH = arSource.GetHeight();
    if (winW <= 0.0f || winH <= 0.0f) { return; }
    // Smoothstep easing
    const auto t = ImClamp(aAnimT, 0.0f, 1.0f);
    const auto eased = t * t * (3.0f - 2.0f * t);

    // Compute start corner and sweep direction based on origin
    const auto diagLen = sqrtf(winW * winW + winH * winH);
    ImVec2 startCorner;
    ImVec2 sweepDir;
    float sweepLen = diagLen;
    switch (aOrigin) {
        case ImGeniePageCurlOrigin_BottomLeft:
            startCorner = ImVec2(arSource.Min.x, arSource.Max.y);
            sweepDir = ImVec2(winW / diagLen, -winH / diagLen);
            break;
        case ImGeniePageCurlOrigin_BottomRight:
            startCorner = ImVec2(arSource.Max.x, arSource.Max.y);
            sweepDir = ImVec2(-winW / diagLen, -winH / diagLen);
            break;
        case ImGeniePageCurlOrigin_TopLeft:
            startCorner = ImVec2(arSource.Min.x, arSource.Min.y);
            sweepDir = ImVec2(winW / diagLen, winH / diagLen);
            break;
        case ImGeniePageCurlOrigin_TopRight:
            startCorner = ImVec2(arSource.Max.x, arSource.Min.y);
            sweepDir = ImVec2(-winW / diagLen, winH / diagLen);
            break;
        case ImGeniePageCurlOrigin_Bottom:
            startCorner = ImVec2(arSource.Min.x, arSource.Max.y);
            sweepDir = ImVec2(0.0f, -1.0f);
            sweepLen = winH;
            break;
        case ImGeniePageCurlOrigin_Top:
            startCorner = ImVec2(arSource.Min.x, arSource.Min.y);
            sweepDir = ImVec2(0.0f, 1.0f);
            sweepLen = winH;
            break;
        case ImGeniePageCurlOrigin_Left:
            startCorner = ImVec2(arSource.Min.x, arSource.Min.y);
            sweepDir = ImVec2(1.0f, 0.0f);
            sweepLen = winW;
            break;
        case ImGeniePageCurlOrigin_Right:
            startCorner = ImVec2(arSource.Max.x, arSource.Min.y);
            sweepDir = ImVec2(-1.0f, 0.0f);
            sweepLen = winW;
            break;
        default:
            startCorner = ImVec2(arSource.Min.x, arSource.Max.y);
            sweepDir = ImVec2(winW / diagLen, -winH / diagLen);
            break;
    }

    // Fold line position along the sweep
    const auto foldDist = eased * sweepLen;

    // --- Mesh ---
    const auto tint = IM_COL32(255, 255, 255, 255);
    const auto vertsPerRow = static_cast<uint32_t>(aCellsH + 1);
    const auto vertsPerCol = static_cast<uint32_t>(aCellsV + 1);
    const auto vertexCount = vertsPerRow * vertsPerCol;
    const auto indexCount = static_cast<uint32_t>(aCellsH) * static_cast<uint32_t>(aCellsV) * 6U;
    apDrawList->PushTexture(arTexture);
    apDrawList->PrimReserve(indexCount, vertexCount);
    auto currRowStart = static_cast<ImDrawIdx>(apDrawList->_VtxCurrentIdx);
    ImDrawIdx prevRowStart = 0;

    // Fold line point
    const ImVec2 foldPt(startCorner.x + foldDist * sweepDir.x, startCorner.y + foldDist * sweepDir.y);

    for (uint32_t rowIdx = 0; rowIdx < vertsPerCol; ++rowIdx) {
        const auto vf = (vertsPerCol > 1) ? (static_cast<float>(rowIdx) / static_cast<float>(vertsPerCol - 1)) : 0.0f;
        for (uint32_t colIdx = 0; colIdx < vertsPerRow; ++colIdx) {
            const auto uf = (vertsPerRow > 1) ? (static_cast<float>(colIdx) / static_cast<float>(vertsPerRow - 1)) : 0.0f;

            // Original screen position
            const auto sx = arSource.Min.x + uf * winW;
            const auto sy = arSource.Min.y + vf * winH;

            // Signed distance from fold point along sweep direction
            const auto dx = sx - foldPt.x;
            const auto dy = sy - foldPt.y;
            const auto sweepDist = dx * sweepDir.x + dy * sweepDir.y;

            const auto uvX = uf;
            const auto uvY = aFlipV ? (1.0f - vf) : vf;

            if (sweepDist <= 0.0f) {
                // In front of fold: flat, normal position
                apDrawList->PrimWriteVtx(ImVec2(sx, sy), ImVec2(uvX, uvY), tint);
            } else {
                // Past fold: project onto the fold line (rolled up)
                // Remove the sweep component, keep perpendicular component
                auto projX = sx - sweepDist * sweepDir.x;
                auto projY = sy - sweepDist * sweepDir.y;
                // Clamp to window bounds so the fold line doesn't exceed the rect
                projX = ImClamp(projX, arSource.Min.x, arSource.Max.x);
                projY = ImClamp(projY, arSource.Min.y, arSource.Max.y);
                apDrawList->PrimWriteVtx(ImVec2(projX, projY), ImVec2(uvX, uvY), tint);
            }
        }
        if (rowIdx > 0) {
            for (int32_t colIdx = 0; colIdx < aCellsH; ++colIdx) {
                const auto tl = prevRowStart + static_cast<ImDrawIdx>(colIdx);
                const auto tr = tl + 1;
                const auto bl = currRowStart + static_cast<ImDrawIdx>(colIdx);
                const auto br = bl + 1;
                apDrawList->PrimWriteIdx(tl);
                apDrawList->PrimWriteIdx(tr);
                apDrawList->PrimWriteIdx(br);
                apDrawList->PrimWriteIdx(tl);
                apDrawList->PrimWriteIdx(br);
                apDrawList->PrimWriteIdx(bl);
            }
        }
        prevRowStart = currRowStart;
        currRowStart = static_cast<ImDrawIdx>(currRowStart + vertsPerRow);
    }
    apDrawList->PopTexture();
}

// Auto-detect which side of the button the window is on.
// Compares center positions: the dominant axis (x or y) determines the side.
static ImGenieSide s_autoDetectSide(const ImRect& arSource, const ImRect& arTarget) {
    const auto delta = arSource.GetCenter() - arTarget.GetCenter();
    if (fabsf(delta.x) > fabsf(delta.y)) { return (delta.x > 0) ? ImGenieSide_Right : ImGenieSide_Left; }
    return (delta.y > 0) ? ImGenieSide_Bottom : ImGenieSide_Top;
}

// Animate the genie effect (disappearance or appearance) on any side.
//
// The mesh has two edges:
//   - Trailing edge: stays at the source window position (the "body")
//   - Converging edge: narrows toward the target button as aAnimT increases
//
// For vertical (Top/Bottom): trailing = top row (p00/p10), converging = bottom row (p01/p11).
// For horizontal (Left/Right): trailing = left col (p00/p01), converging = right col (p10/p11).
//
// The converging edge narrows to target width over the first 30% of the anim.
// The trailing edge starts disappearing after 25% of the anim (startRatio increases).
// This creates the characteristic "sucked into the button" genie look.
static void s_latticeAnimate(ImDrawList* apDrawList,
                             const ImTextureRef& arTexture,
                             const ImVec2& arTextureSize,
                             float aAnimT,
                             const ImRect& arSource,
                             const ImRect& arTarget,
                             int32_t aCellsH,
                             int32_t aCellsV,
                             ImGenieAnimMode aAnimMode,
                             ImGenieSide aSide,
                             bool aFlipV = false) {
    if ((!apDrawList) || (arTexture._TexID == 0) || (aAnimT < 0.0f) || (aAnimT > 1.0f)) { return; }
    const auto side = (aSide == ImGenieSide_Auto) ? s_autoDetectSide(arSource, arTarget) : aSide;
    const auto horizontal = (side == ImGenieSide_Left || side == ImGenieSide_Right);
    // Bottom/Right need flipped UVs because the trailing edge is at the end of the texture
    const auto flipPrimaryUv = (side == ImGenieSide_Bottom || side == ImGenieSide_Right);
    // p00/p10 = trailing edge (or p00/p01 for horizontal)
    // convSrc = converging edge at full width (source), convTgt = at button width
    ImVec2 p00, p10, p01, p11;
    ImVec2 convSrc0, convSrc1, convTgt0, convTgt1;
    float totalDist{}, sourceDim{};
    switch (side) {
        case ImGenieSide_Top:
            p00 = arSource.Min;
            p10 = ImVec2(arSource.Max.x, arSource.Min.y);
            convSrc0 = ImVec2(arSource.Min.x, arTarget.Min.y);
            convSrc1 = ImVec2(arSource.Max.x, arTarget.Min.y);
            convTgt0 = arTarget.Min;
            convTgt1 = ImVec2(arTarget.Max.x, arTarget.Min.y);
            totalDist = arTarget.Min.y - arSource.Min.y;
            sourceDim = arSource.GetHeight();
            break;
        case ImGenieSide_Bottom:
            p00 = ImVec2(arSource.Min.x, arSource.Max.y);
            p10 = arSource.Max;
            convSrc0 = ImVec2(arSource.Min.x, arTarget.Max.y);
            convSrc1 = ImVec2(arSource.Max.x, arTarget.Max.y);
            convTgt0 = ImVec2(arTarget.Min.x, arTarget.Max.y);
            convTgt1 = arTarget.Max;
            totalDist = arSource.Max.y - arTarget.Max.y;
            sourceDim = arSource.GetHeight();
            break;
        case ImGenieSide_Left:
            p00 = arSource.Min;
            p01 = ImVec2(arSource.Min.x, arSource.Max.y);
            convSrc0 = ImVec2(arTarget.Min.x, arSource.Min.y);
            convSrc1 = ImVec2(arTarget.Min.x, arSource.Max.y);
            convTgt0 = arTarget.Min;
            convTgt1 = ImVec2(arTarget.Min.x, arTarget.Max.y);
            totalDist = arTarget.Min.x - arSource.Min.x;
            sourceDim = arSource.GetWidth();
            break;
        case ImGenieSide_Right:
        default:
            p00 = ImVec2(arSource.Max.x, arSource.Min.y);
            p01 = arSource.Max;
            convSrc0 = ImVec2(arTarget.Max.x, arSource.Min.y);
            convSrc1 = ImVec2(arTarget.Max.x, arSource.Max.y);
            convTgt0 = ImVec2(arTarget.Max.x, arTarget.Min.y);
            convTgt1 = arTarget.Max;
            totalDist = arSource.Max.x - arTarget.Max.x;
            sourceDim = arSource.GetWidth();
            break;
    }
    if (horizontal) {
        p10 = convSrc0;
        p11 = convSrc1;
    } else {
        p01 = convSrc0;
        p11 = convSrc1;
    }
    // sourceRatio: how much of the [0..1] parameter space the source window covers.
    // startRatio / endRatio define the visible portion of the mesh.
    // As anim progresses, startRatio grows (trailing edge shrinks) and endRatio approaches 1.
    const auto sourceRatio = (totalDist > 0.0f) ? (sourceDim / totalDist) : 1.0f;
    auto startRatio = 0.0f;
    auto endRatio = sourceRatio;
    if (aAnimT > 0.0f) {
        // Converging edge reaches target width over first 30%
        const auto ratio = (aAnimT < 0.3f) ? (aAnimT / 0.3f) : 1.0f;
        // Trailing edge starts shrinking after 25%
        if (aAnimT > 0.25f) { startRatio = (aAnimT - 0.25f) / (1.0f - 0.25f); }
        const auto conv0 = ImLerp(convSrc0, convTgt0, ratio);
        const auto conv1 = ImLerp(convSrc1, convTgt1, ratio);
        if (horizontal) {
            p10 = conv0;
            p11 = conv1;
        } else {
            p01 = conv0;
            p11 = conv1;
        }
        endRatio = ImLerp(sourceRatio, 1.0f, ratio);
    }
    s_drawTexturedCoonsMeshPrimAnim(
        apDrawList, arTexture, p00, p10, p01, p11, aCellsH, aCellsV, startRatio, endRatio, aAnimMode, horizontal, flipPrimaryUv, aFlipV);
}

// ---------- Debug mesh ----------

static void s_drawDebugMesh(ImDrawList* apDrawList, const ImGenieEffect& arEffect) {
    if (arEffect.params.drawDebugMesh && !apDrawList->CmdBuffer.empty()) {
        auto* pCmd = &apDrawList->CmdBuffer.front();
        ImGui::DebugNodeDrawCmdShowMeshAndBoundingBox(apDrawList, apDrawList, pCmd, true, true);
    }
}

// ---------- Utility ----------

// Recursively add DrawLists from all child windows to the given ImDrawData.
// Children are added in depth-first order (parent before its children),
// which matches ImGui's own rendering order.
static void s_addChildDrawLists(const ImGuiWindow* apWin, ImDrawData* apDrawData) {
    for (int i = 0; i < apWin->DC.ChildWindows.Size; ++i) {
        const ImGuiWindow* child = apWin->DC.ChildWindows[i];
        if (child != nullptr && child->DrawList != nullptr && child->DrawList->CmdBuffer.Size > 0) {
            apDrawData->AddDrawList(child->DrawList);
            s_addChildDrawLists(child, apDrawData);
        }
    }
}

// Build an ImDrawData from a window's DrawList and call the user-provided capture callback.
// This isolates the user from ImGui internals: they just receive (width, height, drawData).
static ImTextureRef s_captureWindow(const ImGuiWindow* apWin, ImDrawData* apMainDrawData) {
    IM_ASSERT(s_ctx && "The context is null");
    IM_ASSERT(s_ctx->createCaptureFunc && "The capture creation function is not defined");
    ImTextureRef ret{};
    if (apWin == nullptr || apWin->DrawList == nullptr) { return ret; }
    const auto w = static_cast<int32_t>(apWin->Size.x);
    const auto h = static_cast<int32_t>(apWin->Size.y);
    if (w <= 0 || h <= 0) { return ret; }
    ImDrawData offscreenData;
    offscreenData.Valid = true;
    offscreenData.DisplayPos = apWin->Pos;
    offscreenData.DisplaySize = apWin->Size;
    offscreenData.FramebufferScale = ImGui::GetIO().DisplayFramebufferScale;
    offscreenData.OwnerViewport = ImGui::GetMainViewport();
    offscreenData.Textures = (apMainDrawData != nullptr) ? apMainDrawData->Textures : nullptr;
    offscreenData.AddDrawList(apWin->DrawList);
    s_addChildDrawLists(apWin, &offscreenData);
    return s_ctx->createCaptureFunc(w, h, &offscreenData);
}

// Release the captured FBO texture via the user-provided destroy callback.
static void s_deleteEffectTexture(ImGenieEffect& aorEffect) {
    IM_ASSERT(s_ctx && "The context is null");
    IM_ASSERT(s_ctx->destroyCaptureFunc && "The capture destruction function is not defined");
    if (aorEffect.capturedTex._TexID != 0) {
        s_ctx->destroyCaptureFunc(aorEffect.capturedTex);
        aorEffect.capturedTex._TexID = 0;
    }
}

// Remove a window's DrawList from the main ImDrawData so it won't be rendered
// by the backend. This hides the real window while the captured texture is shown instead.
static void s_removeDrawListFromDrawData(ImDrawData* apDrawData, ImDrawList* apDrawList) {
    if (apDrawData == nullptr || apDrawList == nullptr) { return; }
    for (int32_t i = 0; i < apDrawData->CmdLists.Size; ++i) {
        if (apDrawData->CmdLists[i] == apDrawList) {
            apDrawData->TotalVtxCount -= apDrawList->VtxBuffer.Size;
            apDrawData->TotalIdxCount -= apDrawList->IdxBuffer.Size;
            apDrawData->CmdLists.erase(apDrawData->CmdLists.Data + i);
            apDrawData->CmdListsCount--;
            break;
        }
    }
}

// Recursively remove DrawLists of all child windows from the main ImDrawData.
static void s_removeChildDrawListsFromDrawData(ImDrawData* apDrawData, const ImGuiWindow* apWin) {
    for (int i = 0; i < apWin->DC.ChildWindows.Size; ++i) {
        const ImGuiWindow* child = apWin->DC.ChildWindows[i];
        if (child != nullptr && child->DrawList != nullptr) {
            s_removeDrawListFromDrawData(apDrawData, child->DrawList);
            s_removeChildDrawListsFromDrawData(apDrawData, child);
        }
    }
}

// ---------- Wobbly spring helpers ----------

// Extract the 4 corners of a rect: [0]=TL, [1]=TR, [2]=BL, [3]=BR
static void s_cornersFromRect(const ImRect& arRect, ImVec2 aoCorners[4]) {
    aoCorners[0] = arRect.Min;
    aoCorners[1] = ImVec2(arRect.Max.x, arRect.Min.y);
    aoCorners[2] = ImVec2(arRect.Min.x, arRect.Max.y);
    aoCorners[3] = arRect.Max;
}

// Initialize the 4 spring corners at the window's current position,
// and compute bilinear weights from the grab point (where the user clicked).
// Closer corners get higher weight = stiffer spring = less wobble near cursor.
static void s_initSprings(ImGenieEffect& aorEffect, const ImRect& arWinRect, const ImVec2& arGrabPos) {
    ImVec2 corners[4];
    s_cornersFromRect(arWinRect, corners);
    for (int32_t i = 0; i < 4; ++i) {
        aorEffect.springs[i].current = corners[i];
        aorEffect.springs[i].velocity = ImVec2(0, 0);
    }
    // Normalized grab position within the window [0..1]
    const auto gx = ImClamp((arWinRect.GetWidth() > 0.0f) ? (arGrabPos.x - arWinRect.Min.x) / arWinRect.GetWidth() : 0.5f, 0.0f, 1.0f);
    const auto gy = ImClamp((arWinRect.GetHeight() > 0.0f) ? (arGrabPos.y - arWinRect.Min.y) / arWinRect.GetHeight() : 0.5f, 0.0f, 1.0f);
    aorEffect.grabUV = ImVec2(gx, gy);
    // Bilinear weights: each corner's weight = product of distances from grab point
    aorEffect.springWeights[0] = (1.0f - gx) * (1.0f - gy);  // TL
    aorEffect.springWeights[1] = gx * (1.0f - gy);           // TR
    aorEffect.springWeights[2] = (1.0f - gx) * gy;           // BL
    aorEffect.springWeights[3] = gx * gy;                    // BR
}

// Advance the spring simulation by aDt seconds (Euler integration with substeps).
// Each corner is pulled toward its target position by a spring force (Hooke's law + damping).
// Stiffness varies per corner: higher weight = stiffer = follows cursor more closely.
// Generic spring update: all 4 corners spring toward targets with uniform stiffness/damping.
static void s_updateSpringsUniform(ImGenieEffect& aorEffect, const ImVec2 aTargets[4], const ImGenieSpringParams& arSpring, float aDt) {
    const auto subDt = aDt / arSpring.substeps;
    for (int32_t step = 0; step < arSpring.substeps; ++step) {
        for (int32_t i = 0; i < 4; ++i) {
            auto force = (aTargets[i] - aorEffect.springs[i].current) * arSpring.stiffness;
            force = force - aorEffect.springs[i].velocity * arSpring.damping;
            aorEffect.springs[i].velocity = aorEffect.springs[i].velocity + force * subDt;
            aorEffect.springs[i].current = aorEffect.springs[i].current + aorEffect.springs[i].velocity * subDt;
        }
    }
}

// Wobbly spring update: stiffness varies per corner based on grab distance (springWeights).
static void s_updateSprings(ImGenieEffect& aorEffect, const ImRect& arWinRect, float aDt) {
    const auto& rParams = aorEffect.params;
    ImVec2 targets[4];
    s_cornersFromRect(arWinRect, targets);
    const auto& spring = rParams.effects.wobbly.spring;
    const auto subDt = aDt / spring.substeps;
    for (int32_t step = 0; step < spring.substeps; ++step) {
        for (int32_t i = 0; i < 4; ++i) {
            const auto stiffness = spring.stiffness +  //
                (rParams.effects.wobbly.maxStiffness - spring.stiffness) * aorEffect.springWeights[i];
            auto force = (targets[i] - aorEffect.springs[i].current) * stiffness;
            force = force - aorEffect.springs[i].velocity * spring.damping;
            aorEffect.springs[i].velocity = aorEffect.springs[i].velocity + force * subDt;
            aorEffect.springs[i].current = aorEffect.springs[i].current + aorEffect.springs[i].velocity * subDt;
        }
    }
}

// ---------- Context management ----------

ImGenieContext* ImGenie::CreateContext() {
    auto* ctx = IM_NEW(ImGenieContext);
    if (s_ctx == nullptr) { SetCurrentContext(ctx); }
    return ctx;
}

void ImGenie::DestroyContext(ImGenieContext* apCtx) {
    if (apCtx == nullptr) { apCtx = s_ctx; }
    if (apCtx == nullptr) { return; }
    IM_ASSERT(s_ctx && "The context is null");
    for (auto it = apCtx->effects.begin(); it != apCtx->effects.end(); ++it) { s_deleteEffectTexture(it->second); }
    if (s_ctx == apCtx) { s_ctx = nullptr; }
    IM_DELETE(apCtx);
}

ImGenieContext* ImGenie::GetCurrentContext() { return s_ctx; }

bool ImGenie::HasActiveEffects() {
    IM_ASSERT(s_ctx && "The context is null");
    if (s_ctx == nullptr) { return false; }
    return !s_ctx->effects.empty();
}

bool ImGenie::IsEffectActive(const char* aWindowName) {
    IM_ASSERT(s_ctx && "The context is null");
    if (s_ctx == nullptr) { return false; }
    const auto id = ImHashStr(aWindowName);
    auto it = s_ctx->effects.find(id);
    return it != s_ctx->effects.end();
}

void ImGenie::SetCurrentContext(ImGenieContext* apCtx) { s_ctx = apCtx; }

void ImGenie::SetCreateCaptureFunc(const CreateCaptureFunctor& arFunc) {
    IM_ASSERT(s_ctx && "The context is null");
    if (s_ctx == nullptr) { return; }
    s_ctx->createCaptureFunc = arFunc;
}

void ImGenie::SetDestroyCaptureFunc(const DestroyCaptureFunctor& arFunc) {
    IM_ASSERT(s_ctx && "The context is null");
    if (s_ctx == nullptr) { return; }
    s_ctx->destroyCaptureFunc = arFunc;
}

void ImGenie::SetCaptureFlipV(bool aFlipV) {
    IM_ASSERT(s_ctx && "The context is null");
    if (s_ctx == nullptr) { return; }
    s_ctx->captureFlipV = aFlipV;
}

// ---------- ImGenie API ----------

// Main API function, called every frame for each tracked window.
//
// State machine overview:
//   1. Disappearance: user closes window -> PendingCapture -> Captured -> Animating -> done
//   2. Appearance: user opens window -> AppearingCapture -> AppearingAnimating -> done
//   3. Wobbly move: user drags window -> MovingCapture -> MovingActive -> MovingSettle -> done
//
// Returns true if the caller can show the window normally.
// Returns false if ImGenie is handling the window (animating).
bool ImGenie::Allow(const char* aWindowName, bool* apoOpen, const ImGenieParams* apParams) {
    IM_ASSERT(s_ctx && "The context is null");
    if (aWindowName == nullptr || apoOpen == nullptr) { return true; }
    auto& ctx = *s_ctx;
    const auto& params = apParams ? *apParams : ctx.globalParams;
    const auto id = ImHashStr(aWindowName);
    auto it = ctx.effects.find(id);

    // ==================== Active effect handling ====================
    if (it != ctx.effects.end()) {
        auto& effect = it->second;
        // Update params each frame, and track button position (destRect follows genie params)
        effect.params = params;
        const auto& dr = params.transitions.genie.destRect;
        effect.destRect = ImRect(dr.minX, dr.minY, dr.maxX, dr.maxY);

        // --- Disappearance: capture frame ---
        // Keep window open one more frame so Capture() can snapshot its DrawList
        if (effect.state == ImGenieEffect::State::PendingCapture) {
            *apoOpen = true;
            return true;
        }

        // --- Disappearance: captured, start animation ---
        if (effect.state == ImGenieEffect::State::Captured) {
            effect.state = ImGenieEffect::State::Animating;
            effect.animT = 0.0f;
            // Init springs for slide wobbly
            if (effect.params.transitions.transitionMode == ImGenieTransitionMode_Slide && effect.params.transitions.slide.wobbly) {
                ImVec2 corners[4];
                s_cornersFromRect(effect.sourceRect, corners);
                for (int32_t i = 0; i < 4; ++i) {
                    effect.springs[i].current = corners[i];
                    effect.springs[i].velocity = ImVec2(0.0f, 0.0f);
                }
            }
        }

        // --- Disappearance: animating ---
        if (effect.state == ImGenieEffect::State::Animating) {
            const auto dt = ImGui::GetIO().DeltaTime;
            effect.animT += dt / effect.params.transitions.animDuration;
            if (effect.animT >= 1.0f) {
                s_deleteEffectTexture(effect);
                ctx.effectNames.erase(id);
                ctx.effects.erase(it);
                *apoOpen = false;
                return true;
            }
            auto* pDrawList = ImGui::GetForegroundDrawList();
            const auto mode = effect.params.transitions.transitionMode;
            if (mode == ImGenieTransitionMode_PageCurl) {
                s_pageCurlAnimate(pDrawList, effect.capturedTex, effect.capturedSize,
                                  1.0f - effect.animT, effect.sourceRect,
                                  effect.params.transitions.pageCurl.cellsH,
                                  effect.params.transitions.pageCurl.cellsV,
                                  effect.params.transitions.pageCurl.origin,
                                  ctx.captureFlipV);
            } else if (mode == ImGenieTransitionMode_Fade) {
                s_fadeAnimate(pDrawList, effect.capturedTex, effect.capturedSize,
                              1.0f - effect.animT, effect.sourceRect, ctx.captureFlipV);
            } else if (mode == ImGenieTransitionMode_Scale) {
                s_scaleAnimate(pDrawList, effect.capturedTex, effect.capturedSize,
                               1.0f - effect.animT, effect.sourceRect, ctx.captureFlipV);
            } else if (mode == ImGenieTransitionMode_Slide) {
                const auto& slideP = effect.params.transitions.slide;
                if (slideP.wobbly) {
                    // Pin leading corners to their animated off-screen position, spring-simulate trailing corners
                    const auto dir = s_resolveSlideDir(effect.sourceRect, slideP.dir);
                    const auto& dispSize = ImGui::GetIO().DisplaySize;
                    float fullDistX = 0.0f, fullDistY = 0.0f;
                    if (dir == ImGenieSlideDir_Left || dir == ImGenieSlideDir_TopLeft || dir == ImGenieSlideDir_BottomLeft)
                        fullDistX = -effect.sourceRect.Max.x;
                    else if (dir == ImGenieSlideDir_Right || dir == ImGenieSlideDir_TopRight || dir == ImGenieSlideDir_BottomRight)
                        fullDistX = dispSize.x - effect.sourceRect.Min.x;
                    if (dir == ImGenieSlideDir_Up || dir == ImGenieSlideDir_TopLeft || dir == ImGenieSlideDir_TopRight)
                        fullDistY = -effect.sourceRect.Max.y;
                    else if (dir == ImGenieSlideDir_Down || dir == ImGenieSlideDir_BottomLeft || dir == ImGenieSlideDir_BottomRight)
                        fullDistY = dispSize.y - effect.sourceRect.Min.y;
                    const auto t = ImClamp(effect.animT, 0.0f, 1.0f);
                    const auto eased = t * t * (3.0f - 2.0f * t);
                    const ImVec2 off(fullDistX * eased, fullDistY * eased);
                    // Determine pinned (leading) corners: TL=0, TR=1, BL=2, BR=3
                    bool pinned[4] = {false, false, false, false};
                    switch (dir) {
                        case ImGenieSlideDir_Left:        pinned[0] = pinned[2] = true; break;
                        case ImGenieSlideDir_Right:       pinned[1] = pinned[3] = true; break;
                        case ImGenieSlideDir_Up:          pinned[0] = pinned[1] = true; break;
                        case ImGenieSlideDir_Down:        pinned[2] = pinned[3] = true; break;
                        case ImGenieSlideDir_TopLeft:     pinned[0] = true; break;
                        case ImGenieSlideDir_TopRight:    pinned[1] = true; break;
                        case ImGenieSlideDir_BottomLeft:  pinned[2] = true; break;
                        case ImGenieSlideDir_BottomRight: pinned[3] = true; break;
                        default: break;
                    }
                    // Pin leading corners
                    ImVec2 srcCorners[4];
                    s_cornersFromRect(effect.sourceRect, srcCorners);
                    for (int32_t i = 0; i < 4; ++i) {
                        if (pinned[i]) {
                            effect.springs[i].current = srcCorners[i] + off;
                            effect.springs[i].velocity = ImVec2(0.0f, 0.0f);
                        }
                    }
                    // Spring-simulate trailing corners toward their off-screen targets
                    ImVec2 targets[4];
                    for (int32_t i = 0; i < 4; ++i) targets[i] = srcCorners[i] + off;
                    s_updateSpringsUniform(effect, targets, slideP.spring, dt);
                    // Re-pin leading (spring update moved them)
                    for (int32_t i = 0; i < 4; ++i) {
                        if (pinned[i]) {
                            effect.springs[i].current = srcCorners[i] + off;
                        }
                    }
                    s_latticeDraw(pDrawList, effect.capturedTex,
                                  effect.springs[0].current, effect.springs[1].current,
                                  effect.springs[2].current, effect.springs[3].current,
                                  slideP.spring.cellsH, slideP.spring.cellsV,
                                  ImGenieAnimMode_Compress, ctx.captureFlipV);
                } else {
                    s_slideAnimate(pDrawList, effect.capturedTex, effect.capturedSize,
                                   1.0f - effect.animT, effect.sourceRect,
                                   effect.params.transitions.slide, ctx.captureFlipV);
                }
            } else {
                // Genie
                const auto horizontal = (effect.resolvedSide == ImGenieSide_Left || effect.resolvedSide == ImGenieSide_Right);
                const auto cellsH = horizontal ? effect.params.transitions.genie.cellsV : effect.params.transitions.genie.cellsH;
                const auto cellsV = horizontal ? effect.params.transitions.genie.cellsH : effect.params.transitions.genie.cellsV;
                s_latticeAnimate(pDrawList, effect.capturedTex, effect.capturedSize,
                                 effect.animT, effect.sourceRect, effect.destRect,
                                 cellsH, cellsV,
                                 effect.params.transitions.genie.animMode,
                                 effect.resolvedSide, ctx.captureFlipV);
            }
            s_drawDebugMesh(pDrawList, effect);
            *apoOpen = false;
            return false;
        }

        // --- Appearance: capture frame ---
        // Keep window open so Capture() can snapshot its first rendered frame
        if (effect.state == ImGenieEffect::State::AppearingCapture) {
            *apoOpen = true;
            return true;
        }

        // --- Appearance: animating ---
        if (effect.state == ImGenieEffect::State::AppearingAnimating) {
            const auto dt = ImGui::GetIO().DeltaTime;
            effect.animT += dt / effect.params.transitions.animDuration;
            if (effect.animT >= 1.0f) {
                s_deleteEffectTexture(effect);
                ctx.effectNames.erase(id);
                ctx.effects.erase(it);
                *apoOpen = true;
                ctx.openStates[id] = true;
                return true;
            }
            auto* pDrawList = ImGui::GetForegroundDrawList();
            const auto mode = effect.params.transitions.transitionMode;
            if (mode == ImGenieTransitionMode_PageCurl) {
                s_pageCurlAnimate(pDrawList, effect.capturedTex, effect.capturedSize,
                                  effect.animT, effect.sourceRect,
                                  effect.params.transitions.pageCurl.cellsH,
                                  effect.params.transitions.pageCurl.cellsV,
                                  effect.params.transitions.pageCurl.origin,
                                  ctx.captureFlipV);
            } else if (mode == ImGenieTransitionMode_Fade) {
                s_fadeAnimate(pDrawList, effect.capturedTex, effect.capturedSize,
                              effect.animT, effect.sourceRect, ctx.captureFlipV);
            } else if (mode == ImGenieTransitionMode_Scale) {
                s_scaleAnimate(pDrawList, effect.capturedTex, effect.capturedSize,
                               effect.animT, effect.sourceRect, ctx.captureFlipV);
            } else if (mode == ImGenieTransitionMode_Slide) {
                const auto& slideP = effect.params.transitions.slide;
                if (slideP.wobbly) {
                    // Appearing: springs started off-screen, converge to source rect
                    // Leading corners are pinned to the lerped position, trailing corners spring-follow
                    const auto dir = s_resolveSlideDir(effect.sourceRect, slideP.dir);
                    const auto& dispSize = ImGui::GetIO().DisplaySize;
                    float fullDistX = 0.0f, fullDistY = 0.0f;
                    if (dir == ImGenieSlideDir_Left || dir == ImGenieSlideDir_TopLeft || dir == ImGenieSlideDir_BottomLeft)
                        fullDistX = -effect.sourceRect.Max.x;
                    else if (dir == ImGenieSlideDir_Right || dir == ImGenieSlideDir_TopRight || dir == ImGenieSlideDir_BottomRight)
                        fullDistX = dispSize.x - effect.sourceRect.Min.x;
                    if (dir == ImGenieSlideDir_Up || dir == ImGenieSlideDir_TopLeft || dir == ImGenieSlideDir_TopRight)
                        fullDistY = -effect.sourceRect.Max.y;
                    else if (dir == ImGenieSlideDir_Down || dir == ImGenieSlideDir_BottomLeft || dir == ImGenieSlideDir_BottomRight)
                        fullDistY = dispSize.y - effect.sourceRect.Min.y;
                    const auto t = ImClamp(effect.animT, 0.0f, 1.0f);
                    const auto eased = t * t * (3.0f - 2.0f * t);
                    const ImVec2 off(fullDistX * (1.0f - eased), fullDistY * (1.0f - eased));  // goes from fullDist to 0
                    bool pinned[4] = {false, false, false, false};
                    switch (dir) {
                        case ImGenieSlideDir_Left:        pinned[0] = pinned[2] = true; break;
                        case ImGenieSlideDir_Right:       pinned[1] = pinned[3] = true; break;
                        case ImGenieSlideDir_Up:          pinned[0] = pinned[1] = true; break;
                        case ImGenieSlideDir_Down:        pinned[2] = pinned[3] = true; break;
                        case ImGenieSlideDir_TopLeft:     pinned[0] = true; break;
                        case ImGenieSlideDir_TopRight:    pinned[1] = true; break;
                        case ImGenieSlideDir_BottomLeft:  pinned[2] = true; break;
                        case ImGenieSlideDir_BottomRight: pinned[3] = true; break;
                        default: break;
                    }
                    ImVec2 srcCorners[4];
                    s_cornersFromRect(effect.sourceRect, srcCorners);
                    for (int32_t i = 0; i < 4; ++i) {
                        if (pinned[i]) {
                            effect.springs[i].current = srcCorners[i] + off;
                            effect.springs[i].velocity = ImVec2(0.0f, 0.0f);
                        }
                    }
                    ImVec2 targets[4];
                    for (int32_t i = 0; i < 4; ++i) targets[i] = srcCorners[i] + off;
                    s_updateSpringsUniform(effect, targets, slideP.spring, dt);
                    for (int32_t i = 0; i < 4; ++i) {
                        if (pinned[i]) {
                            effect.springs[i].current = srcCorners[i] + off;
                        }
                    }
                    s_latticeDraw(pDrawList, effect.capturedTex,
                                  effect.springs[0].current, effect.springs[1].current,
                                  effect.springs[2].current, effect.springs[3].current,
                                  slideP.spring.cellsH, slideP.spring.cellsV,
                                  ImGenieAnimMode_Compress, ctx.captureFlipV);
                } else {
                    s_slideAnimate(pDrawList, effect.capturedTex, effect.capturedSize,
                                   effect.animT, effect.sourceRect,
                                   effect.params.transitions.slide, ctx.captureFlipV);
                }
            } else {
                // Genie: reverse animT to expand from button to full window
                const auto horizontal = (effect.resolvedSide == ImGenieSide_Left || effect.resolvedSide == ImGenieSide_Right);
                const auto cellsH = horizontal ? effect.params.transitions.genie.cellsV : effect.params.transitions.genie.cellsH;
                const auto cellsV = horizontal ? effect.params.transitions.genie.cellsH : effect.params.transitions.genie.cellsV;
                s_latticeAnimate(pDrawList, effect.capturedTex, effect.capturedSize,
                                 1.0f - effect.animT, effect.sourceRect, effect.destRect,
                                 cellsH, cellsV,
                                 effect.params.transitions.genie.animMode,
                                 effect.resolvedSide, ctx.captureFlipV);
            }
            s_drawDebugMesh(pDrawList, effect);
            *apoOpen = false;
            return false;
        }

        // --- Moving: capture frame ---
        if (effect.state == ImGenieEffect::State::MovingCapture) { return true; }

        // --- Moving: active or settling ---
        if (effect.state == ImGenieEffect::State::MovingActive || effect.state == ImGenieEffect::State::MovingSettle) {
            if (effect.capturedTex._TexID == 0) {
                ctx.effectNames.erase(id);
                ctx.effects.erase(it);
                return true;
            }
            auto* pWin = ImGui::FindWindowByName(aWindowName);
            if (pWin == nullptr) {
                s_deleteEffectTexture(effect);
                ctx.effectNames.erase(id);
                ctx.effects.erase(it);
                return true;
            }
            const auto winRect = pWin->Rect();
            const auto dt = ImGui::GetIO().DeltaTime;

            // Detect drag end: check if ImGui still considers this window as being moved
            if (effect.state == ImGenieEffect::State::MovingActive) {
                const auto& g = *GImGui;
                auto stillDragging = false;
                if (g.MovingWindow != nullptr) {
                    stillDragging = (g.MovingWindow == pWin);
                    if (!stillDragging && g.MovingWindow->RootWindow != nullptr) { stillDragging = (g.MovingWindow->RootWindow == pWin); }
                }
                if (!stillDragging) {
                    // Transition to settle: springs will lerp back to real corners
                    effect.state = ImGenieEffect::State::MovingSettle;
                    effect.animT = 0.0f;
                    for (int32_t i = 0; i < 4; ++i) { effect.settleStart[i] = effect.springs[i].current; }
                }
            }

            if (effect.state == ImGenieEffect::State::MovingActive) {
                s_updateSprings(effect, winRect, dt);
                // Pin grab point: ensure the point under the cursor follows it exactly.
                // Without this, springs would lag behind the cursor everywhere.
                // We compute the weighted average of spring positions (= current grab point)
                // and compare with where it should be (weighted average of real corners).
                // The correction is distributed proportionally: factor[i] = w[i] / sum(w[j]^2)
                // This preserves the deformation while ensuring exact cursor tracking.
                ImVec2 targets[4];
                s_cornersFromRect(winRect, targets);
                const auto targetGrab =                     //
                    targets[0] * effect.springWeights[0]    //
                    + targets[1] * effect.springWeights[1]  //
                    + targets[2] * effect.springWeights[2]  //
                    + targets[3] * effect.springWeights[3];
                const auto currentGrab =                                   //
                    effect.springs[0].current * effect.springWeights[0]    //
                    + effect.springs[1].current * effect.springWeights[1]  //
                    + effect.springs[2].current * effect.springWeights[2]  //
                    + effect.springs[3].current * effect.springWeights[3];
                const auto correction = targetGrab - currentGrab;
                auto wSqSum = 0.0f;
                for (int32_t i = 0; i < 4; ++i) { wSqSum += effect.springWeights[i] * effect.springWeights[i]; }
                if (wSqSum > 0.0f) {
                    for (int32_t i = 0; i < 4; ++i) {
                        const auto factor = effect.springWeights[i] / wSqSum;
                        effect.springs[i].current = effect.springs[i].current + correction * factor;
                    }
                }
            } else {
                // Settle phase: EaseOutQuad lerp from deformed positions back to real corners
                effect.animT += dt / effect.params.effects.wobbly.settleDuration;
                if (effect.animT >= 1.0f) {
                    s_deleteEffectTexture(effect);
                    ctx.effectNames.erase(id);
                    ctx.effects.erase(it);
                    return true;
                }
                const auto t = effect.animT;
                const auto eased = 1.0f - (1.0f - t) * (1.0f - t);
                ImVec2 targets[4];
                s_cornersFromRect(winRect, targets);
                for (int32_t i = 0; i < 4; ++i) { effect.springs[i].current = ImLerp(effect.settleStart[i], targets[i], eased); }
            }
            auto* pDrawList = ImGui::GetForegroundDrawList();
            s_latticeDraw(pDrawList,
                          effect.capturedTex,
                          effect.springs[0].current,
                          effect.springs[1].current,
                          effect.springs[2].current,
                          effect.springs[3].current,
                          effect.params.effects.wobbly.spring.cellsH,
                          effect.params.effects.wobbly.spring.cellsV,
                          effect.params.transitions.genie.animMode,
                          ctx.captureFlipV);
            s_drawDebugMesh(pDrawList, effect);
            return true;
        }
        return true;
    }

    // ==================== No active effect — detect transitions ====================

    // Track previous open state to detect open/close transitions
    auto wasOpen = false;
    const auto stateIt = ctx.openStates.find(id);
    if (stateIt != ctx.openStates.end()) { wasOpen = stateIt->second; }
    ctx.openStates[id] = *apoOpen;

    // --- Detect disappearance: window was open, now closed ---
    if (wasOpen && !(*apoOpen)) {
        if (params.transitions.transitionMode == ImGenieTransitionMode_None) {
            return true;  // No effect enabled: let window close instantly
        }
        auto* pWin = ImGui::FindWindowByName(aWindowName);
        if (pWin != nullptr) {
            ImGenieEffect effect;
            effect.state = ImGenieEffect::State::PendingCapture;
            effect.sourceRect = pWin->Rect();
            const auto& dr2 = params.transitions.genie.destRect;
            effect.destRect = ImRect(dr2.minX, dr2.minY, dr2.maxX, dr2.maxY);
            effect.capturedSize = effect.sourceRect.GetSize();
            effect.params = params;
            // Resolve genie side once (won't change during animation even if button moves)
            effect.resolvedSide = (params.transitions.genie.side == ImGenieSide_Auto)  //
                ? s_autoDetectSide(effect.sourceRect, effect.destRect)
                : params.transitions.genie.side;
            ctx.effects[id] = effect;
            ctx.effectNames[id] = aWindowName;
            // Keep window open one more frame for capture
            *apoOpen = true;
            return true;
        }
    }

    // --- Detect appearance: window was closed, now open ---
    if (!wasOpen && *apoOpen) {
        if (params.transitions.transitionMode == ImGenieTransitionMode_None) {
            return true;  // No effect enabled: let window appear instantly
        }
        ImGenieEffect effect;
        effect.state = ImGenieEffect::State::AppearingCapture;
        const auto& dr3 = params.transitions.genie.destRect;
        effect.destRect = ImRect(dr3.minX, dr3.minY, dr3.maxX, dr3.maxY);
        effect.params = params;
        auto* pWin = ImGui::FindWindowByName(aWindowName);
        if (pWin != nullptr) { pWin->HiddenFramesCannotSkipItems = 1; }
        ctx.effects[id] = effect;
        ctx.effectNames[id] = aWindowName;
        *apoOpen = true;
        return true;
    }

    // --- Detect drag start (two-frame detection) ---
    // Frame 1: MovingWindow matches this window → record initial position in dragStartPos.
    // Frame 2: if pWin->Pos has changed since recorded position → real drag confirmed.
    // This two-frame delay avoids triggering on a simple click (no movement).
    if (*apoOpen && params.effects.effectMode == ImGenieEffectMode_Wobbly) {
        const auto& g = *GImGui;
        auto* pWin = ImGui::FindWindowByName(aWindowName);
        if (pWin != nullptr && g.MovingWindow != nullptr) {
            // Check if this window (or its root) is the one being moved by ImGui
            auto isMoving = (g.MovingWindow == pWin);
            if (!isMoving && g.MovingWindow->RootWindow != nullptr) { isMoving = (g.MovingWindow->RootWindow == pWin); }
            if (isMoving) {
                const auto startIt = ctx.dragStartPos.find(id);
                if (startIt == ctx.dragStartPos.end()) {
                    // Frame 1: first frame of drag detected, just record starting position
                    ctx.dragStartPos[id] = pWin->Pos;
                } else {
                    // Frame 2+: check if window actually moved (filters out click-without-drag)
                    const auto delta = pWin->Pos - startIt->second;
                    if (delta.x != 0.0f || delta.y != 0.0f) {
                        // Confirmed real drag → create wobbly move effect
                        ctx.dragStartPos.erase(startIt);
                        ImGenieEffect effect;
                        effect.state = ImGenieEffect::State::MovingCapture;
                        effect.sourceRect = pWin->Rect();
                        effect.capturedSize = pWin->Size;
                        effect.params = params;
                        ctx.effects[id] = effect;
                        ctx.effectNames[id] = aWindowName;
                        return true;
                    }
                }
            }
        } else {
            // Window not being dragged (anymore), clean up tracking
            ctx.dragStartPos.erase(id);
        }
    }
    return true;
}

// Wrapper: Allow + ImGui::Begin combined.
// Unlike ImGui::Begin/End, End() must only be called if Begin() returned true.
// Calling End() when Begin() returned false will trigger an ImGui assert.
bool ImGenie::Begin(const char* aWindowName, bool* apoOpen, ImGuiWindowFlags aFlags, const ImGenieParams* apParams) {
    IM_ASSERT(s_ctx && "The context is null");
    if (!Allow(aWindowName, apoOpen, apParams)) {
        return false;  // ImGenie is animating, don't draw
    }
    if (apoOpen != nullptr && !(*apoOpen)) {
        return false;  // Window is closed
    }
    return ImGui::Begin(aWindowName, apoOpen, aFlags);
}

void ImGenie::End() { ImGui::End(); }

// Called AFTER ImGui::Render() and BEFORE the backend's RenderDrawData().
// At this point, all DrawLists are finalized in ImDrawData. This function:
//   1. Captures window DrawLists into FBO textures (for effects that need a snapshot)
//   2. Removes real window DrawLists from ImDrawData (so the backend won't render them)
//
// The captured texture is then drawn by Allow() on the ForegroundDrawList as a deformed mesh.
void ImGenie::Capture() {
    IM_ASSERT(s_ctx && "The context is null");
    IM_ASSERT(s_ctx->createCaptureFunc && "The capture creation function is not defined");
    auto& ctx = *s_ctx;
    auto* pMainDrawData = ImGui::GetDrawData();
    if (pMainDrawData == nullptr) { return; }
    for (auto it = ctx.effects.begin(); it != ctx.effects.end(); ++it) {
        auto& effect = it->second;
        const auto id = it->first;
        const auto nameIt = ctx.effectNames.find(id);
        if (nameIt == ctx.effectNames.end()) { continue; }

        // --- Appearance: capture the window's first rendered frame into FBO ---
        // The window was just opened; Allow() kept it visible for one frame so we can capture it.
        // After capture, we hide it (remove from draw data) and animate from button → window.
        if (effect.state == ImGenieEffect::State::AppearingCapture) {
            auto* pWin = ImGui::FindWindowByName(nameIt->second);
            if (pWin == nullptr || pWin->DrawList == nullptr || pWin->DrawList->CmdBuffer.Size == 0) {
                // No drawable content → skip animation, show window immediately
                effect.state = ImGenieEffect::State::AppearingAnimating;
                effect.animT = 1.0f;
                continue;
            }
            effect.sourceRect = pWin->Rect();
            effect.capturedSize = pWin->Size;
            // Resolve genie side here (not in ByPass) because pWin->Rect() is only valid now
            effect.resolvedSide =
                (effect.params.transitions.genie.side == ImGenieSide_Auto) ? s_autoDetectSide(effect.sourceRect, effect.destRect) : effect.params.transitions.genie.side;
            // Render the window's DrawList into an offscreen FBO → texture
            effect.capturedTex = s_captureWindow(pWin, pMainDrawData);
            if (effect.capturedTex._TexID != 0) {
                effect.state = ImGenieEffect::State::AppearingAnimating;
                effect.animT = 0.0f;
                // Init springs for slide wobbly (appearing: start from off-screen)
                if (effect.params.transitions.transitionMode == ImGenieTransitionMode_Slide && effect.params.transitions.slide.wobbly) {
                    const auto dir = s_resolveSlideDir(effect.sourceRect, effect.params.transitions.slide.dir);
                    const auto& displaySize = ImGui::GetIO().DisplaySize;
                    float fullDistX = 0.0f, fullDistY = 0.0f;
                    if (dir == ImGenieSlideDir_Left || dir == ImGenieSlideDir_TopLeft || dir == ImGenieSlideDir_BottomLeft)
                        fullDistX = -effect.sourceRect.Max.x;
                    else if (dir == ImGenieSlideDir_Right || dir == ImGenieSlideDir_TopRight || dir == ImGenieSlideDir_BottomRight)
                        fullDistX = displaySize.x - effect.sourceRect.Min.x;
                    if (dir == ImGenieSlideDir_Up || dir == ImGenieSlideDir_TopLeft || dir == ImGenieSlideDir_TopRight)
                        fullDistY = -effect.sourceRect.Max.y;
                    else if (dir == ImGenieSlideDir_Down || dir == ImGenieSlideDir_BottomLeft || dir == ImGenieSlideDir_BottomRight)
                        fullDistY = displaySize.y - effect.sourceRect.Min.y;
                    const ImVec2 off(fullDistX, fullDistY);
                    ImVec2 corners[4];
                    s_cornersFromRect(effect.sourceRect, corners);
                    for (int32_t i = 0; i < 4; ++i) {
                        effect.springs[i].current = corners[i] + off;  // start off-screen
                        effect.springs[i].velocity = ImVec2(0.0f, 0.0f);
                    }
                }
            } else {
                // Capture failed → skip animation, show window immediately
                effect.state = ImGenieEffect::State::AppearingAnimating;
                effect.animT = 1.0f;
            }
            // Hide real window this frame (the mesh animation replaces it)
            s_removeDrawListFromDrawData(pMainDrawData, pWin->DrawList);
            s_removeChildDrawListsFromDrawData(pMainDrawData, pWin);
        }

        // --- Wobbly move: capture window into FBO on first drag frame ---
        // CRITICAL: the `continue` at the end prevents falling into the MovingActive branch
        // on the same frame. Without it, the window would flicker for one frame (visible real
        // window + captured mesh simultaneously).
        else if (effect.state == ImGenieEffect::State::MovingCapture) {
            const auto* pWin = ImGui::FindWindowByName(nameIt->second);
            if (pWin == nullptr || pWin->DrawList == nullptr || pWin->DrawList->CmdBuffer.Size == 0) {
                effect.state = ImGenieEffect::State::MovingSettle;
                continue;
            }
            effect.sourceRect = pWin->Rect();
            effect.capturedSize = pWin->Size;
            effect.capturedTex = s_captureWindow(pWin, pMainDrawData);
            if (effect.capturedTex._TexID != 0) {
                // Initialize spring positions at current window corners, weights from grab point
                s_initSprings(effect, pWin->Rect(), ImGui::GetIO().MousePos);
                effect.state = ImGenieEffect::State::MovingActive;
            } else {
                effect.state = ImGenieEffect::State::MovingSettle;
            }
            // CRITICAL: skip to next effect. Do NOT fall through to MovingActive removal below,
            // because the real window is still visible this frame (capture just happened).
            continue;
        }

        // --- Wobbly move active/settle: hide real window every frame ---
        // The captured texture is drawn by Allow() on ForegroundDrawList; we must remove
        // the real window from draw data so only the deformed mesh is visible.
        else if (effect.state == ImGenieEffect::State::MovingActive || effect.state == ImGenieEffect::State::MovingSettle) {
            auto* pWin = ImGui::FindWindowByName(nameIt->second);
            if (pWin != nullptr && pWin->DrawList != nullptr) {
                s_removeDrawListFromDrawData(pMainDrawData, pWin->DrawList);
                s_removeChildDrawListsFromDrawData(pMainDrawData, pWin);
            }
        }

        // --- Disappearance: capture the window before it closes ---
        // Allow() kept the window open for one extra frame (PendingCapture state).
        // We capture its DrawList now, then it will be animated shrinking into the button.
        else if (effect.state == ImGenieEffect::State::PendingCapture) {
            auto* pWin = ImGui::FindWindowByName(nameIt->second);
            if (pWin == nullptr || pWin->DrawList == nullptr || pWin->DrawList->CmdBuffer.Size == 0) {
                // No drawable content → skip animation
                effect.state = ImGenieEffect::State::Animating;
                effect.animT = 1.0f;
                continue;
            }
            effect.sourceRect = pWin->Rect();
            effect.capturedSize = pWin->Size;
            effect.capturedTex = s_captureWindow(pWin, pMainDrawData);
            if (effect.capturedTex._TexID != 0) {
                effect.state = ImGenieEffect::State::Captured;
            } else {
                // Capture failed → skip animation, close window immediately
                effect.state = ImGenieEffect::State::Animating;
                effect.animT = 1.0f;
            }
        }
    }
}

void ImGenie::ShowDemoWindow(bool* apoOpen, ImGenieParams* apoParams, ImGenieParams* apoDefaultParams) {
    IM_ASSERT(s_ctx && "The context is null");
    if (apoOpen != nullptr && !(*apoOpen)) { return; }
    if (!ImGui::Begin("ImGenie Demo", apoOpen)) {
        ImGui::End();
        return;
    }
    if (s_ctx == nullptr) {
        ImGui::TextUnformatted("No ImGenie context!");
        ImGui::End();
        return;
    }
    ImGui::Text("ImGenie %s", IMGENIE_VERSION);
    ImGui::Separator();

    auto& ctx = *s_ctx;
    auto* pParams = apoParams ? apoParams : &ctx.globalParams;
    auto* pDefaultParams = apoDefaultParams ? apoDefaultParams : &ctx.defaultParams;
    if (ImGui::Button("Reset Defaults")) { *pParams = *pDefaultParams; }
    ImGui::SameLine();
    if (ImGui::Button("Save to Defaults")) { *pDefaultParams = *pParams; }
    ImGui::SameLine();
    ImGui::Checkbox("Draw Debug Mesh", &pParams->drawDebugMesh);
    if (ImGui::CollapsingHeader("Transitions", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto transitionModeIdx = static_cast<int>(pParams->transitions.transitionMode);
        if (ImGui::Combo("Transition Mode", &transitionModeIdx, "None\0Genie\0Page Curl\0Fade\0Scale\0Slide\0\0")) {
            pParams->transitions.transitionMode = static_cast<ImGenieTransitionMode>(transitionModeIdx);
        }
        if (pParams->transitions.transitionMode != ImGenieTransitionMode_None) {
            ImGui::SliderFloat("Anim Duration", &pParams->transitions.animDuration, 0.05f, 3.0f, "%.2f s");
        }
        if (pParams->transitions.transitionMode == ImGenieTransitionMode_Genie) {
            auto sideIdx = static_cast<int>(pParams->transitions.genie.side);
            if (ImGui::Combo("Genie Side", &sideIdx, "Auto\0Top\0Bottom\0Left\0Right\0\0")) { pParams->transitions.genie.side = static_cast<ImGenieSide>(sideIdx); }
            ImGui::SliderInt("Genie Cells V", &pParams->transitions.genie.cellsV, 1, 100);
            ImGui::SliderInt("Genie Cells H", &pParams->transitions.genie.cellsH, 1, 100);
            auto animModeIdx = static_cast<int>(pParams->transitions.genie.animMode);
            if (ImGui::Combo("Anim mode", &animModeIdx, "Compress\0Sliding\0\0")) { pParams->transitions.genie.animMode = static_cast<ImGenieAnimMode>(animModeIdx); }
        }
        if (pParams->transitions.transitionMode == ImGenieTransitionMode_PageCurl) {
            auto originIdx = static_cast<int>(pParams->transitions.pageCurl.origin);
            if (ImGui::Combo("Origin", &originIdx, "Top Left\0Top\0Top Right\0Right\0Bottom Right\0Bottom\0Bottom Left\0Left\0\0")) {
                pParams->transitions.pageCurl.origin = static_cast<ImGeniePageCurlOrigin>(originIdx);
            }
            ImGui::SliderInt("Page Cells H", &pParams->transitions.pageCurl.cellsH, 1, 100);
            ImGui::SliderInt("Page Cells V", &pParams->transitions.pageCurl.cellsV, 1, 100);
        }
        if (pParams->transitions.transitionMode == ImGenieTransitionMode_Slide) {
            auto dirIdx = pParams->transitions.slide.dir;
            if (ImGui::Combo("Direction", &dirIdx, "Auto\0Auto Edge\0Auto Corner\0Left\0Right\0Up\0Down\0Top-Left\0Top-Right\0Bottom-Left\0Bottom-Right\0\0")) {
                pParams->transitions.slide.dir = dirIdx;
            }
            ImGui::Checkbox("Wobbly", &pParams->transitions.slide.wobbly);
            if (pParams->transitions.slide.wobbly) {
                ImGui::SliderFloat("Stiffness", &pParams->transitions.slide.spring.stiffness, 10.0f, 500.0f, "%.0f");
                ImGui::SliderFloat("Damping", &pParams->transitions.slide.spring.damping, 0.1f, 50.0f, "%.1f");
                ImGui::SliderInt("Substeps", &pParams->transitions.slide.spring.substeps, 1, 32);
                ImGui::SliderInt("Slide Cells H", &pParams->transitions.slide.spring.cellsH, 1, 50);
                ImGui::SliderInt("Slide Cells V", &pParams->transitions.slide.spring.cellsV, 1, 50);
            }
        }
    }
    if (ImGui::CollapsingHeader("Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto effectModeIdx = static_cast<int>(pParams->effects.effectMode);
        if (ImGui::Combo("Effect Mode", &effectModeIdx, "None\0Wobbly\0\0")) { pParams->effects.effectMode = static_cast<ImGenieEffectMode>(effectModeIdx); }
        if (pParams->effects.effectMode == ImGenieEffectMode_Wobbly) {
            ImGui::SliderInt("Wobbly Cells V", &pParams->effects.wobbly.spring.cellsV, 1, 100);
            ImGui::SliderInt("Wobbly Cells H", &pParams->effects.wobbly.spring.cellsH, 1, 100);
            ImGui::SliderFloat("Wobbly Max Stiffness", &pParams->effects.wobbly.maxStiffness, 10.0f, 5000.0f, "%.0f");
            ImGui::SliderFloat("Wobbly Min Stiffness", &pParams->effects.wobbly.spring.stiffness, 1.0f, 1000.0f, "%.0f");
            ImGui::SliderFloat("Wobbly Damping", &pParams->effects.wobbly.spring.damping, 0.1f, 100.0f, "%.1f");
            ImGui::SliderInt("Wobbly Substeps", &pParams->effects.wobbly.spring.substeps, 1, 32);
            ImGui::SliderFloat("Wobbly Settle Duration", &pParams->effects.wobbly.settleDuration, 0.01f, 1.0f, "%.2f s");
        }
    }
    if (ImGui::CollapsingHeader("Active effects", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Active effects: %d", static_cast<int>(ctx.effects.size()));
        if (!ctx.effects.empty() && ImGui::CollapsingHeader("Active Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto it = ctx.effects.begin(); it != ctx.effects.end(); ++it) {
                auto& effect = it->second;
                const auto nameIt = ctx.effectNames.find(it->first);
                const auto* name = (nameIt != ctx.effectNames.end()) ? nameIt->second : "???";
                const char* pStateStr = "Unknown";
                switch (effect.state) {
                    case ImGenieEffect::State::PendingCapture: {
                        pStateStr = "PendingCapture";
                    } break;
                    case ImGenieEffect::State::Captured: {
                        pStateStr = "Captured";
                    } break;
                    case ImGenieEffect::State::Animating: {
                        pStateStr = "Animating";
                    } break;
                    case ImGenieEffect::State::AppearingCapture: {
                        pStateStr = "AppearingCapture";
                    } break;
                    case ImGenieEffect::State::AppearingAnimating: {
                        pStateStr = "AppearingAnimating";
                    } break;
                    case ImGenieEffect::State::MovingCapture: {
                        pStateStr = "MovingCapture";
                    } break;
                    case ImGenieEffect::State::MovingActive: {
                        pStateStr = "MovingActive";
                    } break;
                    case ImGenieEffect::State::MovingSettle: {
                        pStateStr = "MovingSettle";
                    } break;
                }
                ImGui::SetNextItemOpen(true);
                if (ImGui::TreeNode(name)) {
                    ImGui::Text("State: %s", pStateStr);
                    ImGui::Text("AnimT: %.3f", effect.animT);
                    const auto isGenie = (effect.state == ImGenieEffect::State::Animating || effect.state == ImGenieEffect::State::AppearingAnimating);
                    if (isGenie) {
                        ImGui::Text("Cells: %d x %d", effect.params.transitions.genie.cellsH, effect.params.transitions.genie.cellsV);
                    } else {
                        ImGui::Text("Cells: %d x %d", effect.params.effects.wobbly.spring.cellsH, effect.params.effects.wobbly.spring.cellsV);
                    }
                    if (effect.state == ImGenieEffect::State::MovingActive || effect.state == ImGenieEffect::State::MovingSettle) {
                        for (int32_t i = 0; i < 4; ++i) {
                            ImGui::Text("Spring[%d]: pos(%.1f, %.1f) vel(%.1f, %.1f) w=%.2f",
                                        i,
                                        effect.springs[i].current.x,
                                        effect.springs[i].current.y,
                                        effect.springs[i].velocity.x,
                                        effect.springs[i].velocity.y,
                                        effect.springWeights[i]);
                        }
                    }
                    ImGui::TreePop();
                }
            }
        }
    }
    ImGui::End();
}

bool ImGenie::DebugCheckVersion(const char* aVersion, size_t aParamsSize, size_t aEffectSize, size_t aContextSize) {
    bool ok = true;
    if (strcmp(aVersion, IMGENIE_VERSION) != 0) {
        ok = false;
        IM_ASSERT(false && "ImGenie version mismatch");
    }
    if (aParamsSize != sizeof(ImGenieParams)) {
        ok = false;
        IM_ASSERT(false && "ImGenieParams size mismatch");
    }
    if (aEffectSize != sizeof(ImGenieEffect)) {
        ok = false;
        IM_ASSERT(false && "ImGenieEffect size mismatch");
    }
    if (aContextSize != sizeof(ImGenieContext)) {
        ok = false;
        IM_ASSERT(false && "ImGenieContext size mismatch");
    }
    return ok;
}

/////////////////////////////
// C API
/////////////////////////////

IMGENIE_C_API const char* ImGenie_GetVersion(void) { return IMGENIE_VERSION; }
IMGENIE_C_API int ImGenie_GetVersionNum(void) { return IMGENIE_VERSION_NUM; }
IMGENIE_C_API bool ImGenie_DebugCheckVersion(const char* aVersion, size_t aParamsSize, size_t aEffectSize, size_t aContextSize) {
    return ImGenie::DebugCheckVersion(aVersion, aParamsSize, aEffectSize, aContextSize);
}
IMGENIE_C_API ImGenieContext* ImGenie_CreateContext(void) { return ImGenie::CreateContext(); }
IMGENIE_C_API void ImGenie_DestroyContext(ImGenieContext* apCtx) { ImGenie::DestroyContext(apCtx); }
IMGENIE_C_API ImGenieContext* ImGenie_GetCurrentContext(void) { return ImGenie::GetCurrentContext(); }
IMGENIE_C_API void ImGenie_SetCurrentContext(ImGenieContext* apCtx) { ImGenie::SetCurrentContext(apCtx); }
IMGENIE_C_API void ImGenie_SetCreateCaptureFunc(ImGenie_CreateCaptureFunc aFunc) {
    if (aFunc != nullptr) {
        ImGenie::SetCreateCaptureFunc([aFunc](int32_t aWidth, int32_t aHeight, ImDrawData* apDrawData) -> ImTextureRef {
            void* tex = aFunc(aWidth, aHeight, static_cast<void*>(apDrawData));
            return ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(tex)));
        });
    }
}
IMGENIE_C_API void ImGenie_SetDestroyCaptureFunc(ImGenie_DestroyCaptureFunc aFunc) {
    if (aFunc != nullptr) {
        ImGenie::SetDestroyCaptureFunc([aFunc](const ImTextureRef& arTex) { aFunc(reinterpret_cast<void*>(static_cast<uintptr_t>(arTex._TexID))); });
    }
}
IMGENIE_C_API void ImGenie_SetCaptureFlipV(bool aFlipV) { ImGenie::SetCaptureFlipV(aFlipV); }
IMGENIE_C_API void ImGenie_Capture(void) { ImGenie::Capture(); }
IMGENIE_C_API bool ImGenie_HasActiveEffects(void) { return ImGenie::HasActiveEffects(); }
IMGENIE_C_API bool ImGenie_IsEffectActive(const char* aWindowName) { return ImGenie::IsEffectActive(aWindowName); }
static ImRect s_toRect(const ImGenie_Rect* apRect) { return ImRect(apRect->minX, apRect->minY, apRect->maxX, apRect->maxY); }
IMGENIE_C_API ImGenieParams* ImGenie_DefaultParams(void) {
    static ImGenieParams s_defaults{};
    return &s_defaults;
}
IMGENIE_C_API void ImGenie_ShowDemoWindow(bool* apoOpen, ImGenieParams* apoParams, ImGenieParams* apoDefaultParams) {
    ImGenie::ShowDemoWindow(apoOpen, apoParams, apoDefaultParams);
}
IMGENIE_C_API bool ImGenie_Allow(const char* aWindowName, bool* apoOpen, const ImGenieParams* apParams) { return ImGenie::Allow(aWindowName, apoOpen, apParams); }
IMGENIE_C_API bool ImGenie_Begin(const char* aWindowName, bool* apoOpen, int aFlags, const ImGenieParams* apParams) {
    return ImGenie::Begin(aWindowName, apoOpen, static_cast<ImGuiWindowFlags>(aFlags), apParams);
}
IMGENIE_C_API void ImGenie_End(void) { ImGenie::End(); }
