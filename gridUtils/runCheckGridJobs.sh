#!/bin/bash

outFilename="/tmp/outCheckGridJobs.txt"

date > $outFilename 2>&1

if [ -e $HOME/.profile ]; then
    . $HOME/.profile
elif [ -e $HOME/.bashrc ]; then
    . $HOME/.bashrc
fi

if [ -z $ROOTSYS ]; then
  hasCvmfs=$(ls /cvmfs/alice.cern.ch 2> /dev/null)
  if [ "$hasCvmfs" != "" ]; then
    source /cvmfs/alice.cern.ch/etc/login.sh
  fi

  if [ -n "$(type -t alienv)" ]; then
    lastVersion=$(alienv q | grep AliPhysics | tail -n 1)
    eval "$(alienv load $lastVersion)"
  elif [ -n "$(type -t alie)" ]; then
    lastVersion=$(alie q | grep AliPhysics | tail -n 1)
    eval "$(alie load $lastVersion)"
  elif [ -e "$ALISOFT/alice-env.sh" ]; then
    .  $ALISOFT/alice-env.sh -n
  else
    echo "Error: cannot load root environment"
    exit
  fi
fi

minRunNum=-1
if [ $1 ]; then
    minRunNum=$1
fi

userMail=""
if [ $2 ]; then
  userMail="$2"
fi

isValidToken=`alien-token-info | grep -c "Token is still valid"`
if [ $isValidToken -eq 0 ]; then
    echo "No valid token found. Nothing done!"
    exit
fi
proxyValidity=`xrdgsiproxy info 2>&1 | grep "time left" | cut -d " " -f 6`
if [[ $proxyValidity == "" || $proxyValidity == "0h:0m:0s" ]]; then
    echo "No valid proxy found. Nothing done!"
    exit
fi

pathToMacro="$(dirname $0)"
if [[ "$pathToMacro" != /* ]]; then
  pathToMacro="$PWD/$pathToMacro"
fi

echo "Valid token $isValidToken proxy $proxyValidity" >> $outFilename 2>&1

root -b <<EOF >> $outFilename 2>&1
.L $pathToMacro/gridCommands.C+
gridFindFailed(${minRunNum},"ALL",-1,"${userMail}");
.q
EOF

# Crontab example:
# 55 * 22-23 12 * /users/aliced/stocco/macros/gridAnalysis/runCheckGridJobs.sh 249044215 > /dev/null 2>&1