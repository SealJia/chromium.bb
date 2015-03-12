/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/rendering/RenderTableRow.h"

#include "core/HTMLNames.h"
#include "core/fetch/ImageResource.h"
#include "core/paint/TableRowPainter.h"
#include "core/rendering/GraphicsContextAnnotator.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderTableCell.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/SubtreeLayoutScope.h"
#include "core/rendering/style/StyleInheritedData.h"

namespace blink {

using namespace HTMLNames;

RenderTableRow::RenderTableRow(Element* element)
    : RenderBox(element)
    , m_rowIndex(unsetRowIndex)
{
    // init RenderObject attributes
    setInline(false);   // our object is not Inline
}

void RenderTableRow::trace(Visitor* visitor)
{
    visitor->trace(m_children);
    RenderBox::trace(visitor);
}

void RenderTableRow::willBeRemovedFromTree()
{
    RenderBox::willBeRemovedFromTree();

    section()->setNeedsCellRecalc();
}

static bool borderWidthChanged(const RenderStyle* oldStyle, const RenderStyle* newStyle)
{
    return oldStyle->borderLeftWidth() != newStyle->borderLeftWidth()
        || oldStyle->borderTopWidth() != newStyle->borderTopWidth()
        || oldStyle->borderRightWidth() != newStyle->borderRightWidth()
        || oldStyle->borderBottomWidth() != newStyle->borderBottomWidth();
}

void RenderTableRow::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    ASSERT(style()->display() == TABLE_ROW);

    RenderBox::styleDidChange(diff, oldStyle);
    propagateStyleToAnonymousChildren();

    if (section() && oldStyle && style()->logicalHeight() != oldStyle->logicalHeight())
        section()->rowLogicalHeightChanged(this);

    // If border was changed, notify table.
    if (parent()) {
        RenderTable* table = this->table();
        if (table && !table->selfNeedsLayout() && !table->normalChildNeedsLayout() && oldStyle && oldStyle->border() != style()->border())
            table->invalidateCollapsedBorders();

        if (table && oldStyle && diff.needsFullLayout() && needsLayout() && table->collapseBorders() && borderWidthChanged(oldStyle, style())) {
            // If the border width changes on a row, we need to make sure the cells in the row know to lay out again.
            // This only happens when borders are collapsed, since they end up affecting the border sides of the cell
            // itself.
            for (RenderBox* childBox = firstChildBox(); childBox; childBox = childBox->nextSiblingBox()) {
                if (!childBox->isTableCell())
                    continue;
                childBox->setChildNeedsLayout();
            }
        }
    }
}

const BorderValue& RenderTableRow::borderAdjoiningStartCell(const RenderTableCell* cell) const
{
    ASSERT_UNUSED(cell, cell->isFirstOrLastCellInRow());
    // FIXME: https://webkit.org/b/79272 - Add support for mixed directionality at the cell level.
    return style()->borderStart();
}

const BorderValue& RenderTableRow::borderAdjoiningEndCell(const RenderTableCell* cell) const
{
    ASSERT_UNUSED(cell, cell->isFirstOrLastCellInRow());
    // FIXME: https://webkit.org/b/79272 - Add support for mixed directionality at the cell level.
    return style()->borderEnd();
}

void RenderTableRow::addChild(RenderObject* child, RenderObject* beforeChild)
{
    if (!child->isTableCell()) {
        RenderObject* last = beforeChild;
        if (!last)
            last = lastCell();
        if (last && last->isAnonymous() && last->isTableCell() && !last->isBeforeOrAfterContent()) {
            RenderTableCell* lastCell = toRenderTableCell(last);
            if (beforeChild == lastCell)
                beforeChild = lastCell->firstChild();
            lastCell->addChild(child, beforeChild);
            return;
        }

        if (beforeChild && !beforeChild->isAnonymous() && beforeChild->parent() == this) {
            RenderObject* cell = beforeChild->previousSibling();
            if (cell && cell->isTableCell() && cell->isAnonymous()) {
                cell->addChild(child);
                return;
            }
        }

        // If beforeChild is inside an anonymous cell, insert into the cell.
        if (last && !last->isTableCell() && last->parent() && last->parent()->isAnonymous() && !last->parent()->isBeforeOrAfterContent()) {
            last->parent()->addChild(child, beforeChild);
            return;
        }

        RenderTableCell* cell = RenderTableCell::createAnonymousWithParentRenderer(this);
        addChild(cell, beforeChild);
        cell->addChild(child);
        return;
    }

    if (beforeChild && beforeChild->parent() != this)
        beforeChild = splitAnonymousBoxesAroundChild(beforeChild);

    RenderTableCell* cell = toRenderTableCell(child);

    // Generated content can result in us having a null section so make sure to null check our parent.
    if (parent())
        section()->addCell(cell, this);

    ASSERT(!beforeChild || beforeChild->isTableCell());
    RenderBox::addChild(cell, beforeChild);

    if (beforeChild || nextRow())
        section()->setNeedsCellRecalc();
}

void RenderTableRow::layout()
{
    ASSERT(needsLayout());

    // Table rows do not add translation.
    LayoutState state(*this, LayoutSize());

    for (RenderTableCell* cell = firstCell(); cell; cell = cell->nextCell()) {
        SubtreeLayoutScope layouter(*cell);
        if (!cell->needsLayout())
            cell->markForPaginationRelayoutIfNeeded(layouter);
        if (cell->needsLayout()) {
            cell->computeAndSetBlockDirectionMargins(table());
            cell->layout();
        }
    }

    m_overflow.clear();
    addVisualEffectOverflow();
    // We do not call addOverflowFromCell here. The cell are laid out to be
    // measured above and will be sized correctly in a follow-up phase.

    // We only ever need to issue paint invalidations if our cells didn't, which means that they didn't need
    // layout, so we know that our bounds didn't change. This code is just making up for
    // the fact that we did not invalidate paints in setStyle() because we had a layout hint.
    if (selfNeedsLayout()) {
        for (RenderTableCell* cell = firstCell(); cell; cell = cell->nextCell()) {
            // FIXME: Is this needed when issuing paint invalidations after layout?
            cell->setShouldDoFullPaintInvalidation();
        }
    }

    // RenderTableSection::layoutRows will set our logical height and width later, so it calls updateLayerTransform().
    clearNeedsLayout();
}

// Hit Testing
bool RenderTableRow::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    // Table rows cannot ever be hit tested.  Effectively they do not exist.
    // Just forward to our children always.
    for (RenderTableCell* cell = lastCell(); cell; cell = cell->previousCell()) {
        // FIXME: We have to skip over inline flows, since they can show up inside table rows
        // at the moment (a demoted inline <form> for example). If we ever implement a
        // table-specific hit-test method (which we should do for performance reasons anyway),
        // then we can remove this check.
        if (!cell->hasSelfPaintingLayer()) {
            LayoutPoint cellPoint = flipForWritingModeForChild(cell, accumulatedOffset);
            if (cell->nodeAtPoint(request, result, locationInContainer, cellPoint, action)) {
                updateHitTestResult(result, locationInContainer.point() - toLayoutSize(cellPoint));
                return true;
            }
        }
    }

    return false;
}

void RenderTableRow::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    TableRowPainter(*this).paint(paintInfo, paintOffset);
}

void RenderTableRow::imageChanged(WrappedImagePtr, const IntRect*)
{
    // FIXME: Examine cells and issue paint invalidations of only the rect the image paints in.
    setShouldDoFullPaintInvalidation();
}

RenderTableRow* RenderTableRow::createAnonymous(Document* document)
{
    RenderTableRow* renderer = new RenderTableRow(0);
    renderer->setDocumentForAnonymous(document);
    return renderer;
}

RenderTableRow* RenderTableRow::createAnonymousWithParentRenderer(const RenderObject* parent)
{
    RenderTableRow* newRow = RenderTableRow::createAnonymous(&parent->document());
    RefPtr<RenderStyle> newStyle = RenderStyle::createAnonymousStyleWithDisplay(parent->style(), TABLE_ROW);
    newRow->setStyle(newStyle.release());
    return newRow;
}

void RenderTableRow::addOverflowFromCell(const RenderTableCell* cell)
{
    // Non-row-spanning-cells don't create overflow (they are fully contained within this row).
    if (cell->rowSpan() == 1)
        return;

    // Cells only generates visual overflow.
    LayoutRect cellVisualOverflowRect = cell->visualOverflowRectForPropagation(style());

    // The cell and the row share the section's coordinate system. However
    // the visual overflow should be determined in the coordinate system of
    // the row, that's why we shift it below.
    LayoutUnit cellOffsetLogicalTopDifference = cell->location().y() - location().y();
    cellVisualOverflowRect.move(0, cellOffsetLogicalTopDifference);

    addVisualOverflow(cellVisualOverflowRect);
}

} // namespace blink
