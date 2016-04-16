#!/bin/bash
echo "Debut du script"
echo "global" > /docker/app/haproxy.cfg
echo "    maxconn 400 " >> /docker/app/haproxy.cfg
echo " " >> /docker/app/haproxy.cfg
echo "defaults" >> /docker/app/haproxy.cfg
echo "    log 127.0.0.1 local5 debug" >> /docker/app/haproxy.cfg
echo "    mode    tcp" >> /docker/app/haproxy.cfg
echo "    retries 4" >> /docker/app/haproxy.cfg
echo "    maxconn 200" >> /docker/app/haproxy.cfg
echo "    timeout connect  5000" >> /docker/app/haproxy.cfg
echo "    timeout client  50000" >> /docker/app/haproxy.cfg
echo "    timeout server  50000" >> /docker/app/haproxy.cfg
echo "" >> /docker/app/haproxy.cfg
echo "frontend http-in" >> /docker/app/haproxy.cfg
echo "    bind *:80" >> /docker/app/haproxy.cfg
echo "    default_backend serveur" >> /docker/app/haproxy.cfg
echo "" >> /docker/app/haproxy.cfg
echo "backend serveur" >> /docker/app/haproxy.cfg
echo "        mode tcp" >> /docker/app/haproxy.cfg
echo "        balance roundrobin" >> /docker/app/haproxy.cfg
cd /docker/app/server/
i=0
for inode in $(ls -R)
do
if
    [ -d $inode ]
then
    status="KO"
    cd $inode 
    for filename in $(ls -R)
    do
	  if
	     [ -f $filename ]
	  then
	    if [ $filename = "status" ]
	      then
	      status=$(cat /docker/app/server/$inode/$filename)
	      haproxy=$(cat /docker/app/server/$inode/haproxy)	 
	      haproxy2=$(echo $haproxy | sed "s/eth0/server$inode/g")
            fi	
          fi
    done	
    if [ $status = "OK" ]
      then
      echo "Status is OK"
      echo $haproxy2 >> /docker/app/haproxy.cfg
    fi
    cd /docker/app/server/
fi
done
echo "Fin du script";
