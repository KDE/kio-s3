/*
    SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "../src/s3url.h"

#include <QSet>
#include <QTest>

class S3UrlTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testS3Url_data();
    void testS3Url();
    void testStartAfter_data();
    void testStartAfter();
    void testObjectId();
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

void S3UrlTest::testStartAfter_data()
{
    QTest::addColumn<QUrl>("url");
    QTest::addColumn<QString>("expectedStartAfter");

    // clang-format off
    QTest::newRow("file in folder")
            << QUrl(QStringLiteral("s3://foo-bucket/r32/small.bin"))
            << QStringLiteral("r32/small.bi");

    QTest::newRow("top-level file")
            << QUrl(QStringLiteral("s3://foo-bucket/small.bin"))
            << QStringLiteral("small.bi");

    QTest::newRow("one-character top-level key")
            << QUrl(QStringLiteral("s3://foo-bucket/a"))
            << QString();

    QTest::newRow("two-byte last character")
            << QUrl(QStringLiteral("s3://foo-bucket/dir/caf\u00e9"))
            << QStringLiteral("dir/caf");

    QTest::newRow("four-byte last character")
            << QUrl(QStringLiteral("s3://foo-bucket/dir/a\U0001F600"))
            << QStringLiteral("dir/a");

    QTest::newRow("key with profile")
            << QUrl(QStringLiteral("s3://demos@r2/images/cat.jpg"))
            << QStringLiteral("images/cat.jp");
    // clang-format on
}

void S3UrlTest::testStartAfter()
{
    QFETCH(QUrl, url);
    QFETCH(QString, expectedStartAfter);
    QVERIFY(url.isValid());

    const auto s3url = S3Url(url);
    QCOMPARE(s3url.startAfter(), expectedStartAfter);

    // The listing must start right before the key: its UTF-8 bytes are a
    // strict prefix of the key's bytes.
    const QByteArray keyBytes = s3url.key().mid(1).toUtf8();
    const Aws::String startAfter = s3url.StartAfter();
    const QByteArray startAfterBytes(startAfter.c_str(), startAfter.size());
    QVERIFY(keyBytes.startsWith(startAfterBytes));
    QVERIFY(startAfterBytes.size() < keyBytes.size());
    QCOMPARE(QString::fromUtf8(startAfterBytes), expectedStartAfter);
}

void S3UrlTest::testObjectId()
{
    const S3ObjectId listed{QStringLiteral("r2"), QStringLiteral("demos"), QStringLiteral("images/cat.jpg")};
    const S3ObjectId fromUrl = S3Url(QUrl(QStringLiteral("s3://demos@r2/images/cat.jpg"))).objectId();
    QVERIFY(fromUrl == listed);
    QCOMPARE(qHash(fromUrl), qHash(listed));

    // The same key without a profile, in another profile and in another
    // bucket, and a profile name that would collide in a joined string.
    const QSet<S3ObjectId> ids{
        listed,
        fromUrl,
        S3Url(QUrl(QStringLiteral("s3://demos/images/cat.jpg"))).objectId(),
        S3Url(QUrl(QStringLiteral("s3://demos@minio/images/cat.jpg"))).objectId(),
        S3Url(QUrl(QStringLiteral("s3://other@r2/images/cat.jpg"))).objectId(),
        S3ObjectId{QStringLiteral("r2/demos"), QStringLiteral("images"), QStringLiteral("cat.jpg")},
    };
    QCOMPARE(ids.size(), 5);
}

#include "s3urltest.moc"
