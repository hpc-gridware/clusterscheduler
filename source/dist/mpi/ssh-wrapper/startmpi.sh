#!/bin/sh

# make sure that the $PE_HOSTFILE is readable
if [ ! -r $PE_HOSTFILE ]; then
   echo "$me: can't read $PE_HOSTFILE" >&2
   exit 1
fi

# create machine file from the PE_HOSTFILE
MACHINEFILE=$TMPDIR/machines
cat $PE_HOSTFILE | while read line; do
   hostname=`echo $line | awk '{print $1}'`
   slots=`echo $line | awk '{print $2}'`
   x=0
   # @todo do we need to consider job_is_first_task here?
   while [ $x -lt $slots ]; do
      # add here code to map regular hostnames into ATM hostnames
      echo $hostname >> $MACHINEFILE
      x=`expr $x + 1`
   done
done

# copy the ssh wrapper to the job's tmp dir
# the tmpdir is inserted into the PATH to make sure that our ssh wrapper is actually called from mpirun
SSHDIR=`dirname $0`
cp $SSHDIR/ssh $TMPDIR/ssh

# signal success to caller
exit 0
