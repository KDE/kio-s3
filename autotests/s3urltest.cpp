/*
    SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "../src/s3url.h"

#include <QTest>

class S3UrlTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testS3Url_data();
    void testS3Url();
};

QTEST_GUILESS_MAIN(S3UrlTest)

void S3UrlTest::testS3Url_data()
{
    QTest::addColumn<QUrl>("url");
    QTest::addColumn<bool>("expectedIsBucket");
    QTest::addColumn<bool>("expectedIsKey");
    QTest::addColumn<bool>("expectedIsProfileRoot");
    QTest::addColumn<QString>("expectedBucketName");
    QTest::addColumn<QString>("expectedProfileName");
    QTest::addColumn<QString>("expectedKey");
    QTest::addColumn<QString>("expectedPrefix");

    // clang-format off
    QTest::newRow("root url")
            << QUrl(QStringLiteral("s3:"))
            << false
            << false
            << false
            << QString()
            << QString()
            << QString()
            << QString();

    QTest::newRow("bucket url")
            << QUrl(QStringLiteral("s3://foo-bucket"))
            << true
            << false
            << false
            << QStringLiteral("foo-bucket")
            << QString()
            << QString()
            << QString();

    QTest::newRow("root key url")
            << QUrl(QStringLiteral("s3://foo-bucket/"))
            << false
            << true
            << false
            << QStringLiteral("foo-bucket")
            << QString()
            << QStringLiteral("/")
            << QString();

    QTest::newRow("top-level file")
            << QUrl(QStringLiteral("s3://foo-bucket/bar.txt"))
            << false
            << true
            << false
            << QStringLiteral("foo-bucket")
            << QString()
            << QStringLiteral("/bar.txt")
            << QStringLiteral("bar.txt/");

    QTest::newRow("top-level folder")
            << QUrl(QStringLiteral("s3://foo-bucket/bar/"))
            << false
            << true
            << false
            << QStringLiteral("foo-bucket")
            << QString()
            << QStringLiteral("/bar/")
            << QStringLiteral("bar/");

    QTest::newRow("top-level folder without trailing slash")
            << QUrl(QStringLiteral("s3://foo-bucket/bar"))
            << false
            << true
            << false
            << QStringLiteral("foo-bucket")
            << QString()
            << QStringLiteral("/bar")
            << QStringLiteral("bar/");

    QTest::newRow("file-in-toplevel-folder")
            << QUrl(QStringLiteral("s3://foo-bucket/bar/foo.txt"))
            << false
            << true
            << false
            << QStringLiteral("foo-bucket")
            << QString()
            << QStringLiteral("/bar/foo.txt")
            << QStringLiteral("bar/foo.txt/");

    QTest::newRow("subfolder")
            << QUrl(QStringLiteral("s3://foo-bucket/bar/baz/"))
            << false
            << true
            << false
            << QStringLiteral("foo-bucket")
            << QString()
            << QStringLiteral("/bar/baz/")
            << QStringLiteral("bar/baz/");

    QTest::newRow("file-in-subfolder")
            << QUrl(QStringLiteral("s3://foo-bucket/bar/baz/foo.txt"))
            << false
            << true
            << false
            << QStringLiteral("foo-bucket")
            << QString()
            << QStringLiteral("/bar/baz/foo.txt")
            << QStringLiteral("bar/baz/foo.txt/");

    // Profile URL tests

    QTest::newRow("profile root")
            << QUrl(QStringLiteral("s3://@r2/"))
            << false
            << false
            << true
            << QString()
            << QStringLiteral("r2")
            << QStringLiteral("/")
            << QString();

    QTest::newRow("bucket with profile")
            << QUrl(QStringLiteral("s3://demos@r2/"))
            << false
            << true
            << false
            << QStringLiteral("demos")
            << QStringLiteral("r2")
            << QStringLiteral("/")
            << QString();

    QTest::newRow("bucket with profile no trailing slash")
            << QUrl(QStringLiteral("s3://demos@r2"))
            << true
            << false
            << false
            << QStringLiteral("demos")
            << QStringLiteral("r2")
            << QString()
            << QString();

    QTest::newRow("key with profile")
            << QUrl(QStringLiteral("s3://demos@r2/images/cat.jpg"))
            << false
            << true
            << false
            << QStringLiteral("demos")
            << QStringLiteral("r2")
            << QStringLiteral("/images/cat.jpg")
            << QStringLiteral("images/cat.jpg/");

    QTest::newRow("profile root without trailing slash")
            << QUrl(QStringLiteral("s3://@minio"))
            << false
            << false
            << true
            << QString()
            << QStringLiteral("minio")
            << QString()
            << QString();
    // clang-format on
}

void S3UrlTest::testS3Url()
{
    QFETCH(QUrl, url);
    QVERIFY(url.isValid());

    const auto s3url = S3Url(url);

    QFETCH(bool, expectedIsBucket);
    QCOMPARE(s3url.isBucket(), expectedIsBucket);

    QFETCH(bool, expectedIsKey);
    QCOMPARE(s3url.isKey(), expectedIsKey);

    QFETCH(bool, expectedIsProfileRoot);
    QCOMPARE(s3url.isProfileRoot(), expectedIsProfileRoot);

    QFETCH(QString, expectedBucketName);
    QFETCH(QString, expectedProfileName);
    QFETCH(QString, expectedKey);
    QFETCH(QString, expectedPrefix);

    QCOMPARE(s3url.bucketName(), expectedBucketName);
    QCOMPARE(s3url.profileName(), expectedProfileName);
    QCOMPARE(s3url.key(), expectedKey);
    QCOMPARE(s3url.prefix(), expectedPrefix);
}

#include "s3urltest.moc"
