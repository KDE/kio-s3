# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>

install(FILES s3-network.desktop
        DESTINATION ${KDE_INSTALL_DATADIR}/remoteview)
install(FILES org.kde.kio_s3.metainfo.xml
        DESTINATION ${KDE_INSTALL_METAINFODIR})
