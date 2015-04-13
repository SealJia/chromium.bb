/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "platform/graphics/ContentLayerDelegate.h"

#include "platform/EventTracer.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/transforms/AffineTransform.h"
#include "platform/transforms/TransformationMatrix.h"
#include "public/platform/WebDisplayItemList.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebRect.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"

namespace blink {

ContentLayerDelegate::ContentLayerDelegate(GraphicsContextPainter* painter)
    : m_painter(painter)
    , m_opaque(false)
{
}

ContentLayerDelegate::~ContentLayerDelegate()
{
}

void ContentLayerDelegate::paintContents(
    SkCanvas* canvas, const WebRect& clip, bool canPaintLCDText,
    WebContentLayerClient::GraphicsContextStatus contextStatus)
{
    static const unsigned char* annotationsEnabled = 0;
    if (UNLIKELY(!annotationsEnabled))
        annotationsEnabled = EventTracer::getTraceCategoryEnabledFlag(TRACE_DISABLED_BY_DEFAULT("blink.graphics_context_annotations"));

    GraphicsContext context(canvas, m_painter->displayItemList(), contextStatus == WebContentLayerClient::GraphicsContextEnabled ? GraphicsContext::NothingDisabled : GraphicsContext::FullyDisabled);
    context.setCertainlyOpaque(m_opaque);
    context.setShouldSmoothFonts(canPaintLCDText);
    if (*annotationsEnabled)
        context.setAnnotationMode(AnnotateAll);

    m_painter->paint(context, clip);
}

void ContentLayerDelegate::paintContents(
    WebDisplayItemList* webDisplayItemList, const WebRect& clip, bool canPaintLCDText,
    WebContentLayerClient::GraphicsContextStatus contextStatus)
{
    // Once Slimming Paint is fully implemented, this method will no longer
    // be needed since Blink will be in charge of creating the display list
    // during the document lifecylcle.

    // Some layers don't yet produce display lists. To handle such layers, we
    // create a canvas backed by an SkPicture, and manually insert this
    // SkPicture into the WebDisplayItemList when the layer's display list is
    // empty.
    SkPictureRecorder recorder;
    RefPtr<SkPicture> picture;
    SkCanvas* canvas = recorder.beginRecording(clip.width, clip.height);
    canvas->save();
    canvas->translate(-clip.x, -clip.y);
    canvas->clipRect(SkRect::MakeXYWH(clip.x, clip.y, clip.width, clip.height));
    paintContents(canvas, clip, canPaintLCDText, contextStatus);
    canvas->restore();
    picture = adoptRef(recorder.endRecording());

    ASSERT(m_painter->displayItemList());

    const PaintList& paintList = m_painter->displayItemList()->paintList();
    for (PaintList::const_iterator it = paintList.begin(); it != paintList.end(); ++it)
        (*it)->appendToWebDisplayItemList(webDisplayItemList);
}

} // namespace blink
