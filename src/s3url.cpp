/*
    SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "s3url.h"

#include <QHashFunctions>

bool operator==(const S3ObjectId &lhs, const S3ObjectId &rhs) noexcept
{
    return lhs.profileName == rhs.profileName && lhs.bucketName == rhs.bucketName && lhs.key == rhs.key;
}

size_t qHash(const S3ObjectId &id, size_t seed) noexcept
{
    return qHashMulti(seed, id.profileName, id.bucketName, id.key);
}

S3Url::S3Url(const QUrl &url)
    : m_url(url)
{}

bool S3Url::isRoot() const
{
    return bucketName().isEmpty() && key().isEmpty() && profileName().isEmpty();
}

bool S3Url::isBucket() const
{
    return !bucketName().isEmpty() && key().isEmpty();
}

bool S3Url::isKey() const
{
    return !bucketName().isEmpty() && !key().isEmpty();
}

bool S3Url::isProfileRoot() const
{
    return m_url.userName().isEmpty() && !m_url.host().isEmpty() && m_url.authority().startsWith(QLatin1Char('@'));
}

QString S3Url::bucketName() const
{
    if (isProfileRoot()) {
        return QString();
    }
    if (m_url.userName().isEmpty()) {
        return m_url.host();
    }
    return m_url.userName();
}

QString S3Url::profileName() const
{
    if (m_url.authority().startsWith(QLatin1Char('@'))) {
        return m_url.host();
    }
    if (m_url.userName().isEmpty()) {
        return QString();
    }
    return m_url.host();
}

QString S3Url::key() const
{
    return m_url.path();
}

QString S3Url::prefix() const
{
    if (!isKey() || key() == QLatin1String("/")) {
        return QString();
    }

    QString prefix = key().mid(1);
    if (!prefix.endsWith(QLatin1Char('/'))) {
        prefix += QLatin1Char('/');
    }

    return prefix;
}

QUrl S3Url::url() const
{
    return m_url;
}

S3ObjectId S3Url::objectId() const
{
    return S3ObjectId{profileName(), bucketName(), key().mid(1)};
}

QString S3Url::startAfter() const
{
    // A listing returns the keys that sort after start-after by their UTF-8
    // bytes. A key without its last code point is a byte prefix of the key and
    // sorts before it, so the listing includes the key. A surrogate pair is
    // removed whole, because a lone surrogate has no UTF-8 encoding.
    QString value = key().mid(1);
    const qsizetype size = value.size();
    if (size >= 2 && value.at(size - 1).isLowSurrogate() && value.at(size - 2).isHighSurrogate()) {
        value.chop(2);
    } else {
        value.chop(1);
    }
    return value;
}

Aws::String S3Url::BucketName() const
{
    // Bucket names can consist only of lowercase letters, numbers, dots and hyphens, so Latin-1 is enough.
    const QByteArray bucket = bucketName().toLatin1();
    return Aws::String(bucket.constData(), bucket.size());
}

Aws::String S3Url::ProfileName() const
{
    const QByteArray profile = profileName().toUtf8();
    return Aws::String(profile.constData(), profile.size());
}

Aws::String S3Url::Key() const
{
    // The S3 object key name is a sequence of Unicode characters with UTF-8 encoding.
    const QByteArray _key = key().toUtf8();
    return Aws::String(_key.constData(), _key.size());
}

Aws::String S3Url::Prefix() const
{
    const QByteArray _prefix = prefix().toUtf8();
    return Aws::String(_prefix.constData(), _prefix.size());
}

Aws::String S3Url::StartAfter() const
{
    const QByteArray _startAfter = startAfter().toUtf8();
    return Aws::String(_startAfter.constData(), _startAfter.size());
}

QDebug operator<<(QDebug debug, const S3Url &s3url)
{
    QDebugStateSaver stateSaver(debug);
    debug.nospace() << s3url.url().toDisplayString() << " (bucket: " << s3url.bucketName() << ", key: " << s3url.key() << ", profile: " << s3url.profileName() << ")";
    return debug.maybeSpace();
}

