/*
    SPDX-FileCopyrightText: 2026 Nekto Oleg <27015@riseup.net>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kcm_kio_s3.h"

#include <KConfig>
#include <KFilePlacesModel>
#include <KPluginFactory>
#include <KProtocolManager>

#include <QUrl>

K_PLUGIN_CLASS_WITH_JSON(KCMKioS3, "kcm_kio_s3.json")

KCMKioS3::KCMKioS3(QObject *parent, const KPluginMetaData &metaData)
    : KQuickConfigModule(parent, metaData)
    , m_model(new S3ProfileModel(this))
{
    setButtons(Apply | Default);

    connect(m_model, &S3ProfileModel::profilesChanged, this, &KCMKioS3::updateNeedsSave);
}

S3ProfileModel *KCMKioS3::profileModel() const
{
    return m_model;
}

void KCMKioS3::load()
{
    KConfig config(QStringLiteral("kio_s3rc"), KConfig::SimpleConfig);
    m_model->loadFromConfig(config);
    m_savedProfiles = m_model->profiles();
    setNeedsSave(false);
    setRepresentsDefaults(m_model->profiles().isEmpty());
}

void KCMKioS3::save()
{
    KConfig config(QStringLiteral("kio_s3rc"), KConfig::SimpleConfig);
    m_model->saveToConfig(config);
    m_savedProfiles = m_model->profiles();
    setNeedsSave(false);

    syncPlaces();
    KProtocolManager::reparseConfiguration();
}

void KCMKioS3::defaults()
{
    m_model->clear();
    updateNeedsSave();
}

void KCMKioS3::updateNeedsSave()
{
    setNeedsSave(m_model->profiles() != m_savedProfiles);
    setRepresentsDefaults(m_model->profiles().isEmpty());
}

void KCMKioS3::syncPlaces()
{
    KFilePlacesModel places;

    // Remove old S3 profile entries
    for (int i = places.rowCount() - 1; i >= 0; --i) {
        const QModelIndex idx = places.index(i, 0);
        const QUrl placeUrl = places.url(idx);
        if (placeUrl.scheme() == QLatin1String("s3") && placeUrl.authority().startsWith(QLatin1Char('@'))) {
            places.removePlace(idx);
        }
    }

    // Add current profiles
    const auto profiles = m_model->profiles();
    for (const auto &profile : profiles) {
        QUrl url;
        url.setScheme(QStringLiteral("s3"));
        url.setAuthority(QStringLiteral("@%1").arg(profile.id));
        url.setPath(QStringLiteral("/"));

        places.addPlace(profile.name, url, QStringLiteral("folder-cloud"));
    }
}

#include "kcm_kio_s3.moc"
