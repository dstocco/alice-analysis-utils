#!/bin/bash

if [ -z $1 ]; then
  echo "Usage: $0 dirToUpdate"
  exit
fi

localDir="${1%/}/"
remoteDir=${localDir/'MU/'/}
remoteDir="aliqamu@lxplus.cern.ch:/afs/cern.ch/work/a/aliqamu/www/$remoteDir"

echo "Updating runs (dry run)"
rsync -avun --ignore-existing $localDir $remoteDir

echo "Dry run perofrmed: execute update? [y/n]"
read answer
if [ "$answer" = "y" ]; then
  rsync -avu --ignore-existing $localDir $remoteDir
fi

echo ""
echo "Updating trending (dry run)"
rsync -avun --exclude='*/' $localDir $remoteDir
echo "Dry run perofrmed: execute update? [y/n]"
read answer
if [ "$answer" = "y" ]; then
  rsync -avu --exclude='*/' $localDir $remoteDir
fi
