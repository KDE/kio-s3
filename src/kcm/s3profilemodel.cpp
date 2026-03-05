/*
    SPDX-FileCopyrightText: 2026 Nekto Oleg <27015@riseup.net>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "s3profilemodel.h"

#include <KConfig>
#include <KConfigGroup>

S3ProfileModel::S3ProfileModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int S3ProfileModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_profiles.size();
}

QVariant S3ProfileModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    const auto &profile = m_profiles.at(index.row());
    switch (role) {
    case IdRole:
        return profile.id;
    case NameRole:
        return profile.name;
    case EndpointUrlRole:
        return profile.endpointUrl;
    case RegionRole:
        return profile.region;
    case AwsProfileRole:
        return profile.awsProfile;
    case PathStyleRole:
        return profile.pathStyle;
    }
    return {};
}

QHash<int, QByteArray> S3ProfileModel::roleNames() const
{
    return {
        {IdRole, "profileId"},
        {NameRole, "name"},
        {EndpointUrlRole, "endpointUrl"},
        {RegionRole, "region"},
        {AwsProfileRole, "awsProfile"},
        {PathStyleRole, "pathStyle"},
    };
}

void S3ProfileModel::addProfile(const QString &name, const QString &endpointUrl, const QString &region, const QString &awsProfile, bool pathStyle)
{
    QString id = name.toLower().simplified().replace(QLatin1Char(' '), QLatin1Char('-'));

    QSet<QString> existingIds;
    for (const auto &p : m_profiles) {
        existingIds.insert(p.id);
    }
    QString baseId = id;
    int suffix = 2;
    while (existingIds.contains(id)) {
        id = QStringLiteral("%1-%2").arg(baseId).arg(suffix++);
    }

    const int row = m_profiles.size();
    beginInsertRows(QModelIndex(), row, row);
    m_profiles.append({id, name, endpointUrl, region, awsProfile, pathStyle});
    endInsertRows();
    Q_EMIT profilesChanged();
}

void S3ProfileModel::editProfile(int row, const QString &name, const QString &endpointUrl, const QString &region, const QString &awsProfile, bool pathStyle)
{
    if (row < 0 || row >= m_profiles.size()) {
        return;
    }

    auto &profile = m_profiles[row];
    profile.name = name;
    profile.endpointUrl = endpointUrl;
    profile.region = region;
    profile.awsProfile = awsProfile;
    profile.pathStyle = pathStyle;

    Q_EMIT dataChanged(index(row), index(row));
    Q_EMIT profilesChanged();
}

void S3ProfileModel::removeProfile(int row)
{
    if (row < 0 || row >= m_profiles.size()) {
        return;
    }

    beginRemoveRows(QModelIndex(), row, row);
    m_profiles.removeAt(row);
    endRemoveRows();
    Q_EMIT profilesChanged();
}

QVariantMap S3ProfileModel::profileAt(int row) const
{
    if (row < 0 || row >= m_profiles.size()) {
        return {};
    }

    const auto &p = m_profiles.at(row);
    return {
        {QStringLiteral("id"), p.id},
        {QStringLiteral("name"), p.name},
        {QStringLiteral("endpointUrl"), p.endpointUrl},
        {QStringLiteral("region"), p.region},
        {QStringLiteral("awsProfile"), p.awsProfile},
        {QStringLiteral("pathStyle"), p.pathStyle},
    };
}

void S3ProfileModel::loadFromConfig(KConfig &config)
{
    beginResetModel();
    m_profiles.clear();

    const QStringList groups = config.groupList();
    for (const QString &groupName : groups) {
        if (!groupName.startsWith(QLatin1String("Profile-"))) {
            continue;
        }

        const QString id = groupName.mid(8); // strlen("Profile-")
        KConfigGroup group = config.group(groupName);

        S3Profile profile;
        profile.id = id;
        profile.name = group.readEntry("Name", id);
        profile.endpointUrl = group.readEntry("EndpointUrl", QString());
        profile.region = group.readEntry("Region", QString());
        profile.awsProfile = group.readEntry("AwsProfile", QString());
        profile.pathStyle = group.readEntry("PathStyle", false);
        m_profiles.append(profile);
    }

    endResetModel();
    Q_EMIT profilesChanged();
}

void S3ProfileModel::saveToConfig(KConfig &config) const
{
    // Remove old profile groups
    const QStringList groups = config.groupList();
    for (const QString &groupName : groups) {
        if (groupName.startsWith(QLatin1String("Profile-"))) {
            config.deleteGroup(groupName);
        }
    }

    // Write current profiles
    for (const auto &profile : m_profiles) {
        KConfigGroup group = config.group(QStringLiteral("Profile-%1").arg(profile.id));
        group.writeEntry("Name", profile.name);
        group.writeEntry("EndpointUrl", profile.endpointUrl);
        group.writeEntry("Region", profile.region);
        group.writeEntry("AwsProfile", profile.awsProfile);
        group.writeEntry("PathStyle", profile.pathStyle);
    }

    config.sync();
}

void S3ProfileModel::clear()
{
    beginResetModel();
    m_profiles.clear();
    endResetModel();
    Q_EMIT profilesChanged();
}

QList<S3Profile> S3ProfileModel::profiles() const
{
    return m_profiles;
}
