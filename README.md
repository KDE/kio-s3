<!--
    SPDX-License-Identifier: CC0-1.0
    SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>
-->

KIO S3
======

A KIO worker for Amazon Simple Storage Service (Amazon S3): https://aws.amazon.com/s3/

S3 is an object store. It has buckets and objects.
Buckets contain objects, and objects are made of data (usually a file) and metadata (information about the data).

BUILDING
========

The Amazon AWS SDK for C++ is required to build this project: https://github.com/aws/aws-sdk-cpp
In particular the `core` and `s3` SDK components are required.

USAGE
=====

1. Configure AWS credentials and region: https://docs.aws.amazon.com/credref/latest/refdocs/overview.html
2. Run `dolphin s3://`.

The worker supports S3 URIs with the following format:

    s3://mybucket/mykey

A valid S3 URI has the bucket name as `host` and the key as `path` of the URI.

FEATURES
========

- List S3 buckets and objects
- Upload files and folders to a bucket
- Delete objects from a bucket
- Copy folders or objects within S3
- Move or rename objects within S3


KNOW ISSUES
===========

- Moving or renaming a folder doesn't work, because folders don't exist in S3.
    - *Workaround*: copy the folder and then delete the original folder.
- Since folders don't exist in S3, you can type any garbage URL in the Dolphin navigator bar and the worker will pretend that it's a empty folder.
    - *Workaround*: prefer folder browsing using the Dolphin views, instead of manually typing the URLs in the navigation bar.
- Creating an empty folder is currently not supported.
    - *Workaround*: create it from the S3 web console.
- Listing a folder with more than 1000 files is currently not supported.
