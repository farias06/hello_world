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
            fi	
          fi
    done	
    if [ $status = "KO" ]
      then
      echo "Status is KO for $inode"
      flag="OK"	
      inodestop=$inodeval
    fi    
    if [ $inodeval -gt $inodemax ]
      then
      inodemax=$inodeval
    fi
    cd /docker/app/server/
fi
done
if [ $flag = "KO" ]
  then
  echo "On na pas vu de container à l'arret on va donc un faire un nouveau le plus grand étant $inodemax";
  inodemax=`expr $inodemax + 1 `
  echo "Le nouveau sera donc le $inodemax ";
  mkdir /docker/app/server/$inodemax
  port=`expr 8085 + $inodemax `
  docker run -p $port:80 --link postgres2:postgres2 -v /docker/app/server/$inodemax:/app/server/ --name my-server7-$inodemax --log-driver=syslog --log-opt tag="my-server7-$inodemax"  -d my-server7
fi
if [ $flag = "OK" ]
  then
  echo "On a vu un container à l'arret on va donc le lancer : $inodestop";
  docker start my-server7-$inodemax
fi
echo "Fin du script";
