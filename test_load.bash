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
pourc=$(( $load * 100 / $max))
#On fixe le seuil à 0% ce qui est extrême on devrait surement mettre 25%
if [ $pourc -eq 0 ]
  then
  # On fixe le minimum à 10 ressources.
  if [ $max -gt 10 ]
     then
     logger -t CheckLoad "On n'a plus de charge on stoppe un container"
     echo "On peut arreter des containers";
     /docker/scripts/stop_container.bash
     echo "On refait la configuration de haproxy"
     /docker/scripts/build_haproxy.bash
     echo "On recharge haproxy"
     docker restart mon-haproxy-v15c
  fi
fi
#On fixe le seuil a 70% avant de lancer de nouveaux containers
if [ $pourc -gt 70 ]
  then
  logger -t CheckLoad "On a de la charge on lance un container"
  echo "On doit faire un nouveau container"
  /docker/scripts/create_container.bash
  echo "On refait la configuration de haproxy"
  /docker/scripts/build_haproxy.bash
  echo "On recharge haproxy"
  docker restart mon-haproxy-v15c  
fi
echo "Fin du script";
