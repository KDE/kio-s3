# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>

cmake_minimum_required(VERSION 3.16)

set(KIO_S3_VERSION 1.0.2)
project(kio-s3 VERSION ${KIO_S3_VERSION})

set(QT_MIN_VERSION 6.5.0)
set(KF_MIN_VERSION 6.0.0)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(FeatureSummary)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQtDeclareLoggingCategory)
include(ECMSetupVersion)
include(KDEClangFormat)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    Network)

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    DocTools
    I18n
    KIO)

find_package(KF6KCMUtils ${KF_MIN_VERSION} CONFIG)
set_package_properties(KF6KCMUtils PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Required for building the System Settings module.")

find_package(KF6KirigamiAddons)
set_package_properties(KF6KirigamiAddons PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Required for building the System Settings module.")

find_package(AWSSDK REQUIRED COMPONENTS
    core
    s3)

set_package_properties(AWSSDK PROPERTIES DESCRIPTION "the AWS SDK for C++"
    URL "https://github.com/aws/aws-sdk-cpp"
    TYPE REQUIRED
    PURPOSE "Needed to build the S3 KIO worker")

find_package(Qt6Test QUIET)
set_package_properties(Qt6Test PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Required for building tests.")

if(NOT Qt6Test_FOUND)
   set(BUILD_TESTING OFF CACHE BOOL "Build the testing tree.")
endif()

ecm_setup_version(PROJECT
    VARIABLE_PREFIX S3
    VERSION_HEADER s3version.h)

add_definitions(-DQT_NO_URL_CAST_FROM_STRING)
add_definitions(-DQT_STRICT_ITERATORS)
add_definitions(-DTRANSLATION_DOMAIN=\"kio5_s3\")

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()
add_subdirectory(desktop)
add_subdirectory(src)

add_subdirectory(doc)
kdoctools_install(po)

ki18n_install(po)

file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h *.hpp *.c)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
