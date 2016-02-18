#!/bin/bash


outDir="/tmp/proofLogs"
if [ -e $outDir ]; then
  echo "Remove $outDir? [y/n]"
  read answer
  if [ "$answer" = "y" ]; then
    rm -rf $outDir
  fi
fi
if [ ! -e $outDir ]; then
  mkdir $outDir
fi

proofServer="nansafmaster3.in2p3.fr"

baseDir='/tmp/pod-log-$USER'
condorPid=${2-'*'}
workingNode=${1-'*'}

logDir=$(gsissh -p 1975 -t "$proofServer" 'find '"$baseDir/$condorPid"' -type d -exec ls -td {} + | head -n 1')

logDir=$(echo $logDir | tr -d '\r')

rsync -e 'gsissh -p 1975' "${proofServer}:${logDir}/proof_log.${workingNode}.tgz" ${outDir}/


currDir=$PWD

cd $outDir
for archive in $(ls *.tgz); do
  tar -zxf $archive 2>/dev/null
  find var -name "*.log" -exec mv {} ./ \;
  rm -rf var
done

cd $currDir

echo ""
echo "Crash found in:"
grep -l "There was a crash" $outDir/*.log

echo ""
echo "See logs in $outDir"
