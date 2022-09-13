/*
    SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KIO_S3_H
#define KIO_S3_H

#include "s3backend.h"

#include <KIO/WorkerBase>

#include <QUrl>

class S3Worker : public KIO::WorkerBase
{
public:

    S3Worker(const QByteArray &protocol,
            const QByteArray &pool_socket,
            const QByteArray &app_socket);
    ~S3Worker() override;

    KIO::WorkerResult listDir(const QUrl &url) override;
    KIO::WorkerResult stat(const QUrl &url) override;
    KIO::WorkerResult mimetype(const QUrl &url) override;
    KIO::WorkerResult get(const QUrl &url) override;
    KIO::WorkerResult put(const QUrl &url, int permissions, KIO::JobFlags flags) override;
    KIO::WorkerResult copy(const QUrl &src, const QUrl &dest, int permissions, KIO::JobFlags flags) override;
    KIO::WorkerResult mkdir(const QUrl &url, int permissions) override;
    KIO::WorkerResult del(const QUrl &url, bool isfile) override;
    KIO::WorkerResult rename(const QUrl &src, const QUrl &dest, KIO::JobFlags flags) override;

private:
    Q_DISABLE_COPY(S3Worker)

    QScopedPointer<S3Backend> d { new S3Backend(this) };
};

#endif // KIO_S3_H
