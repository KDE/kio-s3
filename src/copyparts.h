/*
    SPDX-FileCopyrightText: 2026 Nekto Oleg <27015@riseup.net>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COPYPARTS_H
#define COPYPARTS_H

#include <QList>
#include <QtGlobal>

#include <algorithm>

// The most UploadPartCopy requests of one copy that run at once. DigitalOcean
// Spaces runs on Ceph, whose S3 gateway reads the range of a part copy 4 MiB at
// a time. Copying a 5 GiB object there one 128 MiB part at a time took 3.4
// times as long as CopyObject, and eight at a time less than half as long.
// Eight stays under the limit of older Spaces buckets, which accept at most 10
// concurrent PUT or COPY requests on one object.
constexpr qsizetype MaxPartsInFlight = 8;

// Each part is one UploadPartCopy. With up to eight parts in flight, copies in
// parts of 128 MiB were faster than in parts of 512 MiB at every size measured:
// 768 MiB to 5 GiB on Spaces, 1 and 2 GiB on Cloudflare R2. A 1 GiB object in
// parts of 512 MiB cannot use more than two of the eight.
constexpr qint64 CopyPartSize = 128LL * 1024 * 1024;

// Objects that need more than one part are copied in parts. The S3 API lets one
// CopyObject copy up to 5 GiB, and on Cloudflare R2 a 4 GiB CopyObject took
// 875 s. A multipart copy also shows progress after every part. A 256 MiB
// object in two parts took half the time of CopyObject on R2, 0.88 of it on
// RustFS and 1.5 times as long on DigitalOcean Spaces. A copy of one part took
// longer than CopyObject on Spaces and RustFS.
constexpr qint64 MultipartCopyThreshold = CopyPartSize;

inline bool needsMultipartCopy(qint64 size)
{
    return size > MultipartCopyThreshold;
}

// The most parts one multipart upload can hold.
constexpr qint64 MaxCopyPartCount = 10000;

// The byte range of one part, as CopySourceRange writes it: both ends included.
struct CopyPart
{
    qint64 first = 0;
    qint64 last = 0;
};

inline qint64 copyPartSize(qint64 size)
{
    return std::max(CopyPartSize, (size + MaxCopyPartCount - 1) / MaxCopyPartCount);
}

// Every part has the same size except the last one, as put() does for uploads:
// some servers, Cloudflare R2 among them, require it.
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
