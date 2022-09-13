/*
    SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kio_s3.h"
#include "s3debug.h"

#include <QCoreApplication>

class KIOPluginForMetaData : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.worker.s3" FILE "s3.json")
};

extern "C"
int Q_DECL_EXPORT kdemain(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QLatin1String("kio_s3"));

    if (argc != 4) {
        fprintf(stderr, "Usage: kio_s3 protocol domain-socket1 domain-socket2\n");
        exit(-1);
    }

    S3Worker worker(argv[1], argv[2], argv[3]);
    qCDebug(S3) << "Starting kio_s3...";
    worker.dispatchLoop();

    return 0;
}

S3Worker::S3Worker(const QByteArray &protocol, const QByteArray &pool_socket, const QByteArray &app_socket)
    : WorkerBase("s3", pool_socket, app_socket)
{
    Q_UNUSED(protocol)
    qCDebug(S3) << "kio_s3 ready.";
}

S3Worker::~S3Worker()
{
    qCDebug(S3) << "kio_s3 ended.";
}

KIO::WorkerResult S3Worker::listDir(const QUrl &url)
{
    return finalize(d->listDir(url));
}

KIO::WorkerResult S3Worker::stat(const QUrl &url)
{
    return finalize(d->stat(url));
}

KIO::WorkerResult S3Worker::mimetype(const QUrl &url)
{
    return finalize(d->mimetype(url));
}

KIO::WorkerResult S3Worker::get(const QUrl &url)
{
    return finalize(d->get(url));
}

KIO::WorkerResult S3Worker::put(const QUrl &url, int permissions, KIO::JobFlags flags)
{
    return finalize(d->put(url, permissions, flags));
}

KIO::WorkerResult S3Worker::copy(const QUrl &src, const QUrl &dest, int permissions, KIO::JobFlags flags)
{
    return finalize(d->copy(src, dest, permissions, flags));
}

KIO::WorkerResult S3Worker::mkdir(const QUrl &url, int permissions)
{
    return finalize(d->mkdir(url, permissions));
}

KIO::WorkerResult S3Worker::del(const QUrl &url, bool isfile)
{
    return finalize(d->del(url, isfile));
}

KIO::WorkerResult S3Worker::rename(const QUrl &src, const QUrl &dest, KIO::JobFlags flags)
{
    return finalize(d->rename(src, dest, flags));
}

KIO::WorkerResult S3Worker::finalize(const S3Backend::Result &result)
{
    if (result.exitCode > 0) {
        return KIO::WorkerResult::fail(result.exitCode, result.errorMessage);
    }

    return KIO::WorkerResult::pass();
}

#include "kio_s3.moc"
