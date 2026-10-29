/*
    SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef S3BACKEND_H
#define S3BACKEND_H

#include "s3url.h"

#include <KIO/Job>
#include <KIO/WorkerBase>

#include <QCache>
#include <QList>
#include <QPair>
#include <QUrl>

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

#include <aws/core/Aws.h>
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
    void clearObjectInfoCache();

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

    struct ObjectInfo
    {
        qint64 size = 0;
        Aws::String etag; // with its quotes, as the server returned it
    };

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
    Q_REQUIRED_RESULT KIO::WorkerResult multipartCopy(const Aws::S3::S3Client &client, const S3Url &s3src, const S3Url &s3dest);
    Aws::S3::Model::HeadObjectOutcome headObject(const Aws::S3::S3Client &client, const S3Url &s3url);
    QString contentType(const S3Url &s3url);

    void rememberObjectInfo(const S3ObjectId &id, qint64 size, const Aws::String &etag);
    std::optional<ObjectInfo> cachedObjectInfo(const S3Url &s3url) const;
    void fillObjectInfoAround(const Aws::S3::S3Client &client, const S3Url &s3url);

    Aws::S3::S3ClientConfiguration createClientConfiguration(const QString &profileName = QString()) const;
    Aws::S3::S3Client createS3Client(const QString &profileName, const Aws::S3::S3ClientConfiguration &config) const;

    const Aws::S3::S3ClientConfiguration &cachedClientConfiguration(const QString &profileName);
    std::shared_ptr<Aws::S3::S3Client> cachedS3Client(const QString &profileName);
    std::shared_ptr<Aws::S3::S3Client> cachedCopyClient(const QString &profileName);

    // Calls Aws::InitAPI() and Aws::ShutdownAPI(). As the first member it is
    // built first and destroyed last, after every client. Without
    // ShutdownAPI(), a destructor of libaws-cpp-sdk-core releases the default
    // client bootstrap while Qt unloads the plugin at exit, and the thread that
    // cleans up its event loops then frees memory through an allocator of the
    // already unloaded library.
    class AwsSdk
    {
    public:
        AwsSdk()
        {
            Aws::InitAPI(m_options);
        }
        ~AwsSdk()
        {
            Aws::ShutdownAPI(m_options);
        }

    private:
        Q_DISABLE_COPY(AwsSdk)
        Aws::SDKOptions m_options;
    };
    AwsSdk m_awsSdk;

    mutable std::mutex m_clientCacheMutex;
    std::unordered_map<QString, Aws::S3::S3ClientConfiguration> m_configCache;
    std::unordered_map<QString, std::shared_ptr<Aws::S3::S3Client>> m_clientCache;
    std::unordered_map<QString, std::shared_ptr<Aws::S3::S3Client>> m_copyClientCache;

    // CMD_COPY carries no size. s3.json sets no instance limits, so KIO runs
    // one kio_s3 process per application, and the process that listed a folder
    // is the one that copies from it. Only the command thread uses this cache,
    // so it has no lock.
    QCache<S3ObjectId, ObjectInfo> m_objectInfoCache;

    Aws::String m_configProfileName;    // This must be passed to the S3Client objects to get the proper region from ~/.aws/config
    Aws::String m_endpointOverride;
    S3Worker *q = nullptr;
};

#endif // S3BACKEND_H
