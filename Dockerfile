FROM fedora
MAINTAINER toto toto@cyber-neurones.org 
COPY ./server8 /sbin/server8
RUN dnf install postgresql -y
# Le port en ecoute 
EXPOSE 80 
# Pour lancer postgres 
CMD ["/sbin/server8"]

