# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>

if (QT_MAJOR_VERSION STREQUAL "5")
    set(DOCSUBDIR "kioslave5/s3")
else()
    set(DOCSUBDIR "kioworker6/s3")
endif()

kdoctools_create_handbook(index.docbook
    INSTALL_DESTINATION ${KDE_INSTALL_DOCBUNDLEDIR}/en
    SUBDIR ${DOCSUBDIR})
