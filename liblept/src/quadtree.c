/*====================================================================*
 -  Copyright (C) 2001 Leptonica.  All rights reserved.
 -
 -  Redistribution and use in source and binary forms, with or without
 -  modification, are permitted provided that the following conditions
 -  are met:
 -  1. Redistributions of source code must retain the above copyright
 -     notice, this list of conditions and the following disclaimer.
 -  2. Redistributions in binary form must reproduce the above
 -     copyright notice, this list of conditions and the following
 -     disclaimer in the documentation and/or other materials
 -     provided with the distribution.
 -
 -  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 -  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 -  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 -  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ANY
 -  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 -  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 -  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 -  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 -  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 -  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 -  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *====================================================================*/

/*!
 * \file quadtree.c
 * <pre>
 *
 *      Top level quadtree linear statistics
 *          l_int32   pixQuadtreeMean()
 *          l_int32   pixQuadtreeVariance()
 *
 *      Statistics in an arbitrary rectangle
 *          l_int32   pixMeanInRectangle()
 *          l_int32   pixVarianceInRectangle()
 *
 *      Quadtree regions
 *          BOXAA    *boxaaQuadtreeRegions()
 *
 *      Quadtree access
 *          l_int32   quadtreeGetParent()
 *          l_int32   quadtreeGetChildren()
 *          l_int32   quadtreeMaxLevels()
 *
 *      Display quadtree
 *          PIX      *fpixaDisplayQuadtree()
 *
 *
 *  There are many other statistical quantities that can be computed
 *  in a quadtree, such as rank values, and these can be added as
 *  the need arises.
 *
 *  Similar results that can approximate a single level of the quadtree
 *  can be generated by pixGetAverageTiled().  There we specify the
 *  tile size over which the mean, mean square, and root variance
 *  are generated; the results are saved in a (reduced size) pix.
 *  Because the tile dimensions are integers, it is usually not possible
 *  to obtain tilings that are a power of 2, as required for quadtrees.
 * </pre>
 */

#include <math.h>
#include "allheaders.h"

#ifndef  NO_CONSOLE_IO
#define  DEBUG_BOXES       0
#endif  /* !NO_CONSOLE_IO */


/*----------------------------------------------------------------------*
 *                  Top-level quadtree linear statistics                *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixQuadtreeMean()
 *
 * \param[in]    pixs     8 bpp, no colormap
 * \param[in]    nlevels  in quadtree; max allowed depends on image size
 * \param[in]   *pix_ma   input mean accumulator; can be null
 * \param[out]  *pfpixa   mean values in quadtree
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The returned fpixa has %nlevels of fpix, each containing
 *          the mean values at its level.  Level 0 has a
 *          single value; level 1 has 4 values; level 2 has 16; etc.
 * </pre>
 */
l_int32
pixQuadtreeMean(PIX     *pixs,
                l_int32  nlevels,
                PIX     *pix_ma,
                FPIXA  **pfpixa)
{
l_int32    i, j, w, h, size, n;
l_float32  val;
BOX       *box;
BOXA      *boxa;
BOXAA     *baa;
FPIX      *fpix;
PIX       *pix_mac;

    PROCNAME("pixQuadtreeMean");

    if (!pfpixa)
        return ERROR_INT("&fpixa not defined", procName, 1);
    *pfpixa = NULL;
    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs not defined or not 8 bpp", procName, 1);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (nlevels > quadtreeMaxLevels(w, h))
        return ERROR_INT("nlevels too large for image", procName, 1);

    if (!pix_ma)
        pix_mac = pixBlockconvAccum(pixs);
    else
        pix_mac = pixClone(pix_ma);
    if (!pix_mac)
        return ERROR_INT("pix_mac not made", procName, 1);

    if ((baa = boxaaQuadtreeRegions(w, h, nlevels)) == NULL) {
        pixDestroy(&pix_mac);
        return ERROR_INT("baa not made", procName, 1);
    }

    *pfpixa = fpixaCreate(nlevels);
    for (i = 0; i < nlevels; i++) {
        boxa = boxaaGetBoxa(baa, i, L_CLONE);
        size = 1 << i;
        n = boxaGetCount(boxa);  /* n == size * size */
        fpix = fpixCreate(size, size);
        for (j = 0; j < n; j++) {
            box = boxaGetBox(boxa, j, L_CLONE);
            pixMeanInRectangle(pixs, box, pix_mac, &val);
            fpixSetPixel(fpix, j % size, j / size, val);
            boxDestroy(&box);
        }
        fpixaAddFPix(*pfpixa, fpix, L_INSERT);
        boxaDestroy(&boxa);
    }

    pixDestroy(&pix_mac);
    boxaaDestroy(&baa);
    return 0;
}


/*!
 * \brief   pixQuadtreeVariance()
 *
 * \param[in]    pixs        8 bpp, no colormap
 * \param[in]    nlevels     in quadtree
 * \param[in]   *pix_ma      input mean accumulator; can be null
 * \param[in]   *dpix_msa    input mean square accumulator; can be null
 * \param[out]  *pfpixa_v    [optional] variance values in quadtree
 * \param[out]  *pfpixa_rv   [optional] root variance values in quadtree
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The returned fpixav and fpixarv have %nlevels of fpix,
 *          each containing at the respective levels the variance
 *          and root variance values.
 * </pre>
 */
l_int32
pixQuadtreeVariance(PIX     *pixs,
                    l_int32  nlevels,
                    PIX     *pix_ma,
                    DPIX    *dpix_msa,
                    FPIXA  **pfpixa_v,
                    FPIXA  **pfpixa_rv)
{
l_int32    i, j, w, h, size, n;
l_float32  var, rvar;
BOX       *box;
BOXA      *boxa;
BOXAA     *baa;
FPIX      *fpixv, *fpixrv;
PIX       *pix_mac;  /* copy of mean accumulator */
DPIX      *dpix_msac;  /* msa clone */

    PROCNAME("pixQuadtreeVariance");

    if (!pfpixa_v && !pfpixa_rv)
        return ERROR_INT("neither &fpixav nor &fpixarv defined", procName, 1);
    if (pfpixa_v) *pfpixa_v = NULL;
    if (pfpixa_rv) *pfpixa_rv = NULL;
    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs not defined or not 8 bpp", procName, 1);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (nlevels > quadtreeMaxLevels(w, h))
        return ERROR_INT("nlevels too large for image", procName, 1);

    if (!pix_ma)
        pix_mac = pixBlockconvAccum(pixs);
    else
        pix_mac = pixClone(pix_ma);
    if (!pix_mac)
        return ERROR_INT("pix_mac not made", procName, 1);
    if (!dpix_msa)
        dpix_msac = pixMeanSquareAccum(pixs);
    else
        dpix_msac = dpixClone(dpix_msa);
    if (!dpix_msac) {
        pixDestroy(&pix_mac);
        return ERROR_INT("dpix_msac not made", procName, 1);
    }

    if ((baa = boxaaQuadtreeRegions(w, h, nlevels)) == NULL) {
        pixDestroy(&pix_mac);
        dpixDestroy(&dpix_msac);
        return ERROR_INT("baa not made", procName, 1);
    }

    if (pfpixa_v) *pfpixa_v = fpixaCreate(nlevels);
    if (pfpixa_rv) *pfpixa_rv = fpixaCreate(nlevels);
    for (i = 0; i < nlevels; i++) {
        boxa = boxaaGetBoxa(baa, i, L_CLONE);
        size = 1 << i;
        n = boxaGetCount(boxa);  /* n == size * size */
        if (pfpixa_v) fpixv = fpixCreate(size, size);
        if (pfpixa_rv) fpixrv = fpixCreate(size, size);
        for (j = 0; j < n; j++) {
            box = boxaGetBox(boxa, j, L_CLONE);
            pixVarianceInRectangle(pixs, box, pix_mac, dpix_msac, &var, &rvar);
            if (pfpixa_v) fpixSetPixel(fpixv, j % size, j / size, var);
            if (pfpixa_rv) fpixSetPixel(fpixrv, j % size, j / size, rvar);
            boxDestroy(&box);
        }
        if (pfpixa_v) fpixaAddFPix(*pfpixa_v, fpixv, L_INSERT);
        if (pfpixa_rv) fpixaAddFPix(*pfpixa_rv, fpixrv, L_INSERT);
        boxaDestroy(&boxa);
    }

    pixDestroy(&pix_mac);
    dpixDestroy(&dpix_msac);
    boxaaDestroy(&baa);
    return 0;
}


/*----------------------------------------------------------------------*
 *                  Statistics in an arbitrary rectangle                *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixMeanInRectangle()
 *
 * \param[in]    pixs     8 bpp
 * \param[in]    box      region to compute mean value
 * \param[in]    pixma    mean accumulator
 * \param[out]   pval     mean value
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function is intended to be used for many rectangles
 *          on the same image.  It can find the mean within a
 *          rectangle in O(1), independent of the size of the rectangle.
 * </pre>
 */
l_int32
pixMeanInRectangle(PIX        *pixs,
                   BOX        *box,
                   PIX        *pixma,
                   l_float32  *pval)
{
l_int32    w, h, bx, by, bw, bh;
l_uint32   val00, val01, val10, val11;
l_float32  norm;
BOX       *boxc;

    PROCNAME("pixMeanInRectangle");

    if (!pval)
        return ERROR_INT("&val not defined", procName, 1);
    *pval = 0.0;
    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs not defined", procName, 1);
    if (!box)
        return ERROR_INT("box not defined", procName, 1);
    if (!pixma)
        return ERROR_INT("pixma not defined", procName, 1);

        /* Clip rectangle to image */
    pixGetDimensions(pixs, &w, &h, NULL);
    boxc = boxClipToRectangle(box, w, h);
    boxGetGeometry(boxc, &bx, &by, &bw, &bh);
    boxDestroy(&boxc);

    if (bw == 0 || bh == 0)
        return ERROR_INT("no pixels in box", procName, 1);

        /* Use up to 4 points in the accumulator */
    norm = 1.0 / (bw * bh);
    if (bx > 0 && by > 0) {
        pixGetPixel(pixma, bx + bw - 1, by + bh - 1, &val11);
        pixGetPixel(pixma, bx + bw - 1, by - 1, &val10);
        pixGetPixel(pixma, bx - 1, by + bh - 1, &val01);
        pixGetPixel(pixma, bx - 1, by - 1, &val00);
        *pval = norm * (val11 - val01 + val00 - val10);
    } else if (by > 0) {  /* bx == 0 */
        pixGetPixel(pixma, bw - 1, by + bh - 1, &val11);
        pixGetPixel(pixma, bw - 1, by - 1, &val10);
        *pval = norm * (val11 - val10);
    } else if (bx > 0) {  /* by == 0 */
        pixGetPixel(pixma, bx + bw - 1, bh - 1, &val11);
        pixGetPixel(pixma, bx - 1, bh - 1, &val01);
        *pval = norm * (val11 - val01);
    } else {  /* bx == 0 && by == 0 */
        pixGetPixel(pixma, bw - 1, bh - 1, &val11);
        *pval = norm * val11;
    }

    return 0;
}


/*!
 * \brief   pixVarianceInRectangle()
 *
 * \param[in]    pixs        8 bpp
 * \param[in]    box         region to compute variance and/or root variance
 * \param[in]    pix_ma      mean accumulator
 * \param[in]    dpix_msa    mean square accumulator
 * \param[out]   pvar        [optional] variance
 * \param[out]   prvar       [optional] root variance
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function is intended to be used for many rectangles
 *          on the same image.  It can find the variance and/or the
 *          square root of the variance within a rectangle in O(1),
 *          independent of the size of the rectangle.
 * </pre>
 */
l_int32
pixVarianceInRectangle(PIX        *pixs,
                       BOX        *box,
                       PIX        *pix_ma,
                       DPIX       *dpix_msa,
                       l_float32  *pvar,
                       l_float32  *prvar)
{
l_int32    w, h, bx, by, bw, bh;
l_uint32   val00, val01, val10, val11;
l_float64  dval00, dval01, dval10, dval11, mval, msval, var, norm;
BOX       *boxc;

    PROCNAME("pixVarianceInRectangle");

    if (!pvar && !prvar)
        return ERROR_INT("neither &var nor &rvar defined", procName, 1);
    if (pvar) *pvar = 0.0;
    if (prvar) *prvar = 0.0;
    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs not defined", procName, 1);
    if (!box)
        return ERROR_INT("box not defined", procName, 1);
    if (!pix_ma)
        return ERROR_INT("pix_ma not defined", procName, 1);
    if (!dpix_msa)
        return ERROR_INT("dpix_msa not defined", procName, 1);

        /* Clip rectangle to image */
    pixGetDimensions(pixs, &w, &h, NULL);
    boxc = boxClipToRectangle(box, w, h);
    boxGetGeometry(boxc, &bx, &by, &bw, &bh);
    boxDestroy(&boxc);

    if (bw == 0 || bh == 0)
        return ERROR_INT("no pixels in box", procName, 1);

        /* Use up to 4 points in the accumulators */
    norm = 1.0 / (bw * bh);
    if (bx > 0 && by > 0) {
        pixGetPixel(pix_ma, bx + bw - 1, by + bh - 1, &val11);
        pixGetPixel(pix_ma, bx + bw - 1, by - 1, &val10);
        pixGetPixel(pix_ma, bx - 1, by + bh - 1, &val01);
        pixGetPixel(pix_ma, bx - 1, by - 1, &val00);
        dpixGetPixel(dpix_msa, bx + bw - 1, by + bh - 1, &dval11);
        dpixGetPixel(dpix_msa, bx + bw - 1, by - 1, &dval10);
        dpixGetPixel(dpix_msa, bx - 1, by + bh - 1, &dval01);
        dpixGetPixel(dpix_msa, bx - 1, by - 1, &dval00);
        mval = norm * (val11 - val01 + val00 - val10);
        msval = norm * (dval11 - dval01 + dval00 - dval10);
        var = (msval - mval * mval);
        if (pvar) *pvar = (l_float32)var;
        if (prvar) *prvar = (l_float32)(sqrt(var));
    } else if (by > 0) {  /* bx == 0 */
        pixGetPixel(pix_ma, bw - 1, by + bh - 1, &val11);
        pixGetPixel(pix_ma, bw - 1, by - 1, &val10);
        dpixGetPixel(dpix_msa, bw - 1, by + bh - 1, &dval11);
        dpixGetPixel(dpix_msa, bw - 1, by - 1, &dval10);
        mval = norm * (val11 - val10);
        msval = norm * (dval11 - dval10);
        var = (msval - mval * mval);
        if (pvar) *pvar = (l_float32)var;
        if (prvar) *prvar = (l_float32)(sqrt(var));
    } else if (bx > 0) {  /* by == 0 */
        pixGetPixel(pix_ma, bx + bw - 1, bh - 1, &val11);
        pixGetPixel(pix_ma, bx - 1, bh - 1, &val01);
        dpixGetPixel(dpix_msa, bx + bw - 1, bh - 1, &dval11);
        dpixGetPixel(dpix_msa, bx - 1, bh - 1, &dval01);
        mval = norm * (val11 - val01);
        msval = norm * (dval11 - dval01);
        var = (msval - mval * mval);
        if (pvar) *pvar = (l_float32)var;
        if (prvar) *prvar = (l_float32)(sqrt(var));
    } else {  /* bx == 0 && by == 0 */
        pixGetPixel(pix_ma, bw - 1, bh - 1, &val11);
        dpixGetPixel(dpix_msa, bw - 1, bh - 1, &dval11);
        mval = norm * val11;
        msval = norm * dval11;
        var = (msval - mval * mval);
        if (pvar) *pvar = (l_float32)var;
        if (prvar) *prvar = (l_float32)(sqrt(var));
    }

    return 0;
}


/*----------------------------------------------------------------------*
 *                            Quadtree regions                          *
 *----------------------------------------------------------------------*/
/*!
 * \brief   boxaaQuadtreeRegions()
 *
 * \param[in]    w, h     size of pix that is being quadtree-ized
 * \param[in]    nlevels  number of levels in quadtree
 * \return  baa for quadtree regions at each level, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The returned boxaa has %nlevels of boxa, each containing
 *          the set of rectangles at that level.  The rectangle at
 *          level 0 is the entire region; at level 1 the region is
 *          divided into 4 rectangles, and at level n there are n^4
 *          rectangles.
 *      (2) At each level, the rectangles in the boxa are in "raster"
 *          order, with LR (fast scan) and TB (slow scan).
 * </pre>
 */
BOXAA *
boxaaQuadtreeRegions(l_int32  w,
                     l_int32  h,
                     l_int32  nlevels)
{
l_int32   i, j, k, maxpts, nside, nbox, bw, bh;
l_int32  *xstart, *xend, *ystart, *yend;
BOX      *box;
BOXA     *boxa;
BOXAA    *baa;

    PROCNAME("boxaaQuadtreeRegions");

    if (nlevels < 1)
        return (BOXAA *)ERROR_PTR("nlevels must be >= 1", procName, NULL);
    if (w < (1 << (nlevels - 1)))
        return (BOXAA *)ERROR_PTR("w doesn't support nlevels", procName, NULL);
    if (h < (1 << (nlevels - 1)))
        return (BOXAA *)ERROR_PTR("h doesn't support nlevels", procName, NULL);

    baa = boxaaCreate(nlevels);
    maxpts = 1 << (nlevels - 1);
    xstart = (l_int32 *)LEPT_CALLOC(maxpts, sizeof(l_int32));
    xend = (l_int32 *)LEPT_CALLOC(maxpts, sizeof(l_int32));
    ystart = (l_int32 *)LEPT_CALLOC(maxpts, sizeof(l_int32));
    yend = (l_int32 *)LEPT_CALLOC(maxpts, sizeof(l_int32));
    for (k = 0; k < nlevels; k++) {
        nside = 1 << k;  /* number of boxes in each direction */
        for (i = 0; i < nside; i++) {
            xstart[i] = (w - 1) * i / nside;
            if (i > 0) xstart[i]++;
            xend[i] = (w - 1) * (i + 1) / nside;
            ystart[i] = (h - 1) * i / nside;
            if (i > 0) ystart[i]++;
            yend[i] = (h - 1) * (i + 1) / nside;
#if DEBUG_BOXES
            fprintf(stderr,
               "k = %d, xs[%d] = %d, xe[%d] = %d, ys[%d] = %d, ye[%d] = %d\n",
                    k, i, xstart[i], i, xend[i], i, ystart[i], i, yend[i]);
#endif  /* DEBUG_BOXES */
        }
        nbox = 1 << (2 * k);
        boxa = boxaCreate(nbox);
        for (i = 0; i < nside; i++) {
            bh = yend[i] - ystart[i] + 1;
            for (j = 0; j < nside; j++) {
                bw = xend[j] - xstart[j] + 1;
                box = boxCreate(xstart[j], ystart[i], bw, bh);
                boxaAddBox(boxa, box, L_INSERT);
            }
        }
        boxaaAddBoxa(baa, boxa, L_INSERT);
    }

    LEPT_FREE(xstart);
    LEPT_FREE(xend);
    LEPT_FREE(ystart);
    LEPT_FREE(yend);
    return baa;
}


/*----------------------------------------------------------------------*
 *                            Quadtree access                           *
 *----------------------------------------------------------------------*/
/*!
 * \brief   quadtreeGetParent()
 *
 * \param[in]    fpixa      mean, variance or root variance
 * \param[in]    level,     x, y of current pixel
 * \param[out]   pval       parent pixel value, or 0.0 on error
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Check return value for error.  On error, val is returned as 0.0.
 *      (2) The parent is located at:
 *             level - 1
 *             (x/2, y/2)
 * </pre>
 */
l_int32
quadtreeGetParent(FPIXA      *fpixa,
                  l_int32     level,
                  l_int32     x,
                  l_int32     y,
                  l_float32  *pval)
{
l_int32  n;

    PROCNAME("quadtreeGetParent");

    if (!pval)
        return ERROR_INT("&val not defined", procName, 1);
    *pval = 0.0;
    if (!fpixa)
        return ERROR_INT("fpixa not defined", procName, 1);
    n = fpixaGetCount(fpixa);
    if (level < 1 || level >= n)
        return ERROR_INT("invalid level", procName, 1);

    if (fpixaGetPixel(fpixa, level - 1, x / 2, y / 2, pval) != 0)
        return ERROR_INT("invalid coordinates", procName, 1);
    return 0;
}


/*!
 * \brief   quadtreeGetChildren()
 *
 * \param[in]    fpixa            mean, variance or root variance
 * \param[in]    level,           x, y of current pixel
 * \param[out]   pval00, pval01,
 *               pval10, pval11   four child pixel values
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Check return value for error.  On error, all return vals are 0.0.
 *      (2) The returned child pixels are located at:
 *             level + 1
 *             (2x, 2y), (2x+1, 2y), (2x, 2y+1), (2x+1, 2y+1)
 * </pre>
 */
l_int32
quadtreeGetChildren(FPIXA      *fpixa,
                    l_int32     level,
                    l_int32     x,
                    l_int32     y,
                    l_float32  *pval00,
                    l_float32  *pval10,
                    l_float32  *pval01,
                    l_float32  *pval11)
{
l_int32  n;

    PROCNAME("quadtreeGetChildren");

    if (!pval00 || !pval01 || !pval10 || !pval11)
        return ERROR_INT("&val* not all defined", procName, 1);
    *pval00 = *pval10 = *pval01 = *pval11 = 0.0;
    if (!fpixa)
        return ERROR_INT("fpixa not defined", procName, 1);
    n = fpixaGetCount(fpixa);
    if (level < 0 || level >= n - 1)
        return ERROR_INT("invalid level", procName, 1);

    if (fpixaGetPixel(fpixa, level + 1, 2 * x, 2 * y, pval00) != 0)
        return ERROR_INT("invalid coordinates", procName, 1);
    fpixaGetPixel(fpixa, level + 1, 2 * x + 1, 2 * y, pval10);
    fpixaGetPixel(fpixa, level + 1, 2 * x, 2 * y + 1, pval01);
    fpixaGetPixel(fpixa, level + 1, 2 * x + 1, 2 * y + 1, pval11);
    return 0;
}


/*!
 * \brief   quadtreeMaxLevels()
 *
 * \param[in]    w, h    dimensions of image
 * \return  maxlevels maximum number of levels allowed, or -1 on error
 *
 * <pre>
 * Notes:
 *      (1) The criterion for maxlevels is that the subdivision not
 *          go down below the single pixel level.  The 1.5 factor
 *          is intended to keep any rectangle from accidentally
 *          having zero dimension due to integer truncation.
 * </pre>
 */
l_int32
quadtreeMaxLevels(l_int32  w,
                  l_int32  h)
{
l_int32  i, minside;

    minside = L_MIN(w, h);
    for (i = 0; i < 20; i++) {  /* 2^10 = one million */
        if (minside < (1.5 * (1 << i)))
            return i - 1;
    }

    return -1;  /* fail if the image has over a trillion pixels! */
}


/*----------------------------------------------------------------------*
 *                            Display quadtree                          *
 *----------------------------------------------------------------------*/
/*!
 * \brief   fpixaDisplayQuadtree()
 *
 * \param[in]    fpixa     mean, variance or root variance
 * \param[in]    factor    replication factor at lowest level
 * \param[in]    fontsize  4, ... 20
 * \return  pixd 8 bpp, mosaic of quadtree images, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The mean and root variance fall naturally in the 8 bpp range,
 *          but the variance is typically outside the range.  This
 *          function displays 8 bpp pix clipped to 255, so the image
 *          pixels will mostly be 255 (white).
 * </pre>
 */
PIX *
fpixaDisplayQuadtree(FPIXA   *fpixa,
                     l_int32  factor,
                     l_int32  fontsize)
{
char       buf[256];
l_int32    nlevels, i, mag, w;
L_BMF     *bmf;
FPIX      *fpix;
PIX       *pixt1, *pixt2, *pixt3, *pixt4, *pixd;
PIXA      *pixat;

    PROCNAME("fpixaDisplayQuadtree");

    if (!fpixa)
        return (PIX *)ERROR_PTR("fpixa not defined", procName, NULL);

    if ((nlevels = fpixaGetCount(fpixa)) == 0)
        return (PIX *)ERROR_PTR("pixas empty", procName, NULL);

    if ((bmf = bmfCreate(NULL, fontsize)) == NULL)
        L_ERROR("bmf not made; text will not be added", procName);
    pixat = pixaCreate(nlevels);
    for (i = 0; i < nlevels; i++) {
        fpix = fpixaGetFPix(fpixa, i, L_CLONE);
        pixt1 = fpixConvertToPix(fpix, 8, L_CLIP_TO_ZERO, 0);
        mag = factor * (1 << (nlevels - i - 1));
        pixt2 = pixExpandReplicate(pixt1, mag);
        pixt3 = pixConvertTo32(pixt2);
        snprintf(buf, sizeof(buf), "Level %d\n", i);
        pixt4 = pixAddSingleTextblock(pixt3, bmf, buf, 0xff000000,
                                      L_ADD_BELOW, NULL);
        pixaAddPix(pixat, pixt4, L_INSERT);
        fpixDestroy(&fpix);
        pixDestroy(&pixt1);
        pixDestroy(&pixt2);
        pixDestroy(&pixt3);
    }
    w = pixGetWidth(pixt4);
    pixd = pixaDisplayTiledInRows(pixat, 32, nlevels * (w + 80), 1.0, 0, 30, 2);

    pixaDestroy(&pixat);
    bmfDestroy(&bmf);
    return pixd;
}
