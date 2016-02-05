/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "config.h"
#include "core/paint/PaintLayerClipper.h"

#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"

namespace blink {

static void adjustClipRectsForChildren(const LayoutObject& layoutObject, ClipRects& clipRects)
{
    EPosition position = layoutObject.style()->position();
    // A fixed object is essentially the root of its containing block hierarchy, so when
    // we encounter such an object, we reset our clip rects to the fixedClipRect.
    if (position == FixedPosition) {
        clipRects.setPosClipRect(clipRects.fixedClipRect());
        clipRects.setOverflowClipRect(clipRects.fixedClipRect());
        clipRects.setFixed(true);
    } else if (position == RelativePosition) {
        clipRects.setPosClipRect(clipRects.overflowClipRect());
    } else if (position == AbsolutePosition) {
        clipRects.setOverflowClipRect(clipRects.posClipRect());
    }
}

static void applyClipRects(const ClipRectsContext& context, const LayoutObject& layoutObject, LayoutPoint offset, ClipRects& clipRects)
{
    ASSERT(layoutObject.hasOverflowClip() || layoutObject.hasClip());
    LayoutView* view = layoutObject.view();
    ASSERT(view);
    if (clipRects.fixed() && context.rootLayer->layoutObject() == view)
        offset -= toIntSize(view->frameView()->scrollPosition());
    if (layoutObject.hasOverflowClip()) {
        ClipRect newOverflowClip = toLayoutBox(layoutObject).overflowClipRect(offset, context.scrollbarRelevancy);
        newOverflowClip.setHasRadius(layoutObject.style()->hasBorderRadius());
        clipRects.setOverflowClipRect(intersection(newOverflowClip, clipRects.overflowClipRect()));
        if (layoutObject.isPositioned())
            clipRects.setPosClipRect(intersection(newOverflowClip, clipRects.posClipRect()));
        if (layoutObject.isLayoutView())
            clipRects.setFixedClipRect(intersection(newOverflowClip, clipRects.fixedClipRect()));
    }
    if (layoutObject.hasClip()) {
        LayoutRect newClip = toLayoutBox(layoutObject).clipRect(offset);
        clipRects.setPosClipRect(intersection(newClip, clipRects.posClipRect()).setIsClippedByClipCss());
        clipRects.setOverflowClipRect(intersection(newClip, clipRects.overflowClipRect()).setIsClippedByClipCss());
        clipRects.setFixedClipRect(intersection(newClip, clipRects.fixedClipRect()).setIsClippedByClipCss());

    }
}

PaintLayerClipper::PaintLayerClipper(const LayoutBoxModelObject& layoutObject)
    : m_layoutObject(layoutObject)
{
}

ClipRects* PaintLayerClipper::clipRectsIfCached(const ClipRectsContext& context) const
{
    ASSERT(context.usesCache());
    if (!m_cache)
        return 0;
    ClipRectsCache::Entry& entry = m_cache->get(context.cacheSlot());
    // FIXME: We used to ASSERT that we always got a consistent root layer.
    // We should add a test that has an inconsistent root. See
    // http://crbug.com/366118 for an example.
    if (context.rootLayer != entry.root)
        return 0;
    ASSERT(entry.scrollbarRelevancy == context.scrollbarRelevancy);
#ifdef CHECK_CACHED_CLIP_RECTS
    // This code is useful to check cached clip rects, but is too expensive to leave enabled in debug builds by default.
    ClipRectsContext tempContext(context);
    tempContext.cacheSlot = UncachedClipRects;
    RefPtr<ClipRects> clipRects = ClipRects::create();
    calculateClipRects(tempContext, *clipRects);
    ASSERT(clipRects == *entry.clipRects);
#endif
    return entry.clipRects.get();
}
ClipRects* PaintLayerClipper::storeClipRectsInCache(const ClipRectsContext& context, ClipRects* parentClipRects, const ClipRects& clipRects) const
{
    ClipRectsCache::Entry& entry = cache().get(context.cacheSlot());
    entry.root = context.rootLayer;
#if ENABLE(ASSERT)
    entry.scrollbarRelevancy = context.scrollbarRelevancy;
#endif
    if (parentClipRects) {
        // If our clip rects match the clip rects of our parent, we share storage.
        if (clipRects == *parentClipRects) {
            entry.clipRects = parentClipRects;
            return parentClipRects;
        }
    }
    entry.clipRects = ClipRects::create(clipRects);
    return entry.clipRects.get();
}
ClipRects* PaintLayerClipper::getClipRects(const ClipRectsContext& context) const
{
    if (ClipRects* result = clipRectsIfCached(context))
        return result;
    // Note that it's important that we call getClipRects on our parent
    // before we call calculateClipRects so that calculateClipRects will hit
    // the cache.
    ClipRects* parentClipRects = 0;
    if (context.rootLayer != m_layoutObject.layer() && m_layoutObject.layer()->parent())
        parentClipRects = m_layoutObject.layer()->parent()->clipper().getClipRects(context);
    RefPtr<ClipRects> clipRects = ClipRects::create();
    calculateClipRects(context, *clipRects);
    return storeClipRectsInCache(context, parentClipRects, *clipRects);
}

void PaintLayerClipper::clearClipRectsIncludingDescendants()
{
    m_cache = nullptr;

    for (PaintLayer* layer = m_layoutObject.layer()->firstChild(); layer; layer = layer->nextSibling()) {
        layer->clipper().clearClipRectsIncludingDescendants();
    }
}

void PaintLayerClipper::clearClipRectsIncludingDescendants(ClipRectsCacheSlot cacheSlot)
{
    if (m_cache)
        m_cache->clear(cacheSlot);

    for (PaintLayer* layer = m_layoutObject.layer()->firstChild(); layer; layer = layer->nextSibling()) {
        layer->clipper().clearClipRectsIncludingDescendants(cacheSlot);
    }
}


LayoutRect PaintLayerClipper::localClipRect(const PaintLayer* clippingRootLayer) const
{
    LayoutRect layerBounds;
    ClipRect backgroundRect, foregroundRect;
    ClipRectsContext context(clippingRootLayer, PaintingClipRects);
    calculateRects(context, LayoutRect(LayoutRect::infiniteIntRect()), layerBounds, backgroundRect, foregroundRect);

    LayoutRect clipRect = backgroundRect.rect();
    // TODO(chrishtr): avoid converting to IntRect and back.
    if (clipRect == LayoutRect(LayoutRect::infiniteIntRect()))
        return clipRect;

    LayoutPoint clippingRootOffset;
    m_layoutObject.layer()->convertToLayerCoords(clippingRootLayer, clippingRootOffset);
    clipRect.moveBy(-clippingRootOffset);

    return clipRect;
}

void PaintLayerClipper::calculateRects(const ClipRectsContext& context, const LayoutRect& paintDirtyRect, LayoutRect& layerBounds,
    ClipRect& backgroundRect, ClipRect& foregroundRect, const LayoutPoint* offsetFromRoot) const
{
    bool isClippingRoot = m_layoutObject.layer() == context.rootLayer;

    if (!isClippingRoot && m_layoutObject.layer()->parent()) {
        backgroundRect = backgroundClipRect(context);
        backgroundRect.move(context.subPixelAccumulation);
        backgroundRect.intersect(paintDirtyRect);
    } else {
        backgroundRect = paintDirtyRect;
    }

    foregroundRect = backgroundRect;

    LayoutPoint offset;
    if (offsetFromRoot)
        offset = *offsetFromRoot;
    else
        m_layoutObject.layer()->convertToLayerCoords(context.rootLayer, offset);
    layerBounds = LayoutRect(offset, LayoutSize(m_layoutObject.layer()->size()));

    // Update the clip rects that will be passed to child layers.
    if (m_layoutObject.hasOverflowClip() && shouldRespectOverflowClip(context)) {
        foregroundRect.intersect(toLayoutBox(m_layoutObject).overflowClipRect(offset, context.scrollbarRelevancy));
        if (m_layoutObject.style()->hasBorderRadius())
            foregroundRect.setHasRadius(true);

        // FIXME: Does not do the right thing with columns yet, since we don't yet factor in the
        // individual column boxes as overflow.

        // The LayoutView is special since its overflow clipping rect may be larger than its box rect (crbug.com/492871).
        LayoutRect layerBoundsWithVisualOverflow = m_layoutObject.isLayoutView() ? toLayoutView(m_layoutObject).viewRect() : toLayoutBox(m_layoutObject).visualOverflowRect();
        toLayoutBox(m_layoutObject).flipForWritingMode(layerBoundsWithVisualOverflow); // PaintLayer are in physical coordinates, so the overflow has to be flipped.
        layerBoundsWithVisualOverflow.moveBy(offset);
        backgroundRect.intersect(layerBoundsWithVisualOverflow);
    }

    // CSS clip (different than clipping due to overflow) can clip to any box, even if it falls outside of the border box.
    if (m_layoutObject.hasClip()) {
        // Clip applies to *us* as well, so go ahead and update the damageRect.
        LayoutRect newPosClip = toLayoutBox(m_layoutObject).clipRect(offset);
        backgroundRect.intersect(newPosClip);
        backgroundRect.setIsClippedByClipCss();
        foregroundRect.intersect(newPosClip);
        foregroundRect.setIsClippedByClipCss();
    }
}

void PaintLayerClipper::calculateClipRects(const ClipRectsContext& context, ClipRects& clipRects) const
{
    bool rootLayerScrolls = m_layoutObject.document().settings() && m_layoutObject.document().settings()->rootLayerScrolls();
    if (!m_layoutObject.layer()->parent() && !rootLayerScrolls) {
        // The root layer's clip rect is always infinite.
        clipRects.reset(LayoutRect(LayoutRect::infiniteIntRect()));
        return;
    }

    bool isClippingRoot = m_layoutObject.layer() == context.rootLayer;

    // For transformed layers, the root layer was shifted to be us, so there is no need to
    // examine the parent. We want to cache clip rects with us as the root.
    PaintLayer* parentLayer = !isClippingRoot ? m_layoutObject.layer()->parent() : 0;
    // Ensure that our parent's clip has been calculated so that we can examine the values.
    if (parentLayer) {
        // FIXME: Why don't we just call getClipRects here?
        if (context.usesCache() && parentLayer->clipper().cachedClipRects(context)) {
            clipRects = *parentLayer->clipper().cachedClipRects(context);
        } else {
            parentLayer->clipper().calculateClipRects(context, clipRects);
        }
    } else {
        clipRects.reset(LayoutRect(LayoutRect::infiniteIntRect()));
    }

    adjustClipRectsForChildren(m_layoutObject, clipRects);

    if ((m_layoutObject.hasOverflowClip() && shouldRespectOverflowClip(context)) || m_layoutObject.hasClip()) {
        // This offset cannot use convertToLayerCoords, because sometimes our rootLayer may be across
        // some transformed layer boundary, for example, in the PaintLayerCompositor overlapMap, where
        // clipRects are needed in view space.
        applyClipRects(context, m_layoutObject, roundedLayoutPoint(m_layoutObject.localToContainerPoint(FloatPoint(), context.rootLayer->layoutObject())), clipRects);
    }
}

static ClipRect backgroundClipRectForPosition(const ClipRects& parentRects, EPosition position)
{
    if (position == FixedPosition)
        return parentRects.fixedClipRect();

    if (position == AbsolutePosition)
        return parentRects.posClipRect();

    return parentRects.overflowClipRect();
}

ClipRect PaintLayerClipper::backgroundClipRect(const ClipRectsContext& context) const
{
    ASSERT(m_layoutObject.layer()->parent());
    ASSERT(m_layoutObject.view());

    RefPtr<ClipRects> parentClipRects = ClipRects::create();
    if (m_layoutObject.layer() == context.rootLayer)
        parentClipRects->reset(LayoutRect(LayoutRect::infiniteIntRect()));
    else
        m_layoutObject.layer()->parent()->clipper().getOrCalculateClipRects(context, *parentClipRects);

    ClipRect result = backgroundClipRectForPosition(*parentClipRects, m_layoutObject.style()->position());

    // Note: infinite clipRects should not be scrolled here, otherwise they will accidentally no longer be considered infinite.
    if (parentClipRects->fixed() && context.rootLayer->layoutObject() == m_layoutObject.view() && result != LayoutRect(LayoutRect::infiniteIntRect()))
        result.move(toIntSize(m_layoutObject.view()->frameView()->scrollPosition()));

    return result;
}

void PaintLayerClipper::getOrCalculateClipRects(const ClipRectsContext& context, ClipRects& clipRects) const
{
    if (context.usesCache())
        clipRects = *getClipRects(context);
    else
        calculateClipRects(context, clipRects);
}

bool PaintLayerClipper::shouldRespectOverflowClip(const ClipRectsContext& context) const
{
    PaintLayer* layer = m_layoutObject.layer();
    if (layer != context.rootLayer)
        return true;

    if (context.respectOverflowClip == IgnoreOverflowClip)
        return false;

    if (layer->isRootLayer() && context.respectOverflowClipForViewport == IgnoreOverflowClip)
        return false;

    return true;
}

} // namespace blink
