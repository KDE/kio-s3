# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>

set(kio_s3_SRCS kio_s3.cpp s3backend.cpp s3url.cpp)

ecm_qt_declare_logging_category(kio_s3_SRCS
    HEADER s3debug.h
    IDENTIFIER S3
    CATEGORY_NAME kf.kio.workers.s3)

kcoreaddons_add_plugin(kio_s3
    SOURCES ${kio_s3_SRCS}
    INSTALL_NAMESPACE kf${QT_MAJOR_VERSION}/kio)

target_link_libraries(kio_s3
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Network
    KF${QT_MAJOR_VERSION}::KIOCore
    KF${QT_MAJOR_VERSION}::KIOWidgets
    KF${QT_MAJOR_VERSION}::I18n
    ${AWSSDK_LINK_LIBRARIES})

set_target_properties(kio_s3 PROPERTIES OUTPUT_NAME "s3")
