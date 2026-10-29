/*
    SPDX-FileCopyrightText: 2026 Nekto Oleg <27015@riseup.net>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COPYPARTS_H
#define COPYPARTS_H

#include <QList>
#include <QtGlobal>

#include <algorithm>

// Older DigitalOcean Spaces buckets accept at most 10 concurrent writes to one object.
constexpr qsizetype MaxPartsInFlight = 8;

constexpr qint64 CopyPartSize = 128LL * 1024 * 1024;

constexpr qint64 MultipartCopyThreshold = CopyPartSize;

inline bool needsMultipartCopy(qint64 size)
{
    return size > MultipartCopyThreshold;
}

// The most parts one multipart upload can hold.
constexpr qint64 MaxCopyPartCount = 10000;

// Both ends included, as in CopySourceRange.
struct CopyPart
{
    qint64 first = 0;
    qint64 last = 0;
};

inline qint64 copyPartSize(qint64 size)
{
    return std::max(CopyPartSize, (size + MaxCopyPartCount - 1) / MaxCopyPartCount);
}

// Cloudflare R2 requires every part except the last to have the same size.
inline QList<CopyPart> copyParts(qint64 size)
{
    const qint64 partSize = copyPartSize(size);
    QList<CopyPart> parts;
    parts.reserve((size + partSize - 1) / partSize);
    for (qint64 first = 0; first < size; first += partSize) {
        parts.append(CopyPart{first, std::min(first + partSize, size) - 1});
    }
    return parts;
}

#endif // COPYPARTS_H
