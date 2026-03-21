/*
    SPDX-FileCopyrightText: 2026 Nekto Oleg <27015@riseup.net>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QAbstractListModel>
#include <QSet>

class KConfig;

struct S3Profile {
    QString id;
    QString name;
    QString endpointUrl;
    QString region;
    QString awsProfile;
    bool pathStyle = false;

    bool operator==(const S3Profile &other) const
    {
        return id == other.id && name == other.name && endpointUrl == other.endpointUrl && region == other.region && awsProfile == other.awsProfile
            && pathStyle == other.pathStyle;
    }
};

class S3ProfileModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        EndpointUrlRole,
        RegionRole,
        AwsProfileRole,
        PathStyleRole,
    };

    explicit S3ProfileModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addProfile(const QString &name, const QString &endpointUrl, const QString &region, const QString &awsProfile, bool pathStyle);

    Q_INVOKABLE void editProfile(int row, const QString &name, const QString &endpointUrl, const QString &region, const QString &awsProfile, bool pathStyle);

    Q_INVOKABLE void removeProfile(int row);

    Q_INVOKABLE QVariantMap profileAt(int row) const;

    void loadFromConfig(KConfig &config);
    void saveToConfig(KConfig &config) const;
    void clear();

    QList<S3Profile> profiles() const;

Q_SIGNALS:
    void profilesChanged();

private:
    static QString generateId(const QString &name, const QSet<QString> &existingIds);
    QList<S3Profile> m_profiles;
};
