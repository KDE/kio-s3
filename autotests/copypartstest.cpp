/*
    SPDX-FileCopyrightText: 2026 Nekto Oleg <27015@riseup.net>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "../src/copyparts.h"

#include <QTest>

constexpr qint64 MiB = 1024LL * 1024;
constexpr qint64 GiB = 1024LL * 1024 * 1024;
constexpr qint64 TiB = 1024 * GiB;

class CopyPartsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testMultipartThreshold();
    void testPartsJustAboveThreshold();
    void testPartsJustAboveFiveGiB();
    void testPartSizeAtPartCountLimit();
    void testPartSizeOfSixTiB();
};

QTEST_GUILESS_MAIN(CopyPartsTest)

void CopyPartsTest::testMultipartThreshold()
{
    QVERIFY(!needsMultipartCopy(128 * MiB));
    QVERIFY(needsMultipartCopy(128 * MiB + 1));
    QVERIFY(needsMultipartCopy(5 * GiB));
}

void CopyPartsTest::testPartsJustAboveThreshold()
{
    const QList<CopyPart> parts = copyParts(128 * MiB + 1);
    QCOMPARE(parts.size(), qsizetype(2));
    QCOMPARE(parts.first().first, qint64(0));
    QCOMPARE(parts.first().last, 128 * MiB - 1);
    QCOMPARE(parts.last().first, 128 * MiB);
    QCOMPARE(parts.last().last, 128 * MiB);
}

void CopyPartsTest::testPartsJustAboveFiveGiB()
{
    const qint64 size = 5 * GiB + 1;
    QCOMPARE(copyPartSize(size), CopyPartSize);

    const QList<CopyPart> parts = copyParts(size);
    QCOMPARE(parts.size(), qsizetype(41));
    QCOMPARE(parts.first().first, qint64(0));
    for (qsizetype i = 0; i < parts.size() - 1; ++i) {
        QCOMPARE(parts.at(i).last - parts.at(i).first + 1, CopyPartSize);
        QCOMPARE(parts.at(i + 1).first, parts.at(i).last + 1);
    }
    QCOMPARE(parts.last().first, parts.last().last);
    QCOMPARE(parts.last().last, size - 1);
}

void CopyPartsTest::testPartSizeAtPartCountLimit()
{
    const qint64 size = MaxCopyPartCount * CopyPartSize;
    QCOMPARE(copyPartSize(size), CopyPartSize);
    QCOMPARE(copyParts(size).size(), qsizetype(MaxCopyPartCount));

    QVERIFY(copyPartSize(size + 1) > CopyPartSize);
    QCOMPARE(copyParts(size + 1).size(), qsizetype(MaxCopyPartCount));
}

void CopyPartsTest::testPartSizeOfSixTiB()
{
    const qint64 size = 6 * TiB;
    QVERIFY(copyPartSize(size) > CopyPartSize);

    const QList<CopyPart> parts = copyParts(size);
    QVERIFY(parts.size() <= MaxCopyPartCount);
    QCOMPARE(parts.last().last, size - 1);
}

#include "copypartstest.moc"
