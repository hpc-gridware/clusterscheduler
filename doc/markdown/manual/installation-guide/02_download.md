# Download Product Packages

For clusters intended for production environments, it is highly recommended to use pre-built packages by xxQS_COMPANY_NAMExx. xxQS_COMPANY_NAMExx ensures that all source code components used to build the packages are compatible with each other. The packages are built and carefully tested.

xxQS_COMPANY_NAMExx offers patch releases for pre-built packages, along with support services to ensure that productive clusters receive the latest fixes and security enhancements. Professional engineers are available to provide assistance in case of any questions.

Additionally, the packages from xxQS_COMPANY_NAMExx contain product enhancements that would not be available in packages that you built yourself.

To receive a quote, please contact us at [xxQS_COMPANY_MAILxx](mailto:xxQS_COMPANY_MAILxx) or fill and send following [Questionnaire](https://www.hpc-gridware.com/quote/).

The core xxQS_NAMExx code is available on GitHub. You can clone the required repositories and build the core product yourself, or use the nightly build. Please note that we do not provide support for these packages. It is not recommended to use the nightly build for production systems as it contains untested code that is still in development.

The download of the pre-built packages is available at [xxQS_COMPANY_NAMExx Downloads](https://www.hpc-gridware.com/download-main).

For a product installation you need a set of *tar.gz* files. Required are:

* the common package containing architecture independent files (the file names *gcs-`<version>`-common.\** e.g. *gcs-9.0.0-common.tar.gz*)

* one architecture specific package for each supported compute platform (files with the names *gcs-`<version>`-bin-`<os>`-`<platform>`.\** e.g. *gcs-9.0.0-bin-lx-amd64.tar.gz*)

* the gcs-`<version>`-md5sum.txt file

Additionally, you will also find product documentation, release notes and other packages for product extensions on the download page.

Once you have downloaded all packages, you can test and install them at the designated installation location. Please note in the instructions below the placeholder `<install-dir>` refers to the absolute path of the installation directory, while `<download-dir>` refers to the directory containing the downloaded files.

1. Copy the packages from your download location into the installation directory

    ```
    % cp <download-dir>/gcs-* <install-dir>
    ```

2. Check if the downloaded files where downloaded correctly by calculating the MD5 checksum. 

    ```
    % cd <install-dir>
    % md5 gcs-*
    ...
    % cat gcs-9.0.0-md5sum.txt
    ...
    ```
   
    Compare the output of the md5 command with that of the cat command. If one or more checksums are not correct then re-download the faulty files and repeat the previous steps, otherwise continue.

3. Unpack the packages as root and set the SGE_ROOT variable manually and execute the script *util/setfileperm.sh* to verify and adapt ownership and file permissions of the unpacked files.

    ```
    % su
    # cd <install-dir>
    # tar xfz gcs-*.tar.gz
    # export SGE_ROOT=<install-dir>
    # util/setfileperm.sh $SGE_ROOT
    ```
   
4. If your `<install-dir>` is located on a shared filesystem available on all hosts in the cluster then you can start the installation process.

[//]: # (Eeach file has to end with two emty lines)

