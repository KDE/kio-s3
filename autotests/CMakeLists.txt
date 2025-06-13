# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>

include(ECMAddTests)

ecm_add_test(
    s3urltest.cpp ../src/s3url.cpp
    LINK_LIBRARIES Qt6::Test ${AWSSDK_LINK_LIBRARIES}
    TEST_NAME s3urltest)
