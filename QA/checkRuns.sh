#!/bin/bash

if [[ -z $1 || -z $2 ]]; then
    echo "Usage: $0 fileName1 fileName2"
    exit
fi

inputFile1="$1"
inputFile2="$2"

function GetRunList()
{
  inFile="$1"
  outFile="$2"

  touch $outFile

  inList=`grep -oE "[0-9]+" $inFile | xargs`
  for irun in $inList; do
    len=${#irun}
    if [[ $len == 6 || $len == 9 ]]; then
      echo ${irun#"000"} >> $outFile
    fi
  done
}

tmpFileName1="/tmp/tmpCheckRuns1.txt"
tmpFileName2="/tmp/tmpCheckRuns2.txt"

GetRunList "$inputFile1" "$tmpFileName1"
GetRunList "$inputFile2" "$tmpFileName2"

matchList=`comm -12 <(sort $tmpFileName1) <(sort $tmpFileName2) | xargs`
onlyIn1=`comm -23 <(sort $tmpFileName1) <(sort $tmpFileName2) | xargs`
onlyIn2=`comm -13 <(sort $tmpFileName1) <(sort $tmpFileName2) | xargs`

#inList1=`cat ${tmpFileName} | xargs`
#inList2=`cat ${inputFile2} | xargs`

#matchList=""
#onlyIn1=""
#onlyIn2="${inList2}"
#for irun in ${inList1}; do
#  matchString=`grep ${irun} ${inputFile2}`
#  if [ "${matchString}" != "" ]; then
#    matchList="${matchList} ${irun}"
#    onlyIn2=${onlyIn2//"${matchString}"/""}
#  else
#    matchString=`grep ${irun} ${inputFile1}`
#    onlyIn1="${onlyIn1} ${matchString}"
#  fi
#done
echo "Common runs (`echo ${matchList} | wc -w`) :"
echo "${matchList}" | xargs
echo ""
echo "only in ${inputFile1} (`echo ${onlyIn1} | wc -w`) :"
echo "${onlyIn1}" | xargs
echo ""
echo "only in ${inputFile2} (`echo ${onlyIn2} | wc -w`) :"
echo "${onlyIn2}" | xargs

rm $tmpFileName1
rm $tmpFileName2
