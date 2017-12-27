#!/bin/sh

cmd="${1}" 
case ${cmd} in 
    makeall)
        sh -c 'cd driver/binder && make'
        sh -c 'cd driver/ashmem && make'
        sh -c 'cd servicemanager && make'
        sh -c 'cd libs && make'
        sh -c 'cd test && make'
        sh -c 'cd cmds && make'
        ;;  
    clean)
        sh -c 'cd driver/binder && make clean'
        sh -c 'cd driver/ashmem && make clean'
        sh -c 'cd servicemanager && make clean'
        sh -c 'cd libs && make clean'
        sh -c 'cd test && make clean'
        sh -c 'cd cmds && make clean'
        ;; 
    insmod)
        sh -c 'sudo insmod driver/binder/binder_linux.ko'
        sh -c 'sudo insmod driver/ashmem/ashmem_linux.ko'
        ;; 
    rmmod)
        sh -c 'sudo rmmod binder_linux'
        sh -c 'sudo rmmod ashmem_linux'
        ;; 
   *)  
      echo "`basename ${0}`:usage: [makeall] | [clean] | [insmod] | [rmmod]" 
      exit 1
      ;; 
esac