/*
    SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "s3url.h"

S3Url::S3Url(const QUrl &url)
    : m_url(url)
{}

bool S3Url::isRoot() const
{
    return bucketName().isEmpty() && key().isEmpty();
}

bool S3Url::isBucket() const
{
    return !bucketName().isEmpty() && key().isEmpty();
}

bool S3Url::isKey() const
{
    return !bucketName().isEmpty() && !key().isEmpty();
}

QString S3Url::bucketName() const
{
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

Aws::String S3Url::BucketName() const
{
    // Bucket names can consist only of lowercase letters, numbers, dots and hyphens, so Latin-1 is enough.
    const QByteArray bucket = bucketName().toLatin1();
    return Aws::String(bucket.constData(), bucket.size());
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

QDebug operator<<(QDebug debug, const S3Url &s3url)
{
    QDebugStateSaver stateSaver(debug);
    debug.nospace() << s3url.url().toDisplayString() << " (bucket: " << s3url.bucketName() << ", key: " << s3url.key() << ")";
    return debug.maybeSpace();
}

