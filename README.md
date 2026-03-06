<!--
    SPDX-License-Identifier: CC0-1.0
    SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>
-->

# KIO S3

A KIO worker for Amazon S3 and S3-compatible storage services
(Cloudflare R2, DigitalOcean Spaces, MinIO, etc.): <https://aws.amazon.com/s3/>

S3 is an object store. It has buckets and objects.
Buckets contain objects, and objects are made of data (usually a file) and metadata (information about the data).

## Building

The Amazon AWS SDK for C++ is required to build this project: <https://github.com/aws/aws-sdk-cpp>
In particular the `core` and `s3` SDK components are required.

## Usage

1. Configure AWS credentials and region: <https://docs.aws.amazon.com/credref/latest/refdocs/overview.html>
2. Run `dolphin s3://`.

The worker supports S3 URIs with the following format:

    s3://mybucket/mykey

A valid S3 URI has the bucket name as `host` and the key as `path` of the URI.

## Multi-Profile Support

You can configure multiple S3-compatible backends (AWS, Cloudflare R2,
DigitalOcean Spaces, MinIO, etc.) and switch between them using the
`bucket@profile` URL scheme:

    s3://@myprofile/          — list buckets on the profile's endpoint
    s3://mybucket@myprofile/  — browse a bucket on the profile's endpoint
    s3://mybucket/            — default profile (unchanged behavior)

### Configuration

1. Add your credentials to `~/.aws/credentials` under a named profile:

        [r2]
        aws_access_key_id = YOUR_ACCESS_KEY
        aws_secret_access_key = YOUR_SECRET_KEY

2. Create `~/.config/kio_s3rc` with a `[Profile-<name>]` group:

        [Profile-r2]
        EndpointUrl=https://ACCOUNT_ID.r2.cloudflarestorage.com
        Region=auto
        AwsProfile=r2
        PathStyle=true

   Available settings:

   - `EndpointUrl` — S3-compatible endpoint URL
   - `Region` — AWS region or `auto`
   - `AwsProfile` — profile name in `~/.aws/credentials` (defaults to the profile name)
   - `PathStyle` — use path-style addressing instead of virtual-hosted (required for most non-AWS providers)

3. Open Dolphin and navigate to `s3://@r2/` to browse your R2 buckets.

## Features

- List S3 buckets and objects
- Upload files and folders to a bucket
- Delete objects from a bucket
- Copy folders or objects within S3
- Move or rename objects within S3

## Known Issues

- Moving or renaming a folder doesn't work, because folders don't exist in S3.
  - *Workaround*: copy the folder and then delete the original folder.
- Since folders don't exist in S3, you can type any garbage URL in the Dolphin navigator bar and the worker will pretend that it's a empty folder.
  - *Workaround*: prefer folder browsing using the Dolphin views, instead of manually typing the URLs in the navigation bar.
- Creating an empty folder is currently not supported.
  - *Workaround*: create it from the S3 web console.