/*
    SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "s3backend.h"
#include "kio_s3.h"
#include "s3debug.h"

#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>

#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/config/ConfigAndCredentialsCacheManager.h>
#include <aws/core/Aws.h>
#include <aws/s3/model/Bucket.h>
#include <aws/s3/model/CopyObjectRequest.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/HeadObjectRequest.h>
#include <aws/s3/model/ListObjectsV2Request.h>
#include <aws/s3/model/AbortMultipartUploadRequest.h>
#include <aws/s3/model/CompleteMultipartUploadRequest.h>
#include <aws/s3/model/CompletedMultipartUpload.h>
#include <aws/s3/model/CompletedPart.h>
#include <aws/s3/model/CreateMultipartUploadRequest.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/UploadPartRequest.h>

#include <aws/s3/S3ClientConfiguration.h>

#include <array>

static KIO::WorkerResult invalidUrlError() {
    static const auto s_invalidUrlError = KIO::WorkerResult::fail(
        KIO::ERR_WORKER_DEFINED,
        xi18nc("@info", "Invalid S3 URI, bucket name is missing from the host.<nl/>A valid S3 URI must be written in the form: <link>s3://bucket/key</link>")
    );

    return s_invalidUrlError;
}

S3Backend::S3Backend(S3Worker *q)
    : q(q)
{
    Aws::SDKOptions options;
    Aws::InitAPI(options);

    m_configProfileName = Aws::Auth::GetConfigProfileName();
    qCDebug(S3) << "S3 backend initialized, config profile name:" << m_configProfileName.c_str();
    // Service-specific env var takes priority over global one.
    // See: https://docs.aws.amazon.com/sdkref/latest/guide/feature-ss-endpoints.html
    auto endpointUrl = qEnvironmentVariable("AWS_ENDPOINT_URL_S3");
    if (endpointUrl.isEmpty()) {
        endpointUrl = qEnvironmentVariable("AWS_ENDPOINT_URL");
    }
    if (!endpointUrl.isEmpty()) {
        m_endpointOverride = Aws::String(endpointUrl.toUtf8().constData(), endpointUrl.toUtf8().size());
        qCDebug(S3) << "Using custom endpoint:" << endpointUrl;
    }
    // Fallback to endpoint_url from ~/.aws/config profile.
    if (m_endpointOverride.empty()) {
        const auto profileEndpoint = Aws::Config::GetCachedConfigValue(m_configProfileName, "endpoint_url");
        if (!profileEndpoint.empty()) {
            m_endpointOverride = profileEndpoint;
            qCDebug(S3) << "Using endpoint from config profile:" << profileEndpoint.c_str();
        }
    }
}

Aws::S3::S3ClientConfiguration S3Backend::createClientConfiguration(const QString &profileName) const
{
    if (!profileName.isEmpty()) {
        KConfig kioConfig(QStringLiteral("kio_s3rc"), KConfig::SimpleConfig);
        KConfigGroup profileGroup = kioConfig.group(QStringLiteral("Profile-%1").arg(profileName));

        Aws::S3::S3ClientConfiguration config;
        const auto endpointUrl = profileGroup.readEntry("EndpointUrl", QString());
        if (!endpointUrl.isEmpty()) {
            config.endpointOverride = Aws::String(endpointUrl.toUtf8().constData(), endpointUrl.toUtf8().size());
        }
        const auto region = profileGroup.readEntry("Region", QString());
        if (!region.isEmpty()) {
            config.region = Aws::String(region.toLatin1().constData(), region.toLatin1().size());
        }
        if (profileGroup.readEntry("PathStyle", false)) {
            config.useVirtualAddressing = false;
        }
        return config;
    }

    Aws::S3::S3ClientConfiguration config(m_configProfileName.c_str());
    if (!m_endpointOverride.empty()) {
        config.endpointOverride = m_endpointOverride;
        if (m_endpointOverride.find("amazonaws.com") == Aws::String::npos) {
            config.useVirtualAddressing = false;
        }
        const auto envRegion = qEnvironmentVariable("AWS_DEFAULT_REGION");
        if (!envRegion.isEmpty()) {
            config.region = Aws::String(envRegion.toUtf8().constData(), envRegion.toUtf8().size());
        }
    }
    return config;
}

Aws::S3::S3Client S3Backend::createS3Client(const QString &profileName) const
{
    const auto config = createClientConfiguration(profileName);
    if (!profileName.isEmpty()) {
        KConfig kioConfig(QStringLiteral("kio_s3rc"), KConfig::SimpleConfig);
        KConfigGroup profileGroup = kioConfig.group(QStringLiteral("Profile-%1").arg(profileName));
        const auto awsProfile = profileGroup.readEntry("AwsProfile", QString());
        if (!awsProfile.isEmpty()) {
            const auto creds = Aws::MakeShared<Aws::Auth::ProfileConfigFileAWSCredentialsProvider>("kio-s3", awsProfile.toStdString().c_str());
            return Aws::S3::S3Client(creds, nullptr, config);
        }
    }
    return Aws::S3::S3Client(config);
}

KIO::WorkerResult S3Backend::listDir(const QUrl &url)
{
    const auto s3url = S3Url(url);
    qCDebug(S3) << "Going to list" << s3url;

    if (s3url.isProfileRoot()) {
        const bool hasBuckets = listBuckets(s3url);
        listCwdEntry(ReadOnlyCwd);
        if (hasBuckets) {
            return KIO::WorkerResult::pass();
        }
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, xi18nc("@info", "Could not find S3 buckets, please check your AWS configuration."));
    }

    if (s3url.isRoot()) {
        const bool hasBuckets = listBuckets(s3url);
        listCwdEntry(ReadOnlyCwd);
        if (hasBuckets) {
            return KIO::WorkerResult::pass();
        }
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, xi18nc("@info", "Could not find S3 buckets, please check your AWS configuration."));
    }

    if (s3url.isBucket()) {
        listBucket(s3url);
        listCwdEntry();
        return KIO::WorkerResult::pass();
    }

    if (!s3url.isKey()) {
        qCDebug(S3) << "Could not list invalid S3 url:" << url;
        return invalidUrlError();
    }

    Q_ASSERT(s3url.isKey());

    listKey(s3url);
    listCwdEntry();
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult S3Backend::stat(const QUrl &url)
{
    const auto s3url = S3Url(url);
    qCDebug(S3) << "Going to stat()" << s3url;

    if (s3url.isRoot() || s3url.isProfileRoot()) {
        return KIO::WorkerResult::pass();
    }

    if (s3url.isBucket()) {
        KIO::UDSEntry entry;
        entry.reserve(4);
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, s3url.bucketName());
        entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
        entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH);
        entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
        q->statEntry(entry);
        return KIO::WorkerResult::pass();
    }

    if (!s3url.isKey()) {
        qCDebug(S3) << "Could not stat invalid S3 url:" << url;
        return invalidUrlError();
    }

    Q_ASSERT(s3url.isKey());

    // Try to do an HEAD request for the key.
    // If the URL is a folder, S3 will reply only if there is a 0-sized object with that key.
    const auto pathComponents = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    // The URL could be s3://<bucketName>/ which would have "/" as path(). Fallback to bucketName in that case.
    const bool isRootKey = pathComponents.isEmpty();
    const auto fileName = isRootKey ? s3url.bucketName() : pathComponents.last();

    const Aws::S3::S3Client client = createS3Client(s3url.profileName());

    Aws::S3::Model::HeadObjectRequest headObjectRequest;
    headObjectRequest.SetBucket(s3url.BucketName());
    headObjectRequest.SetKey(s3url.Key());

    auto headObjectRequestOutcome = client.HeadObject(headObjectRequest);
    if (!isRootKey && headObjectRequestOutcome.IsSuccess()) {
        Aws::String contentType = headObjectRequestOutcome.GetResult().GetContentType();
        // This is set by S3 when creating a 0-sized folder from the AWS console. Use the freedesktop mimetype instead.
        if (contentType == "application/x-directory") {
            contentType = "inode/directory";
        }
        const bool isDir = contentType == "inode/directory";
        KIO::UDSEntry entry;
        entry.reserve(7);
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, fileName);
        entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, fileName);
        entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QString::fromLatin1(contentType.c_str(), contentType.size()));
        entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, isDir ? S_IFDIR : S_IFREG);
        entry.fastInsert(KIO::UDSEntry::UDS_SIZE, headObjectRequestOutcome.GetResult().GetContentLength());
        if (isDir) {
            entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH);
        } else {
            // For keys we would need another request (GetObjectAclRequest) to get the permission,
            // but it is kind of pointless to map the AWS ACL model to UNIX permission anyway.
            // So assume keys are always writable, we'll handle the failure if they are not.
            // The same logic will be applied to all the other UDS_ACCESS instances for keys.
            entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH );
            const auto lastModifiedTime = headObjectRequestOutcome.GetResult().GetLastModified();
            entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, lastModifiedTime.SecondsWithMSPrecision());
        }

        q->statEntry(entry);
    } else {
        if (!isRootKey) {
            // S3 returns HTTP 404 for a non-existent key, but HEAD responses carry no body,
            // so the AWS SDK cannot parse the XML error message. It leaves GetResponseCode()
            // at its default sentinel (-1 = REQUEST_NOT_MADE) and sets the message to
            // "No response body". We identify this SDK-specific pattern and treat it as a
            // normal 404. All other errors — 401 (bad credentials), 403 (access denied),
            // 405 (delete marker in versioned bucket), 5xx (server errors), or genuine
            // network failures (-1 with a different message) — fall through to the folder
            // assumption below, because we cannot conclude the key does not exist.
            // Keys ending with '/' are S3 virtual folder prefixes — also keep the assumption.
            const auto &headError = headObjectRequestOutcome.GetError();
            const auto httpCode = static_cast<int>(headError.GetResponseCode());
            const bool isSdkHead404 = (httpCode == -1
                && headError.GetMessage() == Aws::String("No response body"));
            const bool isHttp404 = (httpCode == 404);
            if ((isSdkHead404 || isHttp404) && !s3url.key().endsWith(QLatin1Char('/'))) {
                return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
            }
            qCDebug(S3).nospace() << "Could not get HEAD object for key: " << s3url.key() << " - " << headError.GetMessage().c_str() << " (httpCode: " << httpCode << ") - assuming it's a folder.";
        }
        // HACK: assume this is a folder (i.e. a virtual key without associated object).
        // If it were a key or a 0-sized folder the HEAD request would likely have worked.
        // This is needed to upload local folders to S3.
        KIO::UDSEntry entry;
        entry.reserve(6);
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, fileName);
        entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, fileName);
        entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
        entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
        entry.fastInsert(KIO::UDSEntry::UDS_SIZE, 0);
        entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH);
        q->statEntry(entry);
    }

    return KIO::WorkerResult::pass();
}

KIO::WorkerResult S3Backend::mimetype(const QUrl &url)
{
    const auto s3url = S3Url(url);
    qCDebug(S3) << "Going to get mimetype for" << s3url;

    q->mimeType(contentType(s3url));
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult S3Backend::get(const QUrl &url)
{
    const auto s3url = S3Url(url);
    qCDebug(S3) << "Going to get" << s3url;

    const Aws::S3::S3Client client = createS3Client(s3url.profileName());

    Aws::S3::Model::GetObjectRequest objectRequest;
    objectRequest.SetBucket(s3url.BucketName());
    objectRequest.SetKey(s3url.Key());

    auto getObjectOutcome = client.GetObject(objectRequest);
    if (!getObjectOutcome.IsSuccess()) {
        qCWarning(S3) << "Could not get object with key:" << s3url.key() << " - " << getObjectOutcome.GetError().GetMessage().c_str();
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_READ, url.toDisplayString());
    }

    auto objectResult = getObjectOutcome.GetResultWithOwnership();

    // Emit MIME type from the GetObject response directly (no extra HEAD request needed).
    const auto ct = objectResult.GetContentType();
    q->mimeType(QString::fromLatin1(ct.c_str(), ct.size()));

    qCDebug(S3) << "Key" << s3url.key() << "has Content-Length:" << objectResult.GetContentLength();
    q->totalSize(objectResult.GetContentLength());

    auto& retrievedFile = objectResult.GetBody();
    std::array<char, 1024*1024> buffer{};
    while (!retrievedFile.eof()) {
        const auto readBytes = retrievedFile.read(buffer.data(), buffer.size()).gcount();
        if (readBytes > 0) {
            q->data(QByteArray(buffer.data(), readBytes));
        }
    }

    q->data(QByteArray());

    return KIO::WorkerResult::pass();
}

KIO::WorkerResult S3Backend::put(const QUrl &url, int permissions, KIO::JobFlags flags)
{
    Q_UNUSED(permissions)
    Q_UNUSED(flags)

    static constexpr qint64 MultipartThreshold = 8 * 1024 * 1024;
    static constexpr qint64 MultipartChunkSize  = 8 * 1024 * 1024;

    const auto s3url = S3Url(url);
    qCDebug(S3) << "Going to upload data to" << s3url;

    const Aws::S3::S3Client client = createS3Client(s3url.profileName());
    const Aws::String bucket = s3url.BucketName();
    const Aws::String key = s3url.Key();

    // Read data from KIO, accumulating into partBuffer.
    // Once partBuffer reaches MultipartThreshold we switch to multipart upload.
    QByteArray partBuffer;
    partBuffer.reserve(MultipartChunkSize);

    Aws::String uploadId;
    Aws::Vector<Aws::S3::Model::CompletedPart> completedParts;
    int partNumber = 1;
    KIO::filesize_t processedBytes = 0;

    // Helper: abort an in-progress multipart upload and return an error.
    auto abortAndFail = [&]() -> KIO::WorkerResult {
        Aws::S3::Model::AbortMultipartUploadRequest abortReq;
        abortReq.SetBucket(bucket);
        abortReq.SetKey(key);
        abortReq.SetUploadId(uploadId);
        client.AbortMultipartUpload(abortReq);
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_WRITE, url.toDisplayString());
    };

    // Helper: upload partBuffer as one multipart part.
    auto uploadPart = [&]() -> bool {
        const auto stream = std::make_shared<Aws::StringStream>();
        stream->write(partBuffer.data(), partBuffer.size());

        Aws::S3::Model::UploadPartRequest partReq;
        partReq.SetBucket(bucket);
        partReq.SetKey(key);
        partReq.SetUploadId(uploadId);
        partReq.SetPartNumber(partNumber);
        partReq.SetContentLength(partBuffer.size());
        partReq.SetBody(stream);

        const auto outcome = client.UploadPart(partReq);
        if (!outcome.IsSuccess()) {
            qCWarning(S3) << "UploadPart" << partNumber << "failed:"
                          << outcome.GetError().GetMessage().c_str();
            return false;
        }

        Aws::S3::Model::CompletedPart completed;
        completed.SetPartNumber(partNumber);
        completed.SetETag(outcome.GetResult().GetETag());
        completedParts.push_back(std::move(completed));

        processedBytes += partBuffer.size();
        q->processedSize(processedBytes);
        partBuffer.clear();
        ++partNumber;
        return true;
    };

    int n;
    do {
        QByteArray chunk;
        q->dataReq();
        n = q->readData(chunk);
        if (!chunk.isEmpty()) {
            partBuffer.append(chunk);
        }

        // If buffer reached chunk size and multipart is already started, flush it.
        if (!uploadId.empty() && partBuffer.size() >= MultipartChunkSize) {
            if (!uploadPart()) {
                return abortAndFail();
            }
        }

        // If buffer reached threshold and multipart not yet started, initiate it.
        if (uploadId.empty() && partBuffer.size() >= MultipartThreshold) {
            Aws::S3::Model::CreateMultipartUploadRequest createReq;
            createReq.SetBucket(bucket);
            createReq.SetKey(key);
            const auto createOutcome = client.CreateMultipartUpload(createReq);
            if (!createOutcome.IsSuccess()) {
                qCWarning(S3) << "CreateMultipartUpload failed:"
                              << createOutcome.GetError().GetMessage().c_str();
                return KIO::WorkerResult::fail(KIO::ERR_CANNOT_WRITE, url.toDisplayString());
            }
            uploadId = createOutcome.GetResult().GetUploadId();
            qCDebug(S3) << "Started multipart upload, uploadId:" << uploadId.c_str();

            if (!uploadPart()) {
                return abortAndFail();
            }
        }
    } while (n > 0);

    if (n < 0) {
        qCWarning(S3) << "Failed to read data for upload to" << s3url;
        if (!uploadId.empty()) {
            return abortAndFail();
        }
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_WRITE, url.toDisplayString());
    }

    // Small file path: never reached MultipartThreshold, use simple PutObject.
    if (uploadId.empty()) {
        const auto stream = std::make_shared<Aws::StringStream>();
        stream->write(partBuffer.data(), partBuffer.size());

        Aws::S3::Model::PutObjectRequest request;
        request.SetBucket(bucket);
        request.SetKey(key);
        request.SetBody(stream);

        const auto outcome = client.PutObject(request);
        if (!outcome.IsSuccess()) {
            qCWarning(S3) << "Could not PUT object with key:" << s3url.key()
                          << "-" << outcome.GetError().GetMessage().c_str();
            return KIO::WorkerResult::fail(KIO::ERR_CANNOT_WRITE, url.toDisplayString());
        }

        qCDebug(S3) << "Uploaded" << partBuffer.size() << "bytes to key:" << s3url.key();
        return KIO::WorkerResult::pass();
    }

    // Large file path: upload remaining bytes as final part (may be smaller than chunk size).
    if (!partBuffer.isEmpty()) {
        if (!uploadPart()) {
            return abortAndFail();
        }
    }

    // Complete the multipart upload.
    Aws::S3::Model::CompletedMultipartUpload completedUpload;
    for (auto &part : completedParts) {
        completedUpload.AddParts(part);
    }

    Aws::S3::Model::CompleteMultipartUploadRequest completeReq;
    completeReq.SetBucket(bucket);
    completeReq.SetKey(key);
    completeReq.SetUploadId(uploadId);
    completeReq.SetMultipartUpload(completedUpload);

    const auto completeOutcome = client.CompleteMultipartUpload(completeReq);
    if (!completeOutcome.IsSuccess()) {
        qCWarning(S3) << "CompleteMultipartUpload failed:"
                      << completeOutcome.GetError().GetMessage().c_str();
        return abortAndFail();
    }

    qCDebug(S3) << "Uploaded" << processedBytes << "bytes in" << (partNumber - 1)
                << "parts to key:" << s3url.key();
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult S3Backend::copy(const QUrl &src, const QUrl &dest, int permissions, KIO::JobFlags flags)
{
    Q_UNUSED(permissions)
    Q_UNUSED(flags)

    const auto s3src = S3Url(src);
    const auto s3dest = S3Url(dest);
    qCDebug(S3) << "Going to copy" << s3src << "to" << s3dest;

    if (src == dest) {
        return KIO::WorkerResult::fail(KIO::ERR_FILE_ALREADY_EXIST, QString());
    }

    if (s3src.profileName() != s3dest.profileName()) {
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED,
            xi18nc("@info", "Cannot copy between different S3 profiles. Copy the file locally first."));
    }

    if (s3src.isRoot() || s3src.isBucket()) {
        qCDebug(S3) << "Cannot copy from root or bucket url:" << src;
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_OPEN_FOR_READING, src.toDisplayString());
    }

    if (!s3src.isKey()) {
        qCDebug(S3) << "Cannot copy from invalid S3 url:" << src;
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_OPEN_FOR_READING, src.toDisplayString());
    }

    // TODO: can we copy to isBucket() urls?
    if (s3dest.isRoot() || s3dest.isBucket()) {
        qCDebug(S3) << "Cannot copy to root or bucket url:" << dest;
        return KIO::WorkerResult::fail(KIO::ERR_WRITE_ACCESS_DENIED, dest.toDisplayString());
    }

    if (!s3dest.isKey()) {
        qCDebug(S3) << "Cannot write to invalid S3 url:" << dest;
        return KIO::WorkerResult::fail(KIO::ERR_WRITE_ACCESS_DENIED, dest.toDisplayString());
    }

    const Aws::S3::S3Client client = createS3Client(s3src.profileName());

    // Check if destination key already exists, otherwise S3 will overwrite it leading to data loss.
    Aws::S3::Model::HeadObjectRequest headObjectRequest;
    headObjectRequest.SetBucket(s3dest.BucketName());
    headObjectRequest.SetKey(s3dest.Key());
    auto headObjectRequestOutcome = client.HeadObject(headObjectRequest);
    if (headObjectRequestOutcome.IsSuccess()) {
        return KIO::WorkerResult::fail(KIO::ERR_FILE_ALREADY_EXIST, QString());
    }

    Aws::S3::Model::CopyObjectRequest request;
    request.SetCopySource(s3src.BucketName() + "/" + s3src.Key());
    request.SetBucket(s3dest.BucketName());
    request.SetKey(s3dest.Key());

    auto copyObjectOutcome = client.CopyObject(request);
    if (!copyObjectOutcome.IsSuccess()) {
        qCDebug(S3) << "Could not copy" << src << "to" << dest << "- " << copyObjectOutcome.GetError().GetMessage().c_str();
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, xi18nc("@info", "Could not copy <link>%1</link> to <link>%2</link>", src.toDisplayString(), dest.toDisplayString()));
    }

    return KIO::WorkerResult::pass();
}

KIO::WorkerResult S3Backend::mkdir(const QUrl &url, int permissions)
{
    Q_UNUSED(url)
    Q_UNUSED(permissions)
    qCDebug(S3) << "Pretending creation of folder" << url;
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult S3Backend::del(const QUrl &url, bool isFile)
{
    Q_UNUSED(isFile)
    const auto s3url = S3Url(url);
    qCDebug(S3) << "Going to delete" << s3url;

    if (s3url.isRoot() || s3url.isBucket()) {
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_DELETE, url.toDisplayString());
    }

    if (!s3url.isKey()) {
        return invalidUrlError();
    }

    const Aws::S3::S3Client client = createS3Client(s3url.profileName());

    if (deletePrefix(client, s3url)) {
        return KIO::WorkerResult::pass();
    } else {
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_DELETE, url.toDisplayString());
    }
}

KIO::WorkerResult S3Backend::rename(const QUrl &src, const QUrl &dest, KIO::JobFlags flags)
{
    const auto s3src = S3Url(src);
    const auto s3dest = S3Url(dest);
    qCDebug(S3) << "Going to rename" << s3src << "to" << s3dest;

    if (s3src.profileName() != s3dest.profileName()) {
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED,
            xi18nc("@info", "Cannot rename between different S3 profiles."));
    }

    if (!s3src.isKey() || !s3dest.isKey()) {
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_RENAME, src.toDisplayString());
    }

    const Aws::S3::S3Client client = createS3Client(s3src.profileName());

    // Check if source is a folder by probing for objects under its prefix.
    Aws::S3::Model::ListObjectsV2Request probeRequest;
    probeRequest.SetBucket(s3src.BucketName());
    probeRequest.SetPrefix(s3src.Prefix());
    probeRequest.SetMaxKeys(1);

    const auto probeOutcome = client.ListObjectsV2(probeRequest);
    if (!probeOutcome.IsSuccess()) {
        qCDebug(S3) << "Could not probe source prefix, assuming single file:" << probeOutcome.GetError().GetMessage().c_str();
    } else if (!probeOutcome.GetResult().GetContents().empty()) {
        return renamePrefix(client, s3src, s3dest);
    }

    // Single file: copy + delete.
    const auto copyResult = copy(src, dest, -1, flags);
    if (!copyResult.success()) {
        qCDebug(S3).nospace() << "Could not copy " << src << " to " << dest << ", aborting rename()";
        if (copyResult.error() == KIO::ERR_FILE_ALREADY_EXIST) {
            return copyResult;
        }
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_RENAME, src.toDisplayString());
    }

    const auto delResult = del(src, false);
    if (!delResult.success()) {
        qCDebug(S3) << "Could not delete" << src << "after it was copied to" << dest;
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_RENAME, src.toDisplayString());
    }

    return KIO::WorkerResult::pass();
}

KIO::WorkerResult S3Backend::renamePrefix(const Aws::S3::S3Client &client, const S3Url &s3src, const S3Url &s3dest)
{
    const Aws::String srcBucket = s3src.BucketName();
    const Aws::String destBucket = s3dest.BucketName();
    const Aws::String srcPrefix = s3src.Prefix();
    const Aws::String destPrefix = s3dest.Prefix();
    qCDebug(S3) << "Renaming folder:" << srcPrefix.c_str() << "to" << destPrefix.c_str();

    // Phase 1: List all objects under the source prefix and copy each to the destination.
    // Track both source and destination keys so we can delete sources after a successful
    // copy, or rollback destination keys if the copy fails partway.
    QList<Aws::String> srcKeys;
    QList<Aws::String> destKeys;

    auto rollback = [&client, &destBucket, &destKeys]() {
        for (const auto &key : destKeys) {
            Aws::S3::Model::DeleteObjectRequest delReq;
            delReq.SetBucket(destBucket);
            delReq.SetKey(key);
            const auto delOutcome = client.DeleteObject(delReq);
            if (!delOutcome.IsSuccess()) {
                qCWarning(S3) << "Rollback: could not delete" << key.c_str()
                              << "-" << delOutcome.GetError().GetMessage().c_str();
            }
        }
    };

    Aws::S3::Model::ListObjectsV2Request listRequest;
    listRequest.SetBucket(srcBucket);
    listRequest.SetPrefix(srcPrefix);

    bool isTruncated = false;
    do {
        const auto listOutcome = client.ListObjectsV2(listRequest);
        if (!listOutcome.IsSuccess()) {
            qCWarning(S3) << "Could not list source prefix:" << srcPrefix.c_str()
                          << "-" << listOutcome.GetError().GetMessage().c_str();
            rollback();
            return KIO::WorkerResult::fail(KIO::ERR_CANNOT_RENAME, s3src.url().toDisplayString());
        }

        const auto &result = listOutcome.GetResult();
        for (const auto &object : result.GetContents()) {
            const Aws::String &srcKey = object.GetKey();
            const Aws::String destKey = destPrefix + srcKey.substr(srcPrefix.size());

            Aws::S3::Model::CopyObjectRequest copyRequest;
            copyRequest.SetCopySource(srcBucket + "/" + srcKey);
            copyRequest.SetBucket(destBucket);
            copyRequest.SetKey(destKey);

            const auto copyOutcome = client.CopyObject(copyRequest);
            if (!copyOutcome.IsSuccess()) {
                qCWarning(S3) << "Could not copy object:" << srcKey.c_str()
                              << "to" << destKey.c_str()
                              << "-" << copyOutcome.GetError().GetMessage().c_str();
                rollback();
                return KIO::WorkerResult::fail(KIO::ERR_CANNOT_RENAME, s3src.url().toDisplayString());
            }

            srcKeys.append(srcKey);
            destKeys.append(destKey);
        }

        isTruncated = result.GetIsTruncated();
        if (isTruncated) {
            listRequest.SetContinuationToken(result.GetNextContinuationToken());
        }
    } while (isTruncated);

    qCDebug(S3) << "Copied" << srcKeys.size() << "objects, now deleting source objects...";

    // Phase 2: Delete all source objects. If deletion fails, the data is safe
    // at the destination — warn the user but still report success.
    bool allDeleted = true;
    for (const auto &key : srcKeys) {
        Aws::S3::Model::DeleteObjectRequest delRequest;
        delRequest.SetBucket(srcBucket);
        delRequest.SetKey(key);
        const auto delOutcome = client.DeleteObject(delRequest);
        if (!delOutcome.IsSuccess()) {
            qCWarning(S3) << "Could not delete source object:" << key.c_str()
                          << "-" << delOutcome.GetError().GetMessage().c_str();
            allDeleted = false;
        }
    }

    if (!allDeleted) {
        q->warning(i18nc("@info", "Some source files could not be deleted after renaming. The renamed folder may still contain leftover files."));
    }

    qCDebug(S3) << "Folder rename complete:" << srcKeys.size() << "objects moved";
    return KIO::WorkerResult::pass();
}

bool S3Backend::listBuckets(const S3Url &s3url)
{
    const Aws::S3::S3Client client = createS3Client(s3url.profileName());
    const auto listBucketsOutcome = client.ListBuckets();
    bool hasBuckets = false;

    if (listBucketsOutcome.IsSuccess()) {
        const auto buckets = listBucketsOutcome.GetResult().GetBuckets();
        hasBuckets = !buckets.empty();
        for (const auto &bucket : buckets) {
            const auto bucketName = QString::fromLatin1(bucket.GetName().c_str(), bucket.GetName().size());
            qCDebug(S3) << "Found bucket:" << bucketName;
            KIO::UDSEntry entry;
            entry.reserve(7);
            entry.fastInsert(KIO::UDSEntry::UDS_NAME, bucketName);
            entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, bucketName);
            const auto profile = s3url.profileName();
            if (profile.isEmpty()) {
                entry.fastInsert(KIO::UDSEntry::UDS_URL, QStringLiteral("s3://%1/").arg(bucketName));
            } else {
                entry.fastInsert(KIO::UDSEntry::UDS_URL, QStringLiteral("s3://%1@%2/").arg(bucketName, profile));
            }
            entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
            entry.fastInsert(KIO::UDSEntry::UDS_SIZE, 0);
            entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH);
            entry.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, QStringLiteral("folder-network"));
            q->listEntry(entry);
        }
    } else {
        qCDebug(S3) << "Could not list buckets:" << listBucketsOutcome.GetError().GetMessage().c_str();
    }

    return hasBuckets;
}

void S3Backend::listBucket(const S3Url &s3url)
{
    const Aws::S3::S3Client client = createS3Client(s3url.profileName());
    const Aws::String bucketName = s3url.BucketName();
    const auto bucket = QString::fromLatin1(bucketName.c_str(), bucketName.size());
    const auto profile = s3url.profileName();
    const auto authority = profile.isEmpty() ? bucket : QStringLiteral("%1@%2").arg(bucket, profile);

    Aws::S3::Model::ListObjectsV2Request request;
    request.SetBucket(bucketName);
    request.SetDelimiter("/");

    qCDebug(S3) << "Listing objects in bucket" << bucketName.c_str() << "...";

    bool isTruncated = false;
    do {
        const auto outcome = client.ListObjectsV2(request);
        if (!outcome.IsSuccess()) {
            qCWarning(S3) << "Could not list bucket:" << outcome.GetError().GetMessage().c_str();
            if (isTruncated) {
                q->warning(i18nc("@info", "Could not retrieve the complete file listing. Some files may not be shown."));
            }
            break;
        }

        const auto &result = outcome.GetResult();

        for (const auto &object : result.GetContents()) {
            KIO::UDSEntry entry;
            const auto objectKey = QString::fromUtf8(object.GetKey().c_str());
            entry.reserve(6);
            entry.fastInsert(KIO::UDSEntry::UDS_NAME, objectKey);
            entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, objectKey);
            entry.fastInsert(KIO::UDSEntry::UDS_URL, QStringLiteral("s3://%1/%2").arg(authority, objectKey));
            entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
            entry.fastInsert(KIO::UDSEntry::UDS_SIZE, object.GetSize());
            entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH );
            q->listEntry(entry);
        }

        for (const auto &commonPrefix : result.GetCommonPrefixes()) {
            KIO::UDSEntry entry;
            QString prefix = QString::fromUtf8(commonPrefix.GetPrefix().c_str(), commonPrefix.GetPrefix().size());
            if (prefix.endsWith(QLatin1Char('/'))) {
                prefix.chop(1);
            }
            entry.reserve(6);
            entry.fastInsert(KIO::UDSEntry::UDS_NAME, prefix);
            entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, prefix);
            entry.fastInsert(KIO::UDSEntry::UDS_URL, QStringLiteral("s3://%1/%2/").arg(authority, prefix));
            entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
            entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH);
            entry.fastInsert(KIO::UDSEntry::UDS_SIZE, 0);
            q->listEntry(entry);
        }

        isTruncated = result.GetIsTruncated();
        if (isTruncated) {
            request.SetContinuationToken(result.GetNextContinuationToken());
        }
    } while (isTruncated);
}

void S3Backend::listKey(const S3Url &s3url)
{
    const Aws::S3::S3Client client = createS3Client(s3url.profileName());
    const QString prefix = s3url.prefix();

    Aws::S3::Model::ListObjectsV2Request request;
    request.SetBucket(s3url.BucketName());
    request.SetDelimiter("/");
    request.SetPrefix(s3url.Prefix());

    qCDebug(S3) << "Listing prefix" << prefix << "...";

    bool isTruncated = false;
    do {
        const auto outcome = client.ListObjectsV2(request);
        if (!outcome.IsSuccess()) {
            qCWarning(S3) << "Could not list prefix" << s3url.key() << "-" << outcome.GetError().GetMessage().c_str();
            if (isTruncated) {
                q->warning(i18nc("@info", "Could not retrieve the complete file listing. Some files may not be shown."));
            }
            break;
        }

        const auto &result = outcome.GetResult();
        qCDebug(S3) << "Prefix" << prefix << "has" << result.GetContents().size() << "objects";

        for (const auto &object : result.GetContents()) {
            QString key = QString::fromUtf8(object.GetKey().c_str(), object.GetKey().size());
            // Note: key might be empty. 0-sized virtual folders have object.GetKey() equal to prefix.
            key.remove(0, prefix.length());

            KIO::UDSEntry entry;
            // S3 always appends trailing slash to "folder" objects.
            if (key.endsWith(QLatin1Char('/'))) {
                entry.reserve(5);
                entry.fastInsert(KIO::UDSEntry::UDS_NAME, key);
                entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, key);
                entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
                entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH);
                entry.fastInsert(KIO::UDSEntry::UDS_SIZE, 0);
                q->listEntry(entry);
            } else if (!key.isEmpty()) { // Not a folder.
                entry.reserve(6);
                entry.fastInsert(KIO::UDSEntry::UDS_NAME, key);
                entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, key);
                entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
                entry.fastInsert(KIO::UDSEntry::UDS_SIZE, object.GetSize());
                entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH );
                entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, object.GetLastModified().SecondsWithMSPrecision());
                q->listEntry(entry);
            }
        }

        qCDebug(S3) << "Prefix" << prefix << "has" << result.GetCommonPrefixes().size() << "common prefixes";
        for (const auto &commonPrefix : result.GetCommonPrefixes()) {
            QString subprefix = QString::fromUtf8(commonPrefix.GetPrefix().c_str(), commonPrefix.GetPrefix().size());
            if (subprefix.endsWith(QLatin1Char('/'))) {
                subprefix.chop(1);
            }
            if (subprefix.startsWith(prefix)) {
                subprefix.remove(0, prefix.length());
            }
            KIO::UDSEntry entry;
            entry.reserve(4);
            entry.fastInsert(KIO::UDSEntry::UDS_NAME, subprefix);
            entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, subprefix);
            entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
            entry.fastInsert(KIO::UDSEntry::UDS_SIZE, 0);
            q->listEntry(entry);
        }

        isTruncated = result.GetIsTruncated();
        if (isTruncated) {
            request.SetContinuationToken(result.GetNextContinuationToken());
        }
    } while (isTruncated);
}

void S3Backend::listCwdEntry(CwdAccess access)
{
    // List UDSEntry for "."
    KIO::UDSEntry entry;
    entry.reserve(4);
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.fastInsert(KIO::UDSEntry::UDS_SIZE, 0);
    if (access == ReadOnlyCwd) {
        entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    } else {
        entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH);
    }

    q->listEntry(entry);
}

bool S3Backend::deletePrefix(const Aws::S3::S3Client &client, const S3Url &s3url)
{
    const Aws::String prefix = s3url.Prefix();
    const Aws::String bucketName = s3url.BucketName();
    qCDebug(S3) << "Going to delete all objects under prefix:" << prefix.c_str();

    // Flat listing (no delimiter) returns all objects under the prefix,
    // including objects in nested subfolders. This avoids recursion and
    // handles folders with more than 1000 objects via pagination.
    Aws::S3::Model::ListObjectsV2Request listRequest;
    listRequest.SetBucket(bucketName);
    listRequest.SetPrefix(prefix);

    bool allDeleted = true;
    int totalDeleted = 0;
    bool isTruncated = false;
    do {
        const auto listOutcome = client.ListObjectsV2(listRequest);
        if (!listOutcome.IsSuccess()) {
            qCWarning(S3) << "Could not list prefix:" << prefix.c_str() << "-" << listOutcome.GetError().GetMessage().c_str();
            return false;
        }

        const auto &result = listOutcome.GetResult();
        const auto &objects = result.GetContents();

        if (objects.empty() && totalDeleted == 0) {
            // The prefix was a file or a 0-sized folder object — delete the key directly.
            Aws::S3::Model::DeleteObjectRequest request;
            request.SetBucket(bucketName);
            request.SetKey(s3url.Key());
            auto deleteOutcome = client.DeleteObject(request);
            if (!deleteOutcome.IsSuccess()) {
                qCWarning(S3) << "Could not delete object with key:" << s3url.key() << "-" << deleteOutcome.GetError().GetMessage().c_str();
                return false;
            }
            return true;
        }

        for (const auto &object : objects) {
            Aws::S3::Model::DeleteObjectRequest request;
            request.SetBucket(bucketName);
            request.SetKey(object.GetKey());
            auto deleteOutcome = client.DeleteObject(request);
            if (!deleteOutcome.IsSuccess()) {
                qCWarning(S3) << "Could not delete object:" << object.GetKey().c_str() << "-" << deleteOutcome.GetError().GetMessage().c_str();
                allDeleted = false;
            } else {
                totalDeleted++;
            }
        }

        isTruncated = result.GetIsTruncated();
        if (isTruncated) {
            listRequest.SetContinuationToken(result.GetNextContinuationToken());
        }
    } while (isTruncated);

    qCDebug(S3) << "Deleted" << totalDeleted << "objects under prefix:" << prefix.c_str();
    return allDeleted;
}

QString S3Backend::contentType(const S3Url &s3url)
{
    QString contentType;

    const Aws::S3::S3Client client = createS3Client(s3url.profileName());

    Aws::S3::Model::HeadObjectRequest headObjectRequest;
    headObjectRequest.SetBucket(s3url.BucketName());
    headObjectRequest.SetKey(s3url.Key());

    auto headObjectRequestOutcome = client.HeadObject(headObjectRequest);
    if (headObjectRequestOutcome.IsSuccess()) {
        contentType = QString::fromLatin1(headObjectRequestOutcome.GetResult().GetContentType().c_str(), headObjectRequestOutcome.GetResult().GetContentType().size());
        qCDebug(S3) << "Key" << s3url.key() << "has Content-Type:" << contentType;
    } else {
        qCDebug(S3) << "Could not get content type for key:" << s3url.key() << " - " << headObjectRequestOutcome.GetError().GetMessage().c_str();
    }

    return contentType;
}
