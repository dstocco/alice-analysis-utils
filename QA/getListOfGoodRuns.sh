#!/bin/bash

if [ -z $1 ]; then
  echo "Usage: $0 muonQA.tex [outputFilename]"
  exit 1
fi

texFile="$1"
outFilename="$2"

if [ -z $2 ]; then
  outDir=$(dirname $texFile)
  if [ "$outDir" = "" ]; then
    outDir="."
  fi
  outFilename="$outDir/runListGoodForQA.txt"
fi


runListLogbook=$(grep runTab $texFile | grep -v newcommand | grep -v notInLogColor | cut -d "}" -f 1 | cut -d "{" -f 2 | xargs)
runListQA=$(grep runTab $texFile | grep -v newcommand | grep -v notInLogColor | grep -v errorColor | cut -d "}" -f 1 | xargs)

runListGoodForQA=""
for irun in $runListQA; do
  if [[ "$irun" == *"["* ]]; then
    echo "Keep: $irun? [y/n]";
    read answer
    if [[ $answer != "y" ]]; then
      continue
    fi
  fi
  runNum=$(echo $irun | cut -d "{" -f 2 | xargs)
  runListGoodForQA="${runListGoodForQA} $runNum"
done

echo -e "${runListGoodForQA// /\\n}" | sort -rn > $outFilename

function openWebpage {
  local runList="$1"
  local logbookUrl="https://alice-logbook.cern.ch/logbook/date_online.php?p_cont=es&prsf_rn=%2C%2C${runList// /+}&p_tab=ptc"

  which xdg-open > /dev/null 2>&1
  if [[ $? == 0 ]]; then
    xdg-open "$logbookUrl"
  else
    open "$logbookUrl"
  fi
}

openWebpage "$runListLogbook"
openWebpage "$runListGoodForQA"
