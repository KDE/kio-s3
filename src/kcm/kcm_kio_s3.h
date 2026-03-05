/*
    SPDX-FileCopyrightText: 2026 Nekto Oleg <27015@riseup.net>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "s3profilemodel.h"

#include <KQuickConfigModule>

class KCMKioS3 : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(S3ProfileModel *profileModel READ profileModel CONSTANT)

public:
    KCMKioS3(QObject *parent, const KPluginMetaData &metaData);

    S3ProfileModel *profileModel() const;

public Q_SLOTS:
    void load() override;
    void save() override;
    void defaults() override;

private:
    void updateNeedsSave();
    void syncPlaces();

    S3ProfileModel *m_model = nullptr;
    QList<S3Profile> m_savedProfiles;
};
