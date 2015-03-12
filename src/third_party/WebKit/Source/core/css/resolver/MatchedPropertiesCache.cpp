/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "core/css/resolver/MatchedPropertiesCache.h"

#include "core/css/StylePropertySet.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/rendering/style/RenderStyle.h"

namespace blink {

#if ENABLE(OILPAN)
bool CachedMatchedPropertiesHashTraits::traceInCollection(Visitor* visitor, Member<CachedMatchedProperties>& cachedProperties, WTF::ShouldWeakPointersBeMarkedStrongly strongify)
{
    // Only honor the cache's weakness semantics if the collection is traced
    // with WeakPointersActWeak. Otherwise just trace the cachedProperties
    // strongly, ie. call trace on it.
    if (cachedProperties && strongify == WTF::WeakPointersActWeak) {
        // A given cache entry is only kept alive if none of the MatchedProperties
        // in the CachedMatchedProperties value contain a dead "properties" field.
        // If there is a dead field the entire cache entry is removed.
        for (const auto& matchedProperties : cachedProperties->matchedProperties) {
            if (!visitor->isAlive(matchedProperties.properties)) {
                // For now report the cache entry as dead. This might not
                // be the final result if in a subsequent call for this entry,
                // the "properties" field has been marked via another path.
                return true;
            }
        }
    }
    // At this point none of the entries in the matchedProperties vector
    // had a dead "properties" field so trace CachedMatchedProperties strongly.
    visitor->trace(cachedProperties);
    return false;
}
#endif

void CachedMatchedProperties::set(const RenderStyle* style, const RenderStyle* parentStyle, const MatchResult& matchResult)
{
    matchedProperties.appendVector(matchResult.matchedProperties);
    ranges = matchResult.ranges;

    // Note that we don't cache the original RenderStyle instance. It may be further modified.
    // The RenderStyle in the cache is really just a holder for the substructures and never used as-is.
    this->renderStyle = RenderStyle::clone(style);
    this->parentRenderStyle = RenderStyle::clone(parentStyle);
}

void CachedMatchedProperties::clear()
{
    matchedProperties.clear();
    renderStyle = nullptr;
    parentRenderStyle = nullptr;
}

MatchedPropertiesCache::MatchedPropertiesCache()
#if !ENABLE(OILPAN)
    : m_additionsSinceLastSweep(0)
    , m_sweepTimer(this, &MatchedPropertiesCache::sweep)
#endif
{
}

const CachedMatchedProperties* MatchedPropertiesCache::find(unsigned hash, const StyleResolverState& styleResolverState, const MatchResult& matchResult)
{
    ASSERT(hash);

    Cache::iterator it = m_cache.find(hash);
    if (it == m_cache.end())
        return 0;
    CachedMatchedProperties* cacheItem = it->value.get();
    ASSERT(cacheItem);

    size_t size = matchResult.matchedProperties.size();
    if (size != cacheItem->matchedProperties.size())
        return 0;
    if (cacheItem->renderStyle->insideLink() != styleResolverState.style()->insideLink())
        return 0;
    for (size_t i = 0; i < size; ++i) {
        if (matchResult.matchedProperties[i] != cacheItem->matchedProperties[i])
            return 0;
    }
    if (cacheItem->ranges != matchResult.ranges)
        return 0;
    return cacheItem;
}

void MatchedPropertiesCache::add(const RenderStyle* style, const RenderStyle* parentStyle, unsigned hash, const MatchResult& matchResult)
{
#if !ENABLE(OILPAN)
    static const unsigned maxAdditionsBetweenSweeps = 100;
    if (++m_additionsSinceLastSweep >= maxAdditionsBetweenSweeps
        && !m_sweepTimer.isActive()) {
        static const unsigned sweepTimeInSeconds = 60;
        m_sweepTimer.startOneShot(sweepTimeInSeconds, FROM_HERE);
    }
#endif

    ASSERT(hash);
    Cache::AddResult addResult = m_cache.add(hash, nullptr);
    if (addResult.isNewEntry)
        addResult.storedValue->value = adoptPtrWillBeNoop(new CachedMatchedProperties);

    CachedMatchedProperties* cacheItem = addResult.storedValue->value.get();
    if (!addResult.isNewEntry)
        cacheItem->clear();

    cacheItem->set(style, parentStyle, matchResult);
}

void MatchedPropertiesCache::clear()
{
    m_cache.clear();
}

void MatchedPropertiesCache::clearViewportDependent()
{
    Vector<unsigned, 16> toRemove;
    for (const auto& cacheEntry : m_cache) {
        CachedMatchedProperties* cacheItem = cacheEntry.value.get();
        if (cacheItem->renderStyle->hasViewportUnits())
            toRemove.append(cacheEntry.key);
    }
    m_cache.removeAll(toRemove);
}

#if !ENABLE(OILPAN)
void MatchedPropertiesCache::sweep(Timer<MatchedPropertiesCache>*)
{
    // Look for cache entries containing a style declaration with a single ref and remove them.
    // This may happen when an element attribute mutation causes it to generate a new inlineStyle()
    // or presentationAttributeStyle(), potentially leaving this cache with the last ref on the old one.
    Vector<unsigned, 16> toRemove;
    for (const auto& cacheEntry : m_cache) {
        CachedMatchedProperties* cacheItem = cacheEntry.value.get();
        Vector<MatchedProperties>& matchedProperties = cacheItem->matchedProperties;
        for (size_t i = 0; i < matchedProperties.size(); ++i) {
            if (matchedProperties[i].properties->hasOneRef()) {
                toRemove.append(cacheEntry.key);
                break;
            }
        }
    }
    m_cache.removeAll(toRemove);
    m_additionsSinceLastSweep = 0;
}
#endif

bool MatchedPropertiesCache::isCacheable(const Element* element, const RenderStyle* style, const RenderStyle* parentStyle)
{
    // FIXME: CSSPropertyWebkitWritingMode modifies state when applying to document element. We can't skip the applying by caching.
    if (element == element->document().documentElement() && element->document().writingModeSetOnDocumentElement())
        return false;
    if (style->unique() || (style->styleType() != NOPSEUDO && parentStyle->unique()))
        return false;
    if (style->hasAppearance())
        return false;
    if (style->zoom() != RenderStyle::initialZoom())
        return false;
    if (style->writingMode() != RenderStyle::initialWritingMode())
        return false;
    // The cache assumes static knowledge about which properties are inherited.
    if (parentStyle->hasExplicitlyInheritedProperties())
        return false;
    return true;
}

void MatchedPropertiesCache::trace(Visitor* visitor)
{
#if ENABLE(OILPAN)
    visitor->trace(m_cache);
#endif
}

}
