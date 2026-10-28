/*
    SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef S3URL_H
#define S3URL_H

#include <QDebug>
#include <QUrl>

#include <aws/core/utils/memory/stl/AWSString.h>

// One object across profiles and buckets. The key has no leading '/', as in a
// listing. A profile name can contain any character, so the parts are not
// joined into one string.
struct S3ObjectId
{
    QString profileName;
    QString bucketName;
    QString key;
};

bool operator==(const S3ObjectId &lhs, const S3ObjectId &rhs) noexcept;
size_t qHash(const S3ObjectId &id, size_t seed = 0) noexcept;

class S3Url
{
public:
    explicit S3Url(const QUrl &url);

    bool isRoot() const;
    bool isBucket() const;
    bool isKey() const;
    bool isProfileRoot() const;
    QString bucketName() const;
    QString profileName() const;
    QString key() const;
    QString prefix() const;
    QUrl url() const;
    S3ObjectId objectId() const;
    // The start-after value of a listing that begins at this key.
    QString startAfter() const;
    // Helpers to convert from QString to Aws::String
    // The uppercase naming is a trick to have a sort of overload-by-return-type and is consistent with the AWS sdk style (e.g. SetBucket() and SetKey()).
    Aws::String BucketName() const;
    Aws::String ProfileName() const;
    Aws::String Key() const;
    Aws::String Prefix() const;
    Aws::String StartAfter() const;

private:
    const QUrl m_url;
};

QDebug operator<<(QDebug debug, const S3Url &s3url);

#endif // S3URL_H
