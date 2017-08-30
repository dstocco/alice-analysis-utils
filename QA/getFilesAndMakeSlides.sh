#!/bin/bash

baseRemoteDir="http://aliqamu.web.cern.ch/aliqamu"
baseRemoteDirEVS="http://aliqaevs.web.cern.ch/aliqaevs"
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


subDir="$1"
texFile="muonQA.tex"
backupTexFile="$texFile.backup"
runListQA="runListQA.txt"
runListLogbook="runListLogbook.txt"

function MakeDir()
{
  local dirName="$1"
  if [ ! -d "$dirName" ]; then
    mkdir -p "$dirName"
  fi
}

function GetPeriod()
{
  echo "$(basename "$(dirname $subDir)")"
}

function GetPass()
{
  echo "$(basename $subDir)"
}

function SetupDir()
{
  if [ -z "$subDir" ]; then
    echo "Please enter specific: dataType/year/period/pass"
    read subDir
  fi
  subDir=${subDir#\/}
}


function GetRemoteFile()
{
  local inFile=$1
  local outFile=$2

  if [ ${inFile:0:4} = "http" ]; then
    if [ -e $outFile ]; then
      # Download only if local file is older than remote file
      curl -f -z "$outFile" -R -o "$outFile" "$inFile"
    else
      curl -f -R -o "$outFile" "$inFile"
    fi
  else
    rsync -avu "$inFile" "$outFile"
  fi

  if [ ! -e $outFile ]; then
    echo "Problems in downloading file $inFile"
    return 1
  fi

  return 0
}


function GetRemoteQAFiles()
{
  #### Syncronize with remote directory

  local baseRemote="$1"
  local relPath="$2"

  local inputDir="${baseRemote}/$relPath"
  GetRemoteFile "$inputDir/QA_muon_tracker.root" "$relPath/QA_muon_tracker.root"
  GetRemoteFile "$inputDir/QA_muon_trigger.root" "$relPath/QA_muon_trigger.root"

  if [ $? -eq 1 ]; then
    return 1
  fi

  GetRemoteFile "${baseRemoteDirEVS}/$relPath/trending.root" "$relPath/trending_evs.root"

  return 0
}


function BackupLatex()
{
  if [ -e "$texFile" ]; then
    echo "Updating existing latex file"
    mv $texFile $backupTexFile
#  if [ $forceLatex -eq 0 ]; then
#    echo "Latex file already present. It will not be re-created"
#  else
#    rm $texFile
#  fi
  fi
}


function MakeQASlides() {

  if [ -z $ALICE_PHYSICS ]; then
    echo "Please set ALICE_PHYSICS environement"
    return 1
  fi

#  BackupLatex
  local period
  period="$(GetPeriod)"
  local pass
  pass="$(GetPass)"

  ##### Ask for a list of triggers
  if [ -z $triggerList ]; then
    if [ -e "$texFile" ]; then
      triggerList=$(grep "%TriggerList=" $texFile | cut -d "=" -f2)
    fi
    if [ -z $triggerList ]; then
#      open "QA_muon_tracker.pdf"
      echo "Trigger list (comma separated) for $PWD"
      echo "(e.g. CINT7-B-NOPF-MUFAST,CMSL7-B-NOPF-MUFAST,CMSH7-B-NOPF-MUFAST,CMUL7-B-NOPF-MUFAST )"
      read triggerList
    fi
  fi

  root -b -q $ALICE_PHYSICS/PWGPP/MUON/lite/MakeSlides.C+\(\"$period\"\,\"$pass\",\"$triggerList\",\""$authors"\",\"QA_muon_tracker.root\",\"QA_muon_trigger.root\",\"trending_evs.root\",\"$texFile\"\)
#  issueFound=$(grep "page=-1" $texFile)
#  if [ "$issueFound" != "" ]; then
#    echo "Problem in $texFile : one page was not found :"
#    echo "$issueFound"
#    echo "Please check the file and fix the issue (e.g. by removing the slide)"
#    open $texFile
#    answer="n"
#    while [ "$answer" != "y" ]; do
#      echo "Press y when done"
#      read answer
#    done
#  fi

  if [ ! -e "$texFile" ]; then
    echo "Something wrong: latex file not produced..."
    if [ -e $backupTexFile ]; then
      mv $backupTexFile $texFile
    fi
    return 1
  fi
  return 0
}


function MakeRunListQA()
{
  ##### Get analyzed runs from QA
  grep "runTab" $texFile | grep -oE "{[0-9]+}" | sed 's/{//;s/}//' > $runListQA
}

function GetRunListQA()
{
  local redo="$1"
  if [[ ! -e $runListQA || $redo -eq 1 ]]; then
    MakeRunListQA
  fi
  cat $runListQA
}


function MakeRunListLogbook()
{
  ##### Get run list from logbook
  local period="$1"
  local logbookUrl="https://alice-logbook.cern.ch/logbook/date_online.php?p_cont=sb&p_rspn=1&p_rsob=l.run&p_rsob_dir=DESC&ptcf_rtc=%2CAt+least%2CMUON_TRG%3BMUON_TRK%3B%2CAt+least&prsf_rtype=PHYSICS%2C&prsf_rdur=10+m%2C%2C&pqff_det_MUON_TRK=Bad+run%2C1&pqff_det_MUON_TRG=Bad+run%2C1&prsf_rgmr=Yes&pqff_rq=Bad+run%2C1&psf_sd=Yes&psf_det_GLOBAL=DONE%2C0&psf_det_MUON_TRK=DONE%2C0&psf_det_MUON_TRG=DONE%2C0&pbf_bm=STABLE%2C&prsf_rlp=$period%2C"

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
}


function SummaryFromExistingSlides()
{
  ##### If old tex file existed, update summary and run-by-run information
  if [ ! -e "$backupTexFile" ]; then
    return 1;
  fi
  # Use the summary and ending from the existing file
  local sep1='frametitle{Run summary'
  local sep2='frametitle{Hardware issues}'

  local summary="summary_$texFile"
  cat $backupTexFile | sed -n "1,/$sep1/p" | grep -v "$sep1" > $summary
  local body="body_$texFile"
  cat $texFile | sed -n "/$sep1/,/$sep2/p" | grep -v "$sep2" > $body
  local ending="end_$texFile"
  cat $backupTexFile | sed -n "/$sep2/,/\end{document}/p" > $ending

  cat $summary $body $ending > $texFile
  rm $summary $body $ending

  # Update summary run-by-run info
  if [ ! -e $runListQA ]; then
    MakeRunListQA
  fi
  local runListQAstr
  runListQAstr=$(cat $runListQA | xargs)
  local changeCommand=""
  sedCut='s/^[[:space:]]*//;s/\\/\\\\/g;s/\[/\\\[/g;s/\]/\\\]/g;s/\?/\\\?/;s:/:\\/:g;'
  for irun in $runListQAstr; do
    infoRun=$(grep "runTab" $backupTexFile | grep $irun | grep -v "{}" | sed "$sedCut")
    if [ "$infoRun" != "" ]; then
      genRun=$(grep "runTab" $texFile | grep $irun | sed "$sedCut")
      changeCommand="${changeCommand}s/$genRun/$infoRun/;"
      if [ ${#changeCommand} -gt 2000 ]; then
# Apparently there is a limit to the length of the command sed can get...
# We hence run sed when the string starts to become long
        sed  -i '' "$changeCommand" $texFile
        changeCommand=""
      fi
    fi
  done
  if [ ${#changeCommand} -gt 1 ]; then
    sed -i '' "$changeCommand" $texFile
  fi

#  mv $tmpTexFile $generatedTexFile
#  diff --changed-group-format='%<' --old-group-format='%<' --new-group-format='%>' $backupTexFile $generatedTexFile > $tmpTexFile
}


function UpdateWithLogbookInfo()
{
  ##### Change color of table lines for runs not in the logbook
  if [ ! -e $runListLogbook ]; then
    MakeRunListLogbook
  fi
  if [ ! -e $runListQA ]; then
    MakeRunListQA
  fi

  local onlyInQA onlyInLogbook
  onlyInQA=$(comm -23 <(sort $runListQA) <(sort $runListLogbook) | xargs)
  onlyInLogbook=$(comm -13 <(sort $runListQA) <(sort $runListLogbook) | xargs)

  if [[ -z "$onlyInLogbook" && -z "$onlyInQA" ]]; then
    return 0;
  fi

  awk -v logOnly="${onlyInLogbook}" -v qaOnly="${onlyInQA}" '
    BEGIN {
      isSummarySlide=0
      isRunSummary=0
      matchString="In e-logbook, not yet in QA:"
      outString=matchString " " logOnly
      foundLogSummary=0
      nQAonly=split(qaOnly,qaOnlyArr," ")
    }
    {
      if ( index($0,"\\frametitle\{Summary") ) isSummarySlide=1
      else if ( index($0,"\\frametitle\{Run summary") ) isRunSummary=1
      isEnd = index($0,"\\end{frame}")
      output=$0

      if ( isSummarySlide==1 ) {
        if ( foundLogSummary == 0 ) {
          if ( index($0,matchString) || isEnd ) {
            print outString
            foundLogSummary=1
            if ( ! isEnd ) next
          }
        }
      }
      else if ( isRunSummary==1 ) {
        for ( irun=1; irun<=nQAonly;irun++ ) {
          if ( index($0,qaOnlyArr[irun]) ) sub("runTab{","runTab[\\notInLogColor]{");
        }
      }
      if ( isEnd ) {
        isSummarySlide=0
        isRunSummary=0
      }
      print $0;
    } ' $texFile > $texFile.tmp
  mv $texFile.tmp $texFile
}



function PrintRunSummary()
{
  if [ ! -e $runListLogbook ]; then
    MakeRunListLogbook
  fi
  if [ ! -e $runListQA ]; then
    MakeRunListQA
  fi

  local inQAandLogbook onlyInQA onlyInLogbook
  inQAandLogbook=$(comm -12 <(sort $runListQA) <(sort $runListLogbook) | xargs)
  onlyInQA=$(comm -23 <(sort $runListQA) <(sort $runListLogbook) | xargs)
  onlyInLogbook=$(comm -13 <(sort $runListQA) <(sort $runListLogbook) | xargs)

  echo ""
  echo "Common runs (`echo ${inQAandLogbook} | wc -w | xargs`) :"
  echo "${inQAandLogbook}" | xargs
  echo ""
  echo "only in QA (`echo ${onlyInQA} | wc -w | xargs`) :"
  echo "${onlyInQA}" | xargs
  echo ""
  echo "only in logbook (`echo ${onlyInLogbook} | wc -w | xargs`) :"
  echo "${onlyInLogbook}" | xargs
}


function CompileLatex()
{
  local compileTexLog="pdflatex.log"
  echo ""
  echo "Compiling latex:"
  echo "pdflatex $texFile (output in $compileTexLog) ..."
  pdflatex $texFile > $compileTexLog
  pdflatex $texFile >> $compileTexLog

  open $texFile
}

function main() {
  local baseLocalDir="$PWD"
  SetupDir

  local localDir="$baseLocalDir/$subDir"
  MakeDir "$subDir"

  GetRemoteQAFiles "$baseRemoteDir" "$subDir"
  if [ $? -ne 0 ]; then
    cd $baseLocalDir || return 2
    return 1
  fi
  cd $localDir || return 2
  MakeQASlides
  if [ $? -ne 0 ]; then
    cd $baseLocalDir || return 2
    return 1
  fi

  cd $localDir || return 2
  MakeRunListQA
  MakeRunListLogbook "$(GetPeriod)"

#  SummaryFromExistingSlides
  UpdateWithLogbookInfo

  CompileLatex

  PrintRunSummary

  cd $baseLocalDir || return 2
  return 0
}

main
