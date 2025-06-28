# Introduction

This guide is intended for developers who want to contribute to the ClusterScheduler project. It provides an overview of the project structure, the development process, and the tools used in the project.

# Contributing to the ClusterScheduler Project

@todo how to contribute to the project (fork, branch, pull request, ...)

# Tags and Branches

## Tags

Whenever we release a stable version we create a tag in the git repository. Version numbers consist of three parts: major, minor, and patch, e.g. `9.0.0`. The tag is named after the version number, e.g. `V900_TAG`. Pre-releases might also be tagged, e.g. `V900alpha1_TAG`.

## Branches

The `master` branch is the main development branch. It is used for the development of new features and bug fixes. When we start working on features for a new major or minor release we will create a new branch for the maintenance of the current release, e.g. a `V90_BRANCH` for the maintenance of `9.0.x` versions when we start development on `9.1.0`.

## Existing Tags and Branches of the ClusterScheduler Project

Private branches and feature specific branches might be created by developers for their work.
They will not be described here.

Tags and branches before `V9` will also not be described here.

| Branch       | Tag         | Description                                       |  
|:-------------|:------------|:--------------------------------------------------|
| master       |             | main development branch                           |  
|              | V900\_TAG   | first OCS/GCS release                             |  
|              | V901\_TAG   | first 9.0 patch                                   |
|              | V902\_TAG   | second 9.0 patch                                  |
|              |             |                                                   |
| V900\_BRANCH |             | maintenance of 9.0.0                              |
|              | V900p1\_TAG | patch to the 9.0.0 making it work on GCP (CS-663) |
|              |             |                                                   |
| V90\_BRANCH  |             | maintenance of 9.0                                |
|              | V903\_TAG   | third 9.0 patch                                   |
|              | V904\_TAG   | fourth 9.0 patch                                  |
|              | V905\_TAG   | fifth 9.0 patch                                   |
|              | V906\_TAG   | sixth 9.0 patch                                   |
|              |             |                                                   |

[//]: # (Each file has to end with two emty lines)
