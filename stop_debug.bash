#!/bin/bash
cd /docker/app/server/
i=0
load=0
max=0
for inode in $(ls -R)
do
if
    [ -d $inode ]
then
    cd $inode 
    status="KO"
    for filename in $(ls -R)
    do
	  if
	     [ -f $filename ]
	  then
	    if [ $filename = "status" ]
	      then
	      status=$(cat /docker/app/server/$inode/$filename)
	      debug=$(cat /docker/app/server/$inode/debug)
	      syslog=$(cat /docker/app/server/$inode/syslog)	
            fi	
          fi
    done	
    if [ $status = "OK" ]
      then
      echo "Stop syslog ($syslog) et debug ($debug) sur $inode"
      echo 0 > /docker/app/server/$inode/debug	
      echo 0 > /docker/app/server/$inode/syslog 
    fi
    cd /docker/app/server/
fi
done
echo "Fin du script";
