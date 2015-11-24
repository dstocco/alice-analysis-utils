#!/bin/bash

baseRemoteDir="http://aliqamu.web.cern.ch/aliqamu"
triggerList=""
authors='Cynthia Hadjidakis, Diego Stocco, Mohamad Tarhini'

optList="a:cst:"
while getopts $optList option
do
  case $option in
    a ) authors=$OPTARG;;
    c ) baseRemoteDir="aliqamu@lxplus.cern.ch:/afs/cern.ch/work/a/aliqamu/www";;
#    f ) forceLatex=1;;
    s ) baseRemoteDir="${SUBA}:/scratch/aliced/stocco/gridAnalysis/muonQA/MU";;
    t ) triggerList=$OPTARG;;
    * ) echo "Unimplemented option chosen."
    EXIT=1
;;
  esac
done

shift $(($OPTIND - 1))

if [[ "$EXIT" -eq 1 ]]; then
  echo "Usage: `basename $0` (-$optList) dataType/year/period/pass"
  echo "       -a comma separated author list (default: $authors)"
  echo "       -c search on cern afs (default: $baseRemoteDir)"
#  echo "       -f run latex even for existing files (default: $forceLatex)"
  echo "       -s search on subatech (default: $baseRemoteDir)"
  echo "       -t comma separated trigger list"
  exit 2
fi

if [ -z $ALICE_PHYSICS ]; then
  echo "Please set ALICE_PHYSICS environement"
  exit
fi


subDir="$1"

if [ -z $subDir ]; then
  echo "Please enter specific: dataType/year/period/pass"
  read subDir
fi


#### Syncronize with remote directory
baseLocalDir="$PWD"
inputDir="${baseRemoteDir}/$subDir"
localDir="${baseLocalDir}/$subDir"


if [ ! -d "$localDir" ]; then
  mkdir -p "$localDir"
fi

cd $localDir
isNew=0
if [ ${baseRemoteDir:0:4} = "http" ]; then
#  fileList="QA_muon_tracker.pdf QA_muon_tracker.root QA_muon_trigger.pdf QA_muon_trigger.root"
#  for ifile in $fileList; do
#    tmpFile="tmp_$ifile"
#    curl -R "$file" -o "$tmpFile"
#    if [ $ifile -nt $tmpFile ]; then
#      mv $tmpFile $ifile
#      isNew=1
#    else
#      rm $tmpFile
#    fi
  curl -RO "${inputDir}/QA_muon_{tracker,trigger}.{pdf,root}"
else
  rsync -avu --exclude="00*" --exclude="*.log" $inputDir/ $localDir/
fi

trackerFile="QA_muon_tracker.pdf"
if [ ! -e "$trackerFile" ]; then
  echo "Problems in downloading files..."
  exit
fi


pass=$(basename "$subDir")
period=$(basename $(dirname "$subDir"))
texFile="muonQA.tex"

oldTexFile="$texFile.backup"

if [ -e "$texFile" ]; then
  echo "Updating existing latex file"
  mv $texFile $oldTexFile
#  if [ $forceLatex -eq 0 ]; then
#    echo "Latex file already present. It will not be re-created"
#  else
#    rm $texFile
#  fi
fi


##### Make slides
#if [ ! -e "$texFile" ]; then

##### Ask for a list of triggers
if [ -z $triggerList ]; then
  if [ -e "$oldTexFile" ]; then
    triggerList=$(grep "%TriggerList=" $oldTexFile | cut -d "=" -f2)
  fi
  if [ -z $triggerList ]; then
    open "$trackerFile"
    echo "Trigger list (comma separated) for $PWD"
    read triggerList
  fi
fi

root -b -q $ALICE_PHYSICS/PWGPP/MUON/lite/MakeSlides.C+\(\"$period\"\,\"$pass\",\"$triggerList\",\""$authors"\",\"QA_muon_tracker.pdf\",\"QA_muon_trigger.pdf\",\"$texFile\"\)
issueFound=$(grep "page=-1" $texFile)
if [ "$issueFound" != "" ]; then
  echo "Problem in $texFile : one page was not found :"
  echo "$issueFound"
  echo "Please check the file and fix the issue (e.g. by removing the slide)"
  open $texFile
  answer="n"
  while [ "$answer" != "y" ]; do
    echo "Press y when done"
    read answer
  done

fi
#fi

if [ ! -e "$texFile" ]; then
  echo "Something wrong: latex file not produced..."
  if [ -e $oldTexFile ]; then
    mv $oldTexFile $texFile
  fi
  cd $baseLocalDir
  exit
fi



##### Get analyzed runs from QA
runListQA="runListQA.txt"
grep "runTab" $texFile | grep -oE "{[0-9]+}" | sed 's/{//;s/}//' > $runListQA


##### Get run list from logbook
runListLogbook="runListLogbook.txt"
logbookUrl="https://alice-logbook.cern.ch/logbook/date_online.php?p_cont=sb&p_rspn=1&p_rsob=l.run&p_rsob_dir=DESC&ptcf_rtc=%2CAt+least%2CMUON_TRG%3BMUON_TRK%3B%2CAt+least&prsf_rtype=PHYSICS%2C&prsf_rdur=10+m%2C%2C&pqff_det_MUON_TRK=Bad+run%2C1&pqff_det_MUON_TRG=Bad+run%2C1&prsf_rgmr=Yes&pqff_rq=Bad+run%2C1&psf_sd=Yes&psf_det_GLOBAL=DONE%2C0&psf_det_MUON_TRK=DONE%2C0&psf_det_MUON_TRG=DONE%2C0&pbf_bm=STABLE%2C&prsf_rlp=$period%2C"

which xdg-open > /dev/null 2>&1
if [[ $? == 0 ]]; then
  xdg-open "$logbookUrl"
else
  open "$logbookUrl"
fi

touch $runListLogbook
open "$runListLogbook"
answer="n"
while [ "$answer" != "y" ]; do
  echo "Please write the run list from the logbook in $runListLogbook"
  echo "Press y when done"
  read answer
done


##### If old tex file existed, update summary and run-by-run information
if [ -e "$oldTexFile" ]; then
  # Use the summary and ending from the existing file
  sep1='frametitle{Run summary'
  sep2='frametitle{Hardware issues}'

  summary="summary_$texFile"
  cat $oldTexFile | sed -n "1,/$sep1/p" | grep -v "$sep1" > $summary
  body="body_$texFile"
  cat $texFile | sed -n "/$sep1/,/$sep2/p" | grep -v "$sep2" > $body
  ending="end_$texFile"
  cat $oldTexFile | sed -n "/$sep2/,/\end{document}/p" > $ending

  cat $summary $body $ending > $texFile
  rm $summary $body $ending

# Update summary run-by-run info
  runListQAstr=$(cat $runListQA | xargs)
  changeCommand=""
  sedCut='s/^[[:space:]]*//;s/\\/\\\\/g;s/\[/\\\[/g;s/\]/\\\]/g;s/\?/\\\?/'
  for irun in $runListQAstr; do
    infoRun=$(grep "runTab" $oldTexFile | grep $irun | grep -v "{}" | sed "$sedCut")
    if [ "$infoRun" != "" ]; then
      genRun=$(grep "runTab" $texFile | grep $irun | sed "$sedCut")
      changeCommand="${changeCommand}s/$genRun/$infoRun/;"
    fi
  done
  #  echo "$changeCommand"
  sed -i "" "$changeCommand" $texFile

#  mv $tmpTexFile $generatedTexFile
#  diff --changed-group-format='%<' --old-group-format='%<' --new-group-format='%>' $oldTexFile $generatedTexFile > $tmpTexFile
fi

inQAandLogbook=`comm -12 <(sort $runListQA) <(sort $runListLogbook) | xargs`
onlyInQA=`comm -23 <(sort $runListQA) <(sort $runListLogbook) | xargs`
onlyInLogbook=`comm -13 <(sort $runListQA) <(sort $runListLogbook) | xargs`


##### Change color of table lines for runs not in the logbook
changeCommand=""
for irun in $onlyInQA; do
changeCommand="${changeCommand}s/runTab{$irun}/runTab[\\\notInLogColor]{$irun}/;"
done
sed -i "" "$changeCommand" $texFile


compileTexLog="pdflatex.log"
echo ""
echo "Compiling latex:"
echo "pdflatex $texFile (output in $compileTexLog) ..."
pdflatex $texFile > $compileTexLog
pdflatex $texFile >> $compileTexLog

open $texFile

echo ""
echo "Common runs (`echo ${inQAandLogbook} | wc -w | xargs`) :"
echo "${inQAandLogbook}" | xargs
echo ""
echo "only in QA (`echo ${onlyInQA} | wc -w | xargs`) :"
echo "${onlyInQA}" | xargs
echo ""
echo "only in logbook (`echo ${onlyInLogbook} | wc -w | xargs`) :"
echo "${onlyInLogbook}" | xargs

#if [ -e "$oldTexFile" ]; then
#  echo ""
#  echo "Please check that the old tex file ($oldTexFile) and the generated one ($generatedTexFile)"
#  echo "were properly merged:"
#  echo ""
#  echo "diff $oldTexFile $texFile : "
#  diff $oldTexFile $texFile
#  echo ""
#  echo "diff $generatedTexFile $texFile :"
#  diff $generatedTexFile $texFile
#  echo ""
#  echo "Remove $generatedTexFile $oldTexFile? [y/n]"
#  read answer
#  if [ "$answer" = "y" ]; then
#    rm $generatedTexFile $oldTexFile
#  fi
#fi

cd $baseLocalDir

