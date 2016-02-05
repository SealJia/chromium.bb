// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayerFixedPositionRecorder_h
#define LayerFixedPositionRecorder_h

#include "wtf/Allocator.h"

namespace blink {

class GraphicsContext;
class LayoutBoxModelObject;

class LayerFixedPositionRecorder {
    USING_FAST_MALLOC(LayerFixedPositionRecorder);
public:
    explicit LayerFixedPositionRecorder(GraphicsContext&, const LayoutBoxModelObject&);
    ~LayerFixedPositionRecorder();

private:
    GraphicsContext& m_graphicsContext;
    const LayoutBoxModelObject& m_layoutObject;
    bool m_isFixedPosition;
    bool m_isFixedPositionContainer;
};

} // namespace blink

#endif // LayerFixedPositionRecorder_h
