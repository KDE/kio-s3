/*
    SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef S3BACKEND_H
#define S3BACKEND_H

#include "s3url.h"

#include <KIO/Job>
#include <KIO/WorkerBase>

#include <QList>
#include <QPair>
#include <QUrl>

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <aws/s3/S3Client.h>
#include <aws/s3/S3ClientConfiguration.h>

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

    void invalidateClientCache(const QString &profileName = QString());

private:
    Q_DISABLE_COPY(S3Backend)

    enum CwdAccess {
        ReadOnlyCwd,
        WritableCwd
    };

    // Outcome of a batched DeleteObjects run: how many keys the service
    // confirmed as deleted, plus the keys it refused along with the reason.
    struct BatchDeleteResult
    {
        qint64 deletedCount = 0;
        QList<QPair<Aws::String, Aws::String>> failedKeys; // key, errorMessage

        bool success() const
        {
            return failedKeys.isEmpty();
        }
    };

    // Invoked after every completed batch with the running total of deleted keys.
    using BatchProgressCallback = std::function<void(qint64 cumulative)>;

    bool listBuckets(const S3Url &s3url);
    void listBucket(const S3Url &s3url);
    void listKey(const S3Url &s3url);
    void listCwdEntry(CwdAccess access = WritableCwd);
    BatchDeleteResult batchDelete(const Aws::S3::S3Client &client,
                                  const Aws::String &bucket,
                                  const QList<Aws::String> &keys,
                                  BatchProgressCallback progress = {});
    Q_REQUIRED_RESULT KIO::WorkerResult deletePrefix(const Aws::S3::S3Client &client, const S3Url &s3url);
    Q_REQUIRED_RESULT KIO::WorkerResult renamePrefix(const Aws::S3::S3Client &client, const S3Url &s3src, const S3Url &s3dest);
    QString contentType(const S3Url &s3url);

    Aws::S3::S3ClientConfiguration createClientConfiguration(const QString &profileName = QString()) const;
    Aws::S3::S3Client createS3Client(const QString &profileName = QString()) const;

    std::shared_ptr<Aws::S3::S3Client> cachedS3Client(const QString &profileName);

    mutable std::mutex m_clientCacheMutex;
    std::unordered_map<QString, std::shared_ptr<Aws::S3::S3Client>> m_clientCache;

    Aws::String m_configProfileName;    // This must be passed to the S3Client objects to get the proper region from ~/.aws/config
    Aws::String m_endpointOverride;
    S3Worker *q = nullptr;
};

#endif // S3BACKEND_H
