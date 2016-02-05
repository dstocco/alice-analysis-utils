#!/bin/bash

if [[ -z $1 || -z $2 ]]; then
  echo "Usage: $0 <runList.txt> </alice/data/year/LHCxx/pass>"
  exit
fi

runList="$(cat $1 | xargs)"

pass="$(basename $2)"
path="$(dirname $2)"

baseDir="$PWD"

for irun in $runList; do
  currDir="${baseDir}$path/000${irun}/$pass"
  if [ -d $currDir ]; then
    echo "Directory $currDir already exists. Content:"
    ls $currDir
    echo "Overwrite? [y/n]"
    read answer
    if [ "$answer" = "y" ]; then
      rm -rf $currDir
    else
      continue
    fi
  fi
  mkdir -p $currDir
  cd $currDir
  tmpList="tmp_$1"
  echo $irun > $tmpList
  ln -s $ALICE_PHYSICS/PWGPP/MUON/lite/mergeGridFiles.C
  aliroot -b << EOF
.L mergeGridFiles.C+
completeProd("$tmpList","$pass","$path")
EOF
  rm $tmpList
  if [ -L mergeGridFiles.C ]; then
    rm mergeGridFiles*
  fi
  outFilename="QAresults_${irun}.root"
  if [ -e $outFilename ]; then
    mv $outFilename QAresults.root
    zip -0 QA_merge_archive.zip QAresults.root
    rm complete*.txt
    rm QAresults.root
  fi
  cd $baseDir
done
