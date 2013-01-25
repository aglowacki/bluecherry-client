/*
 * Copyright 2010-2013 Bluecherry
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "RangeMap.h"
#include <QtAlgorithms>
#include <QDebug>

RangeMap::RangeMap()
{
}

inline bool RangeMap::rangeStartLess(const Range &a, const Range &b)
{
    return a.start() < b.start();
}

bool RangeMap::contains(unsigned position, unsigned size) const
{
    if (ranges.isEmpty())
        return false;

    Range range = Range::fromStartSize(position, size);

    QList<Range>::ConstIterator it = qLowerBound(ranges.begin(), ranges.end(), range, rangeStartLess);
    if (it == ranges.end() || it->start() > position)
    {
        if (it == ranges.begin())
            return false;
        --it;
    }

    /* Position is 1, size is 1; last pos required is 1 */
    Q_ASSERT(it->start() <= position);
    if (it != ranges.end() && it->end() >= range.end())
        return true;

    return false;
}

bool RangeMap::nextMissingRange(unsigned startPosition, unsigned totalSize, unsigned &position, unsigned &size)
{
    if (ranges.isEmpty())
    {
        position = startPosition;
        size = totalSize;
        return true;
    }

    /* Find the range inclusive of or less than startPosition */
    Range r = Range::fromValue(startPosition);

    QList<Range>::ConstIterator it = qLowerBound(ranges.begin(), ranges.end(), r, rangeStartLess);
    if (it == ranges.end())
    {
        position = startPosition;
        size = totalSize;
        return true;
    }

    if (it->start() > startPosition)
    {
        if (it == ranges.begin())
        {
            position = startPosition;
            if (startPosition + totalSize - 1 > it->start())
                size = it->start() - startPosition;
            else
                size = totalSize;
            return true;
        }
        --it;
    }

    Q_ASSERT(it->start() <= startPosition);

    position = qMax(startPosition, it->end() + 1);
    unsigned reducedSize = totalSize - (position - startPosition);
    int end = startPosition + totalSize - 1;

    if (it + 1 == ranges.end())
        size = reducedSize;
    else if ((it + 1)->start() < end)
        size = reducedSize - (end - (it + 1)->start()) - 1;
    else
        size = reducedSize;

    return size != 0;
}

void RangeMap::insert(unsigned position, unsigned size)
{
    Range range = Range::fromStartSize(position, size);
    if (ranges.isEmpty())
    {
        ranges.append(range);
        return;
    }

    /* Item with a position LESS THAN OR EQUAL TO the BEGINNING of the inserted range */
    QList<Range>::Iterator lower = qLowerBound(ranges.begin(), ranges.end(), range, rangeStartLess);
    if (!ranges.isEmpty() && (lower == ranges.end() || lower->start() > position))
        lower--;
    /* Item with a position GREATER THAN the END of the inserted range */
    Range r2 = Range::fromValue(range.end() + 1);
    QList<Range>::Iterator upper = qUpperBound(lower, ranges.end(), r2, rangeStartLess);

    Q_ASSERT(lower == ranges.end() || lower->start() <= position);

    /* Intersection at the beginning */
    if (lower != ranges.end() && position <= lower->end() + 1)
    {
        /* The beginning of the new range intersects the range in 'lower'.
         * Extend this range, and merge ranges after it as needed. */

        Q_ASSERT(lower->start() <= position);
        *lower = Range::fromStartEnd(lower->start(), qMax(range.end(), (upper-1)->end()));

        if ((upper-1) != lower)
            ranges.erase(lower+1, upper);
    }
    else if (lower != ranges.end() && range.end() <= (upper-1)->end())
    {
        /* The end of the new range intersects the range in 'upper'.
         * Extend this range. Merging is unnecessary, because any case
         * where that would be possible is included in the prior one. */

        upper--;
        *upper = Range::fromStartEnd(position, upper->end());
    }
    else
    {
        /* Create a new range item */
        ranges.insert(upper, range);
    }

#ifdef RANGEMAP_DEBUG
    qDebug() << "Rangemap insert" << position << size << ":" << *this;
#endif
#ifndef QT_NO_DEBUG
    debugTestConsistency();
#endif
}

#ifndef QT_NO_DEBUG
void RangeMap::debugTestConsistency()
{
    Range last = Range::fromValue(0);
    for (QList<Range>::Iterator it = ranges.begin(); it != ranges.end(); ++it)
    {
        const Range &current = *it;
        const bool isFirst = it == ranges.begin();

        /* End is after start; these are both inclusive, so they may be equal (in the case
         * of a range of one). */
        Q_ASSERT(current.start() <= current.end());

        if (!isFirst)
        {
            /* Sequentially ordered */
            Q_ASSERT(current.start() > last.end());
            /* There is a gap of at least one between ranges (otherwise, they would be the same range) */
            Q_ASSERT(current.start() - last.end() > 1);
        }

        last = current;
    }
}
#endif
