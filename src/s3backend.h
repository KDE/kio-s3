/*
    SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef S3BACKEND_H
#define S3BACKEND_H

#include "s3url.h"

#include <KIO/Job>
#include <KIO/WorkerBase>

#include <QUrl>

#include <aws/s3/S3Client.h>

class S3Worker;

class S3Backend
{
public:

    S3Backend(S3Worker *q);

    Q_REQUIRED_RESULT KIO::WorkerResult listDir(const QUrl &url);
    Q_REQUIRED_RESULT KIO::WorkerResult stat(const QUrl &url);
    Q_REQUIRED_RESULT KIO::WorkerResult mimetype(const QUrl &url);
    Q_REQUIRED_RESULT KIO::WorkerResult get(const QUrl &url);
    Q_REQUIRED_RESULT KIO::WorkerResult put(const QUrl &url, int permissions, KIO::JobFlags flags);
    Q_REQUIRED_RESULT KIO::WorkerResult copy(const QUrl &src, const QUrl &dest, int permissions, KIO::JobFlags flags);
    Q_REQUIRED_RESULT KIO::WorkerResult mkdir(const QUrl &url, int permissions);
    Q_REQUIRED_RESULT KIO::WorkerResult del(const QUrl &url, bool isFile);
    Q_REQUIRED_RESULT KIO::WorkerResult rename(const QUrl &src, const QUrl &dest, KIO::JobFlags flags);

private:
    Q_DISABLE_COPY(S3Backend)

    enum CwdAccess {
        ReadOnlyCwd,
        WritableCwd
    };

    void listBuckets();
    void listBucket(const Aws::String &bucketName);
    void listKey(const S3Url &s3url);
    void listCwdEntry(CwdAccess access = WritableCwd);
    bool deletePrefix(const Aws::S3::S3Client &client, const S3Url &s3url, const Aws::String &prefix);
    QString contentType(const S3Url &s3url);

    Aws::String m_configProfileName;    // This must be passed to the S3Client objects to get the proper region from ~/.aws/config
    S3Worker *q = nullptr;
};

#endif // S3BACKEND_H
