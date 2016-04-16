#!/bin/bash
cd /docker/app/server/
i=0
flag="KO"
inodemax=0
inodestop=0
for inode in $(ls -R)
do
if
    [ -d $inode ]
then
    cd $inode 
    inodeval=$(echo $inode);
    status="KO"
    for filename in $(ls -R)
    do
	  if
	     [ -f $filename ]
	  then
	    if [ $filename = "status" ]
	      then
	      status=$(cat /docker/app/server/$inode/$filename)
	      load=$(cat /docker/app/server/$inode/load)
            fi	
          fi
    done	
    if [ $status = "OK" ]
      then
      echo "Status is OK for $inode"
      if [ $load = 0 ]	
	then 
	echo "Il n'y a pas de charge $load"
        flag="OK"	
        inodestop=$inodeval
      fi
    fi    
    cd /docker/app/server/
fi
done
if [ $flag = "OK" ]
  then
  echo "On va arreter ce containter : $inodestop";
  docker stop my-server7-$inodestop
  #Pour fixer notre bug ;)
  echo "KO" > /docker/app/server/$inodestop/status
fi
echo "Fin du script";
