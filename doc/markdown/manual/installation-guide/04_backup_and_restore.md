# Backup and Restore

The backup and restore of the xxQS_NAMExx software can be done manually. Unattended or periodic backups can be scheduled using the cron daemon.

The backup procedure depends on the configured spooling method:

- **classic** and **berkeleydb** — flatfile copy of the common
  configuration files and the qmaster spool directory.
- **postgres** — flatfile copy of the common configuration files
  plus `pg_dump --table=config` of the spool database. The
  `pg_dump` and `psql` client tools must be on `PATH` on the host
  that runs `inst_sge -bup` / `inst_sge -rst`. See the chapter
  **PostgreSQL Spooling** for the full PostgreSQL-specific backup
  and restore semantics, including credential handling and the
  `.pgpass` round-trip.

1. Login as root on the master host.

2. Make a copy of the backup configuration template and adjust it to your needs.

   ```
   $ cp $SGE_ROOT/util/install_modules/backup_template.conf $SGE_ROOT/my_backup.conf
   $ vi $SGE_ROOT/my_backup.conf
   ...
   ```

3. Start the backup process.

   ```
   cd $SGE_ROOT
   ./inst_sge -bup -auto $SGE_ROOT/my_backup.conf
   ```

To restore a backup you have to follow these steps:

1. Login as root on the master host.

2. Trigger the restore process.

   ```
   ./inst_sge -rst
   ```

   You will be asked several questions during the restore process (e.g. location of the SGE_ROOT, name of the default cell directory, location of the backup files, etc.).

[//]: # (Each file has to end with two empty lines)

