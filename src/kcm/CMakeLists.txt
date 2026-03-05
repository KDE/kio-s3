# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2026 Nekto Oleg <27015@riseup.net>

kcmutils_add_qml_kcm(kcm_kio_s3
    SOURCES kcm_kio_s3.cpp s3profilemodel.cpp)

target_link_libraries(kcm_kio_s3
    KF6::KCMUtilsQuick
    KF6::I18n
    KF6::ConfigCore
    KF6::KIOCore
    KF6::KIOFileWidgets)
