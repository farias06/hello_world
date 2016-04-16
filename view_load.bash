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
	      cload=$(cat /docker/app/server/$inode/load)	
	      cmax=$(cat /docker/app/server/$inode/max) 
            fi	
          fi
    done	
    if [ $status = "OK" ]
      then
      echo "Status is OK for $inode: max $cmax load $cload"
      load=`expr $load + $cload`
      max=`expr $max + $cmax`
    fi
    cd /docker/app/server/
fi
done
echo "La charge actuelle est de $load/$max";
echo "Fin du script";
