# Backup and Restore 

The backup and restore of the xxQS_NAMExx software can be done manually. Unattended or periodic backups can be scheduled using the cron daemon. 

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

[//]: # (Eeach file has to end with two emty lines)

