# Installing DBWriter

DBWriter is a xxQS_NAMExx component that is responsible for writing reporting data to a relational database.

It is available for Gridware Cluster Scheduler only.

It was part of Sun Grid Engine (SGE) under the name ARCo (Accounting and Reporting Console).

## Prerequisites

Before installing DBWriter make sure to have the following requirements met:

* A database server is installed (supported are PostgreSQL, MySQL, Oracle).
* The database is accessible from the DBWriter host.
* On the DBWriter host, Java 8 must be installed. The DBWriter host can be any host that can access the database server and the xxQS_NAMExx installation directory.
* DBWriter uses JDBC to connect to the database. Get the JDBC driver for your database server.
  * It is a jar file, e.g., for PostgreSQL: `postgresql-42.7.7.jar`
  * You usually get it from the database vendor's website.
  * Copy it into the `$SGE_ROOT/dbwriter/lib` directory.

### Database

DBWriter requires a database server to be installed.
Supported database servers are PostgreSQL, MySQL, and Oracle.

For the following examples, we assume that the database is called `arco` and the user `arco_write` has write access to the database.
The user `arco_read` has read access to the database.

#### PostgreSQL

Install PostgreSQL. Follow the instructions on the PostgreSQL website for your operating system.
Configure PostgreSQL to allow remote connections and access to the database server from the DBWriter host.

As an administrative user, usually `postgres` on the database server:

```bash
$ createuser -P arco_write
$ createuser -P arco_read
$ createdb -O arco_write arco
```

#### MySQL

As an administrative user, usually `root` on the database server:

```bash
mysql
mysql> CREATE USER 'arco_write'@'%' IDENTIFIED BY 'secret';
mysql> CREATE USER 'arco_read'@'%' IDENTIFIED BY 'secret';
mysql> CREATE DATABASE arco;
mysql> GRANT ALL PRIVILEGES ON arco.* TO 'arco_write'@'%';
mysql> GRANT SELECT ON arco.* TO 'arco_read'@'%';
```

#### Oracle

TBD

### Java

DBWriter requires Java 8 to be installed on the system.

Best install from OS packages, e.g.,

* `openjdk-8-jre` on Debian/Ubuntu
* `java-1.8.0-openjdk` on CentOS/RHEL

## Installation

Source the xxQS_NAMExx environment script from the Gridware Cluster Scheduler installation directory.

As root in the `$SGE_ROOT` directory:

```bash
tar xzf <path to gcs-<version>-arco.tar.gz>
cd dbwriter
./inst_dbwriter.sh
```

### Accept the license agreement
```text

                      End User License Agreement (EULA)
                      =================================
                                (as of 09/2024)

1. General information, scope of application

1.1 The General Terms and Conditions (hereinafter referred to as “GTC”) of
the company

    HPC-Gridware GmbH,
    Johanna-Kinkel-Str. 1,
    93049 Regensburg

(hereinafter referred to  as “HPC-Gridware”)  consist of the General Terms
and Conditions

    https://www.hpc-gridware.com/terms-conditions/

and the supplementary  special contractual  terms and conditions  (hereinafter
referred to as “STC”) of the various  business areas (in particular “STC
Software”

    https://www.hpc-gridware.com/terms-and-conditions-software/

and "STC SLA"

    https://www.hpc-gridware.com/terms-and-conditions-SLA/

). In the following, the abbreviation  “GTC” is used for the framework GTC
and the STCs in their entirety.

[...]

6.3 If the parties are  merchants within the meaning  of the German Commercial
Code, special funds under  public law or legal entities  under public law, the
registered office of HPC-Gridware shall be the exclusive place of jurisdiction
for all claims  arising from the  legal relationship  with the  customer. This
also applies to customers  who do not have a general  place of jurisdiction in
the European  Union and to  customers who have  moved their domicile  or usual
place of residence to a country outside  the European Union after concluding a
contract. Irrespective of this, however,  HPC-Gridware is also entitled to sue
the customer at his general place of jurisdiction.
Do you agree with that license? (y/n) [n] >> y
```

### Start the installation
```text

Welcome to the Gridware Cluster Scheduler dbwriter module installation
----------------------------------------------------------------------
The installation will take approximately 5 minutes

Hit <RETURN> to continue >> 
```

Press `RETURN`.


### Specify / confirm the SGE_ROOT directory
```text
Checking $SGE_ROOT directory
----------------------------

The Cluster Scheduler root directory is:

   $SGE_ROOT = /scratch/joga/clusters/V91_BRANCH

If this directory is not correct (e.g. it may contain an automounter
prefix) enter the correct path to this directory or hit <RETURN>
to use default [/scratch/joga/clusters/V91_BRANCH] >> 
```
Press `RETURN`.
```text

Your $SGE_ROOT directory: /scratch/joga/clusters/V91_BRANCH

Hit <RETURN> to continue >> 
```
Press `RETURN`.

### Specify / confirm the SGE_CELL setting
```text
Cluster Scheduler cells
-----------------------

Cluster Scheduler supports multiple cells.

If you are not planning to run multiple Cluster Scheduler clusters or if you don't
know yet what is a Cluster Scheduler cell it is safe to keep the default cell name

   default

If you want to install multiple cells you can enter a cell name now.

The environment variable

   $SGE_CELL=<your_cell_name>

will be set for all further Cluster Scheduler commands.

Enter cell name [default] >> 
```
Press `RETURN`.
```text
Using cell >default<. 
Hit <RETURN> to continue >> 
```
Press `RETURN`.

### Specify the JAVA installation to use
```text
Java setup
----------

ARCo needs at least java 1.8

Enter the path to your java installation [] >> 
```
Enter the path to the Java 8 installation.

If the environment variable `JAVA_HOME` is set, this java installation will be preselected and you just have to press `RETURN`.
Otherwise enter the path to the Java 8 installation, e.g. `/usr/lib/jvm/java-1.8.0-openjdk-amd64`
and press `RETURN`.

### Select the database type
```text
Setup your database connection parameters
-----------------------------------------

Enter your database type ( o = Oracle, p = PostgreSQL, m = MySQL ) [] >> 
```
Select the database type, in this example we'll use `p` for PostgreSQL and press `RETURN`.

### Specify the database host
```text
Enter the name of your postgresql database host [] >>
```
Enter the name of the database host and press `RETURN`.

### Enter the port of the database service
```text
Enter the port of your postgresql database [5432] >>
```
Usually the pre-selected port is fine, press `RETURN`.

If you have changed the port for your specific database installation to a port number different to the default port, then specify the port number here and press `RETURN`.

### Specify the name of the database
```text
Enter the name of your postgresql database [arco] >> 
```
Enter the name of the database and press `RETURN`, or just press `RETURN` if you use the default database name.

### Specify the name of the database user doing write operations
```text
Enter the name of the database user [arco_write] >> 
```
Enter the name of the database user to use for write operations and press `RETURN`,
or just press `RETURN` if you use the default database user name.

### Enter the password of the database write user
```text
Enter the password of the database user >> 
```
Enter the password of the database user and press `RETURN`.

### Enter the table space to use for the arco database
```text
Enter the name of TABLESPACE for tables [pg_default] >> 
```
Press `RETURN` to use the default table space, 
unless you have a specific table space for the arco database. In this case enter the table space name here and press `RETURN`.

### Enter the name of the table space to use for indexes
```text
Enter the name of TABLESPACE for indexes [pg_default] >> 
```
Press `RETURN` to use the default table space,
unless you have a specific table space for indexes. In this case enter the table space name here and press `RETURN`.

### Specify the database schema
```text
Enter the name of the database schema [public] >>
```
Enter the name of the database schema and press `RETURN`, or just press `RETURN` if you use the default database schema.

### Specify the name of a user having the right to read from the database
```text
Applications should connect to the database as a user which has restricted
access. The name of this database user is needed to grant him access to the sge tables
and must be different from arco_write.

Enter the name of this database user [arco_read] >> 
```
Press `RETURN` to use the default database user name,
unless you have a specific database user name for read access.


### Test the database connection
```text
Database connection test
------------------------

Searching for the jdbc driver org.postgresql.Driver 
in directory /scratch/joga/clusters/V91_BRANCH/dbwriter/lib 

OK, jdbc driver found

Should the connection to the database be tested? (y/n) [y] >> 
```
Press `RETURN` to test the database connection.

In case of connection issues the error message from the JDBC library will be printed, e.g.,
```text
Test database connection to 'jdbc:postgresql://ubuntu-22-amd64-1:5432/arco' ...
SEVERE: Can not get a connection to jdbc:postgresql://ubuntu-22-amd64-1:5432/arco
SEVERE: FATAL: password authentication failed for user "arco_write"
Failed (3)
Do you want to repeat database connection setup? (y/n) [y] >> 
```
Fix the issue and press `RETURN` to try again.

### Specify the DBWriter interval
In this interval will DBWriter process the `reporting` file written by `sge_qmaster`.
```text
Generic parameters
------------------

Enter the interval between two dbwriter runs in seconds [60] >> 
```
Accept the default value by pressing `RETURN`, or enter a different value and press `RETURN`.

### Specify the DBWriter spool directory
```text
Enter the path of the dbwriter spool directory [/scratch/joga/clusters/V91_BRANCH/default/spool/dbwriter]>>
```
Accept the default value by pressing `RETURN`.

### Specify the path to a file defining rules

DBWriter has a rule engine which allows generating derived values from values in the database,
as well as rules for the deletion of outdated data.
DBWriter comes with a default rule file, but you can also specify a file with your own rules.
```text
Enter the file with the derived value rules [/scratch/joga/clusters/V91_BRANCH/dbwriter/database/postgres/dbwriter.xml] >>
```
Press `RETURN` to use the default rule file.

### Specify the log level DBWriter will use for writing its log file
```text
The dbwriter can run with different debug levels
Possible values: WARNING INFO CONFIG FINE FINER FINEST
Enter the debug level of the dbwriter [INFO] >> 
```
Press `RETURN` to use the default log level.

### Review and confirm the settings
```text
All parameters are now collected
--------------------------------

        SGE_ROOT=/scratch/joga/clusters/V91_BRANCH
        SGE_CELL=default
       JAVA_HOME=/usr/lib/jvm/java-8-openjdk-amd64/ (1.8.0_482)
          DB_URL=jdbc:postgresql://ubuntu-22-amd64-1:5432/arco
         DB_USER=arco
       READ_USER=arco_read
      TABLESPACE=pg_default
TABLESPACE_INDEX=pg_default
       DB_SCHEMA=public
        INTERVAL=60
       SPOOL_DIR=/scratch/joga/clusters/V91_BRANCH/default/spool/dbwriter
    DERIVED_FILE=/scratch/joga/clusters/V91_BRANCH/dbwriter/database/postgres/dbwriter.xml
     DEBUG_LEVEL=INFO

Are these settings correct? (y/n) [y] >> 
```
If all values are OK, press `RETURN` to start the actual installation.

### Confirm creation / upgrade of the database schema
```text
Database model installation/upgrade
-----------------------------------
Query database version ... no sge tables found
New version of the database model is needed

Should the database model be upgraded to version 10 6.2u1? (y/n) [y] >> 
```
Press `RETURN` to upgrade the database schema.
```text
Upgrade to database model version 10 6.2u1... 

Install version 6.0 (id=0) -------
Create table public.sge_job
Create index sge_job_idx0
Create index sge_job_idx1
create table sge_job_usage
Create table sge_job_log
Create table sge_job_request
Create table sge_queue
Create index sge_queue_idx0
Create table sge_queue_values
Create index sge_queue_values_idx0
Create table sge_host
[...]
Recreate view view_jobs_completed
grant privileges on view_jobs_completed to arco_read
Drop primary key constraint on sge_version table
Create compound primary key for sge_version
Update version table
committing changes
Version 6.2u1 (id=10) successfully installed
OK

Create start script sgedbwriter in /scratch/joga/clusters/V91_BRANCH/default/common

Create configuration file for dbwriter in /scratch/joga/clusters/V91_BRANCH/default/common

Hit <RETURN> to continue >> 
```
You should see output as above.
Press `RETURN` to continue.

### Install the startup script
```text
dbwriter startup script
-----------------------

We can install the startup script that will
start dbwriter at machine boot (y/n) [y] >> 
```
If you want DBWriter to start automatically at machine boot, press `RETURN` to continue.
Depending on your operating system, this will add DBWriter to the OSes init system (e.g., sysv init, systemd, Solaris SMF).

On Linux you will see output similar to the following:
```text
Using slice name from /scratch/joga/clusters/V91_BRANCH/default/common/slice_name: ocs8028
Installed dbwriter systemd unit file to /etc/systemd/system/ocs8028-dbwriter.service
```

If you do not want DBWriter to start automatically at boot time, then enter `n` and press `RETURN`.

### Congratulations! Your DBWriter installation is now complete.
```text
Installation of dbwriter completed
```

### Check if DBWriter is running:

The DBWriter init script can be used to check if DBWriter is running:
```bash
$SGE_ROOT/$SGE_CELL/common/sgedbwriter status
dbwriter is running (pid 4131759)
```

On a host running systemd, you can check the status of the service, e.g.,:
```bash
systemctl status ocs8028-dbwriter
● ocs8028-dbwriter.service - Cluster Scheduler dbwriter service
     Loaded: loaded (/etc/systemd/system/ocs8028-dbwriter.service; enabled; preset: enabled)
     Active: active (running) since Wed 2026-04-22 13:13:43 CEST; 12min ago
    Process: 4131697 ExecStart=/scratch/joga/clusters/V91_BRANCH/default/common/sgedbwriter start (code=exited, status=0/SUCCESS)
   Main PID: 4131759 (java)
      Tasks: 52 (limit: 75992)
     Memory: 110.1M (peak: 121.8M)
        CPU: 1.975s
     CGroup: /ocs8028.slice/ocs8028-dbwriter.service
             ├─4131757 /bin/sh /scratch/joga/clusters/V91_BRANCH/default/common/sgedbwriter start
             └─4131759 /usr/lib/jvm/java-8-openjdk-amd64//bin/java -server -classpath /scratch/joga/clusters/V91_BRANCH/dbwriter/lib/angus-activation-2.0.3.jar:/scratch/joga/cluste>

Apr 22 13:13:42 laptop-joga systemd[1]: Starting ocs8028-dbwriter.service - Cluster Scheduler dbwriter service...
Apr 22 13:13:42 laptop-joga sgedbwriter[4131697]: Creating dbwriter spool directory /scratch/joga/clusters/V91_BRANCH/default/spool/dbwriter
Apr 22 13:13:42 laptop-joga sgedbwriter[4131697]: starting dbwriter
Apr 22 13:13:43 laptop-joga sgedbwriter[4131697]: dbwriter started (pid=4131759)
Apr 22 13:13:43 laptop-joga systemd[1]: Started ocs8028-dbwriter.service - Cluster Scheduler dbwriter service.
```

### Check the DBWriter log file

DBWriter by default writes its log file to `$SGE_ROOT/$SGE_CELL/spool/dbwriter/dbwriter.log`.
It is a text file you can view with `cat` or `tail` or in a text editor.

You should see output like the following from the DBWriter startup:
```text
22/04/2026 13:13:42|laptop-joga|.ReportingDBWriter.initLogging|I|Starting up Gridware Cluster Scheduler dbwriter (Version 9.1.1prealpha) ---------------------------
22/04/2026 13:13:42|laptop-joga|r.ReportingDBWriter.initialize|I|Connection to db jdbc:postgresql://ubuntu-22-amd64-1:5432/arco
22/04/2026 13:13:43|laptop-joga|r.ReportingDBWriter.initialize|I|Found database model version 10
22/04/2026 13:13:43|laptop-joga|tingDBWriter.getDbWriterConfig|I|calculation file /scratch/joga/clusters/V91_BRANCH/dbwriter/database/postgres/dbwriter.xml has changed, reread it
22/04/2026 13:13:43|laptop-joga|Writer$VacuumAnalyzeThread.run|I|Next vacuum analyze will be executed at 23.04.26 12:11
22/04/2026 13:13:43|laptop-joga|ngDBWriter$StatisticThread.run|I|Next statistic calculation will be done at 22.04.26 14:13
```

Once reporting is enabled and data is processed, you should see output like the following:
```text
22/04/2026 13:25:43|laptop-joga|er.file.FileParser.processFile|I|Renaming reporting  to reporting.processing
22/04/2026 13:25:43|laptop-joga|iter.file.FileParser.parseFile|I|Deleting file reporting.processing
22/04/2026 13:25:43|laptop-joga|le.FileParser.createStatistics|I|Processed 16 lines in 0,02s (1000 lines/s)
22/04/2026 13:25:43|laptop-joga|rtingDBWriter.logEventDuration|I|calculating derived values took 0 hours 0 minutes
22/04/2026 13:25:43|laptop-joga|rtingDBWriter.logEventDuration|I|deleting outdated values took 0 hours 0 minutes
22/04/2026 13:25:43|laptop-joga|BWriter$DerivedValueThread.run|I|Next regular task (derived values and delete) will be done at 22.04.26 14:11
22/04/2026 13:26:43|laptop-joga|er.file.FileParser.processFile|I|Renaming reporting  to reporting.processing
22/04/2026 13:26:43|laptop-joga|iter.file.FileParser.parseFile|I|Deleting file reporting.processing
22/04/2026 13:26:43|laptop-joga|le.FileParser.createStatistics|I|Processed 12 lines in 0,01s (923,08 lines/s)
22/04/2026 13:27:43|laptop-joga|er.file.FileParser.processFile|I|Renaming reporting  to reporting.processing
22/04/2026 13:27:43|laptop-joga|iter.file.FileParser.parseFile|I|Deleting file reporting.processing
...
```

## Enable reporting in your Cluster Scheduler installation

DBWriter will read the reporting file written by `sge_qmaster` and write the data to the database. The `reporting` file is located in `$SGE_ROOT/$SGE_CELL/common/reporting`.

For a list of all configuration options, see the section **DBWriter** in the Admin Guide.

### Enable reporting

Writing of the `reporting` file needs to be enabled in the global configuration.
This version of the xxQS_NAMExx does not yet support reading the new one line JSON format, therefore writing of the old reporting file format needs to be enabled.

Edit the global configuration and modify the `reporting_params`:
Set the `reporting=true` and `old_reporting=true` parameters, keep the other parameters unchanged:
```bash
qconf -mconf
...
reporting_params             accounting=true reporting=true old_reporting=true \
                             flush_time=00:00:05 joblog=true sharelog=00:10:00
...
```

### Add host load values to reporting

The load information of the cluster hosts as well as capacity and availability of consumable resources can be written to the database.

As a first example, we'll make DBWriter write load values `cpu` and `np_load_avg`, as well as the capacity and availability of the `slots` consumable.

Edit the global host and modify the `report_variables` attribute:
```bash
qconf -me global
hostname              global
load_scaling          NONE
complex_values        NONE
user_lists            NONE
xuser_lists           NONE
projects              NONE
xprojects             NONE
usage_scaling         NONE
report_variables      cpu,np_load_avg,slots
```
