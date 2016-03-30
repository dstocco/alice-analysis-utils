//--------------------------------------------------------------------------
// The macro contains a series of utility methods to run task on grid or proof
//
// runMode:
//   test, full, offline, merge/terminate
//     NB: merge -> via JDL   -   terminate -> locally
//
// analysisMode:
//   libs    -> only load libraries and return
//   local   -> local inputs
//                 Needs as input either
//                 - the file path of AliESDs.root or
//                 - a txt file with the list of AliESDs.root (one per line)
//   proof, grid, mix, grid terminate  -> manager mode
//
//                 proof needs a dataset as input
//
// N.B.: To perform the TERMINATE step only (without merging):
// runMode = "terminate" && analysisMode = "terminate grid"
//--------------------------------------------------------------------------

#if !defined(__CINT__) || defined(__MAKECINT__)

#define TESTCOMPILATION

#include <Riostream.h>

// ROOT includes
#include "TString.h"
#include "TStopwatch.h"
#include "TSystem.h"
#include "TProof.h"
#include "TGrid.h"
#include "TChain.h"
#include "TROOT.h"
#include "TFile.h"
#include "TEnv.h"
#include "TObjArray.h"
#include "TObjString.h"
#include "TMap.h"
#include "TDatime.h"
#include "TPRegexp.h"
#include "TFileCollection.h"
#include "TApplication.h"

// STEER includes
#include "AliESDInputHandler.h"
#include "AliAODInputHandler.h"
#include "AliAODHandler.h"
#include "AliMCEventHandler.h"
#include "AliMultiInputEventHandler.h"

// ANALYSIS includes
#include "AliAnalysisManager.h"
#include "AliAnalysisTaskSE.h"
#include "AliAnalysisAlien.h"
#endif

//_______________________________________________________
void PrintOptions()
{
  /// Print recognised options
  printf("\nList of recognised options:\n");
  printf("  runMode: test full merge terminate\n");
  printf("  analysisMode: local grid saf saf2 vaf terminateonly\n");
  printf("  inputName: <runNumber> <fileWithRunList> <rootFileToAnalyse(absolute path)>\n");
  printf("  inputOptions: Data/MC FULL/EMBED AOD/ESD <period> <pass> <dataPattern> <dataDir>\n");
  printf("  softVersions: aliphysics=version,aliroot=version,root=version\n");
  printf("  analysisOptions: NOPHYSSEL CENTR OLDCENTR MIXED SPLIT\n");
}

TString GetPodOutDir();

//_______________________________________________________
TString GetProofInfo ( TString info, TString analysisMode )
{
  TString proofCluster = "", proofServer = "", copyCommand = "", openCommand = "", execCommand = "", datasetMode = "", passCommand = "";
  info.ToLower();

  TString runPodCommand = Form("\"%s/runPod.sh nworkers\"",GetPodOutDir().Data());

  TString userName = gSystem->Getenv("alice_API_USER");
  userName.Append("@");
  if ( analysisMode == "saf2" ) {
    proofCluster = "nansafmaster2.in2p3.fr";
    proofCluster.Prepend(userName.Data());
    proofServer = proofCluster;
  }
  else if ( analysisMode == "saf" ) {
    proofCluster = "pod://";
    proofServer = "nansafmaster3.in2p3.fr";
    copyCommand = "rsync -avcL -e 'gsissh -p 1975'";
    openCommand = Form("gsissh -p 1975 -t %s",proofServer.Data());
    execCommand = Form("/opt/SAF3/bin/saf3-enter \"\" %s",runPodCommand.Data());
    datasetMode = "cache";
    passCommand = "\"\"";
  }
  else if ( analysisMode == "vaf" ) {
    Int_t lxplusTunnelPort = 5501;
    proofCluster = "pod://";
    proofServer = "localhost";
    copyCommand = Form("rsync -avcL -e 'ssh -p %i'",lxplusTunnelPort);
    openCommand = Form("ssh %slocalhost -p %i -t",userName.Data(),lxplusTunnelPort);
    execCommand = Form("echo %s | /usr/bin/vaf-enter",runPodCommand.Data());
    datasetMode = "remote";
  }
  else if ( analysisMode == "test" || analysisMode == "prooflite" ) {
    proofCluster = "";
    proofServer = "localhost";
  }

  if ( info == "proofcluster" ) return proofCluster;
  else if ( info == "proofserver" ) return proofServer;
  else if ( info == "copycommand" ) return copyCommand;
  else if ( info == "opencommand" ) return openCommand;
  else if ( info == "execcommand" ) return execCommand;
  else if ( info == "datasetmode" ) return datasetMode;
  else printf("Error: option %s not recognised\n",info.Data());
  return "";
}

//_______________________________________________________
TString GetMode ( TString runMode, TString analysisMode )
{
  if ( analysisMode == "grid" ) return "grid";
  if ( runMode.Contains("terminate") || analysisMode.Contains("terminate") ) return "terminateonly";
  if ( analysisMode.Contains("local") ) return "local";
  if ( analysisMode == "proof" || analysisMode == "saf" || analysisMode == "saf2" || analysisMode == "vaf" ) return "proof";
  return "";
}

//_______________________________________________________
Bool_t IsPod ( TString analysisMode )
{
  TString proofCluster = GetProofInfo("proofcluster",analysisMode);
  return proofCluster.BeginsWith("pod");
}

//_______________________________________________________
Bool_t IsPodMachine ( TString analysisMode )
{
  TString proofServer = GetProofInfo("proofserver",analysisMode);
//  TString hostname = gSystem->Getenv("HOSTNAME");
  TString hostname = gSystem->GetFromPipe("hostname");
  return ( proofServer == hostname || hostname.BeginsWith("alivaf") );
}

//_______________________________________________________
TString GetPodOutDir()
{
  return "taskDir";
}


//_______________________________________________________
TString GetDatasetName()
{
  return "dataset.txt";
}

//_______________________________________________________
Bool_t PerformAction ( TString command, Bool_t& yesToAll )
{

  TString decision = "y";

  if ( gROOT->IsBatch() ) yesToAll = kTRUE; // To run with crontab

  if ( ! yesToAll ) {
    printf("%s ? [y/n/a]\n", command.Data());
    cin >> decision;
  }

  Bool_t goOn = kFALSE;

  if ( ! decision.CompareTo("y") )
  goOn = kTRUE;
  else if ( ! decision.CompareTo("a") ) {
    yesToAll = kTRUE;
    goOn = kTRUE;
  }

  if ( goOn ) {
    printf("Executing: %s\n", command.Data());
    gSystem->Exec(command.Data());
  }

  return goOn;
}

//_______________________________________________________
TString GetRunMacro ( )
{
  TString rootCmd = gSystem->GetFromPipe("tail -n 1 $HOME/.root_hist");
  rootCmd.ReplaceAll("  "," ");
  rootCmd.ReplaceAll(".x ","");
  rootCmd.Remove(TString::kLeading,' ');
  rootCmd.Remove(TString::kTrailing,' ');
  return rootCmd;
}

//_______________________________________________________
TString GetSoftVersion ( TString softType, TString softVersions )
{
  // Format for softVersions: aliphysics=version,aliroot=version,root=version

  TString selected = "";
  softVersions.ReplaceAll(" ","");
  if ( ! softVersions.Contains("aliphysics") ) {
    if ( ! softVersions.Contains("aliroot") ) {
      if ( ! softVersions.IsNull() ) softVersions.Append(",");
      TDatime dt;
      softVersions.Append(Form("aliphysics=vAN-%i-1",dt.GetDate()-1));
    }
  }

  TObjArray* alirootRootArr = softVersions.Tokenize(",");
  for ( Int_t iarr=0; iarr<alirootRootArr->GetEntries(); iarr++ ) {
    TString currVer = alirootRootArr->At(iarr)->GetName();
    if ( ! currVer.Contains(softType.Data(),TString::kIgnoreCase) ) continue;
    if ( currVer.Contains("=") ) {
      TObjArray* currVerArr = currVer.Tokenize("=");
      selected = currVerArr->At(1)->GetName();
      delete currVerArr;
      break;
    }
  }
  delete alirootRootArr;

  return selected;
}

//______________________________________________________________________________
TString GetFileType ( TString checkString )
{
  if ( ! gSystem->AccessPathName(checkString) && ! checkString.EndsWith(".root") ) {

    // The input is a file containing the list of files/datasets

    ifstream inFile(checkString.Data());
    TString currLine = "";

    if (inFile.is_open()) {
      while ( ! inFile.eof() ) {
        currLine.ReadLine(inFile,kFALSE);
        currLine.ReplaceAll(" ","");
        if ( currLine.IsNull() ) continue;
        checkString = currLine;
        break;
      }
      inFile.close();
    }
  }

  if ( checkString.Contains("AliESD") ) return "ESD";
  if ( checkString.Contains("AliAOD") ) return "AOD";

  Bool_t isESD = checkString.Contains("ESD");
  Bool_t isAOD = checkString.Contains("AOD");
  if ( isESD && ! isAOD ) return "ESD";
  else if ( isAOD && ! isESD ) return "AOD";

  return "";
}

//______________________________________________________________________________
TString GetFileType ( TString inputName, TString inputOptions )
{
  TString fileType = GetFileType(inputName);
  if ( fileType.IsNull() ) fileType = GetFileType(inputOptions);
  return fileType;
}

//______________________________________________________________________________
Bool_t IsMC ( TString inputOptions )
{
  return ( inputOptions.Contains("MC") || inputOptions.Contains("EMBED") );
}

//______________________________________________________________________________
Bool_t IsEMBED( TString inputOptions )
{
  return inputOptions.Contains("EMBED");
}

//______________________________________________________________________________
TString GetGridQueryVal ( TString queryString, TString keyword )
{
  TString found = "";
  TObjArray* arr = queryString.Tokenize(";");
  for ( Int_t iarr=0; iarr<arr->GetEntries(); iarr++ ) {
    TString currPart = arr->At(iarr)->GetName();
    if ( currPart.Contains(keyword.Data()) ) {
      TObjArray* auxArr = currPart.Tokenize("=");
      if ( auxArr->GetEntries() == 2 ) found = auxArr->At(1)->GetName();
      delete auxArr;
      break;
    }
  }
  delete arr;
  if ( found.IsNull() ) printf("Warning: cannot find %s in %s\n",keyword.Data(),queryString.Data());
  return found;
}

//______________________________________________________________________________
TString GetRunNumber ( TString queryString )
{
  TString found = "";
  queryString.ReplaceAll("*",""); // Add protection for datasets with a star inside
  for ( Int_t ndigits=9; ndigits>=6; ndigits-- ) {
    TString sre = "";
    for ( Int_t idigit=0;idigit<ndigits; idigit++ ) sre += "[0-9]";
    found = queryString(TRegexp(sre.Data()));
    if ( ! found.IsNull() ) break;
  }
  return found;
//  TString found = "";
//  TObjArray* arr = queryString.Tokenize("/");
//  for ( Int_t iarr=0; iarr<arr->GetEntries(); iarr++ ) {
//    TString currPart = arr->At(iarr)->GetName();
//    if ( currPart.IsDigit() && currPart.Length() >=6 && currPart.Length() <=9 ) {
//      found = currPart;
//      break;
//    }
//  }
//  delete arr;
//  return found;
}

//______________________________________________________________________________
TString GetDataDir ( TString queryString )
{
  TString basePath = GetGridQueryVal(queryString,"BasePath");
  TString runNum = GetRunNumber(queryString);
  if ( basePath.IsNull() ) return "";
  Int_t idx = basePath.Index(runNum);
  basePath.Remove(idx-1);
  return basePath;
}

//______________________________________________________________________________
TString GetDataPattern ( TString queryString )
{
  TString basePath = GetGridQueryVal(queryString,"BasePath");
  TString fileName = GetGridQueryVal(queryString,"FileName");
  TString runNum = GetRunNumber(queryString);
  if ( basePath.IsNull() || fileName.IsNull() ) return "";
  Int_t idx = basePath.Index(runNum) + runNum.Length();
  basePath.Remove(0,idx+1);
  if ( ! basePath.EndsWith("/") ) basePath.Append("/");
  basePath += Form("*%s",fileName.Data());
  return basePath;
}


//______________________________________________________________________________
TString GetPeriod ( TString checkString )
{
  return checkString(TRegexp("LHC[0-9][0-9][a-z]"));
}

//______________________________________________________________________________
TString GetPeriod ( TString inputName, TString inputOptions )
{
  TString period = GetPeriod(inputOptions);
  if ( ! period.IsNull() ) return period;
  if ( gSystem->AccessPathName(inputName) || inputName.EndsWith(".root") ) period = GetPeriod(inputName);
  else {
    TString currLine = "";
    ifstream inFile(inputName.Data());
    while (!inFile.eof()) {
      currLine.ReadLine(inFile);
      period = GetPeriod(currLine);
      if ( ! period.IsNull() ) break;
    }
    inFile.close();
  }
  return period;
}


//_______________________________________
Bool_t HasPassInfo ( TString checkString )
{
  return ( checkString.Contains("pass") || checkString.Contains("muon_calo") );
}

//______________________________________________________________________________
TString GetPass ( TString checkString )
{
  TString found = "";
  if ( ! HasPassInfo(checkString) ) return found;

  TObjArray* optList = checkString.Tokenize(" ");
  for ( Int_t iopt=0; iopt<optList->GetEntries(); iopt++ ) {
    TString currStr = optList->At(iopt)->GetName();
    if ( HasPassInfo(currStr) ) {
      TObjArray* arr = currStr.Tokenize("/");
      for ( Int_t iarr=0; iarr<arr->GetEntries(); iarr++ ) {
        TString checkStr = arr->At(iarr)->GetName();
        if ( HasPassInfo(checkStr) ) {
          checkStr.ReplaceAll("*","");
          found = checkStr;
          break;
        }
      }
      delete arr;
    }
    if ( ! found.IsNull() ) break;
  }
  delete optList;
  return found;
}

//______________________________________________________________________________
TString GetPass ( TString inputName, TString inputOptions )
{
  TString pass = GetPass(inputOptions);
  if ( ! pass.IsNull() ) return pass;
  if ( gSystem->AccessPathName(inputName) ) pass = GetPass(inputName);
  else {
    TString currLine = "";
    ifstream inFile(inputName.Data());
    while (!inFile.eof()) {
      currLine.ReadLine(inFile);
      pass = GetPass(currLine);
      if ( ! pass.IsNull() ) break;
    }
    inFile.close();
  }
  return pass;

}

//______________________________________________________________________________
Bool_t IsAOD ( TString inputName, TString inputOptions )
{
  TString fileType = GetFileType(inputName,inputOptions);
  return ( fileType == "AOD" );
}

//______________________________________________________________________________
Bool_t IsESD ( TString inputName, TString inputOptions )
{
  TString fileType = GetFileType(inputName,inputOptions);
  return ( fileType == "ESD" );
}

//_______________________________________
Bool_t AddInfo ( TString key, TString value, TMap* map, Bool_t keepExisiting = kFALSE )
{
  //  printf("Adding %s\n", value.Data()); // REMEMBER TO CUT
  if ( value.IsNull() ) return kFALSE;
  if ( map->GetValue(key.Data()) ) {
    if ( keepExisiting ) return kFALSE;
    static_cast<TObjString*>(map->GetValue(key.Data()))->SetString(value);
  }
  else {
    map->Add(new TObjString(key),new TObjString(value));
  }
  return kTRUE;
}

//______________________________________________________________________________
TMap* ParseInfo ( TString inputName, TString inputOptions, TString analysisOptions )
{
  TMap* map = new TMap();
  map->SetOwner();

  // Check data type (DATA/MC)
  TString dataType = "DATA";
  if ( IsMC(inputOptions) ) dataType = "MC";
  AddInfo("dataType",dataType,map);

  TString dataTypeDetails = "FULL";
  if ( inputOptions.Contains("EMBED") ) dataTypeDetails = "EMBED";
  AddInfo("mcDetails",dataTypeDetails,map);

  // Check file tyoe (ESD/AOD)
  TString fileType = GetFileType(inputName,inputOptions);
  AddInfo("fileType",fileType,map);

  // Set data inputs (period, dataPattern, pass, dataDir)
  TString period = GetPeriod(inputName,inputOptions);
  if ( period.IsNull() ) period = "UNKNOWN";
  AddInfo("period",period,map);


  AddInfo("physicsSelection",analysisOptions.Contains("NOPHYSSEL")?"NO":"YES",map);
  TString centr = "NO";
  if ( analysisOptions.Contains("OLDCENTR") ) centr = "OLD";
  else if ( analysisOptions.Contains("CENTR") ) centr = "YES";

  AddInfo("centrality",centr,map);

  return map;
}

//_______________________________________________________
Bool_t CopyAdditionalFilesLocally ( TString additionalFile, Bool_t warnOnMissing = kTRUE )
{
  /// Space separated list of files
  TString outDir = "."; //Form("%s/%s",gSystem->pwd(),GetOutDirName().Data());
  Bool_t yesToAll = kTRUE;
  Bool_t isOk = kTRUE;
  TObjArray* arr = additionalFile.Tokenize(" ");
  for ( Int_t iarr=0; iarr<arr->GetEntries(); iarr++ ) {
    TString currFile = arr->At(iarr)->GetName();
    gSystem->ExpandPathName(currFile);
    if ( gSystem->AccessPathName(currFile) ) {
      if ( warnOnMissing ) {
        printf("Error: could not copy %s\n", currFile.Data());
        isOk = kFALSE;
      }
    }
    else PerformAction(Form("cp -p %s %s/",currFile.Data(),outDir.Data()), yesToAll);
  }
  delete arr;
  return isOk;
}

//_______________________________________________________
Bool_t CopyDatasetLocally ( TString inputName, TString analysisMode )
{
  /// Copy dataset locally
  TString inFilename = inputName;
  TString tmpFilename = "tmp_dataset.txt";
  if ( gSystem->AccessPathName(inputName) ) {
    inFilename = tmpFilename;
    gSystem->Exec(Form("echo '%s' > %s",inputName.Data(),inFilename.Data()));
  }
  else if ( inputName.EndsWith(".root") ) return kFALSE;

//  ofstream outFile(Form("%s/%s",GetOutDirName().Data(),GetDatasetName().Data()));
  ofstream outFile(GetDatasetName().Data());
  ifstream inFile(inFilename.Data());
  TString currLine = "";
  TString datasetMode = GetProofInfo("datasetmode",analysisMode);
  while ( ! inFile.eof() ) {
    currLine.ReadLine(inFile);
    if ( currLine.IsNull() ) continue;
    if ( currLine.Contains("Find;") ) {
      Int_t index = currLine.Index(";Find");
      TObjArray findCommands;
      findCommands.SetOwner();
      while ( index >= 0 ) {
        TString currDataset = currLine;
        currDataset.Remove(0,index);
        currLine.Remove(index);
        findCommands.Add(new TObjString(currDataset));
        index = currDataset.Index(";Find");
      }
      findCommands.Add(new TObjString(currLine));
      for ( Int_t iarr=0; iarr<findCommands.GetEntries(); iarr++ ) {
        TString currFind = findCommands.At(iarr)->GetName();
        Int_t index = currFind.Index("Mode=");
        if ( index>=0 ) {
          TString currMode = currFind;
          currMode.Remove(0,index+5);
          index = currMode.Index(";");
          if ( index>=0 ) currMode.Remove(index);
          currFind.ReplaceAll(currMode.Data(),datasetMode.Data());
        }
        else currFind.Append(Form(";Mode=%s;",datasetMode.Data()));
        currFind.ReplaceAll("Mode=;","");
        currFind.ReplaceAll(";;",";");
        outFile << currFind.Data() << endl;
      }
    }
    else outFile << currLine.Data() << endl;
  }
  inFile.close();
  outFile.close();

  if ( gSystem->AccessPathName(tmpFilename) == 0 ) gSystem->Exec(Form("rm %s",tmpFilename.Data()));

  return kTRUE;
}

//_______________________________________________________
Bool_t WorkDir ( TString runMode, TString analysisMode, TString workDir )
{
  /// Make working dir if needed and cd into it

  TString sMode = GetMode(runMode,analysisMode);
  if ( gSystem->AccessPathName("./runTaskUtilities.C") == 0 ) {
    if ( sMode != "terminateonly" ) {
      printf("Found runTaskUtilities.C in the current working directory\n");
      printf("Assume you want to re-run local: do not copy files\n");
    }
    return kFALSE;
  }

  if ( workDir.IsNull() ) {
    workDir = "tmpDir";
    printf("No workdir specified: creating default %s\n",workDir.Data());
  }

  Bool_t yesToAll = kFALSE;
  TString currDir = gSystem->pwd();
  TString command = "";
  TString workDirFull = "";

  Bool_t makeDir = ( gSystem->AccessPathName(workDir) != 0 );
  if ( sMode == "terminateonly" ) {
    if ( makeDir ) {
      printf("Error: mode %s requires an existing workDir containing the analysis results!\n",sMode.Data());
      return kFALSE;
    }
  }
  else if ( ! makeDir ) {
    workDirFull = gSystem->GetFromPipe(Form("cd %s; pwd; cd %s",workDir.Data(),currDir.Data()));
    if ( workDirFull == currDir ) return kFALSE;
    printf("Workdir %s already exist:\n",workDir.Data());
    command = Form("rm -rf %s",workDir.Data());
    makeDir = PerformAction(command,yesToAll);
  }

  if ( makeDir ) {
    yesToAll = kTRUE;
    command = Form("mkdir %s",workDir.Data());
    PerformAction(command,yesToAll);
  }

  if ( workDirFull.IsNull() ) workDirFull = gSystem->GetFromPipe(Form("cd %s; pwd; cd %s",workDir.Data(),currDir.Data()));

  gSystem->cd(workDirFull.Data());

  return makeDir;
}

//_______________________________________________________
Bool_t CopyFilesLocally ( TString libraries, TString inputName, TString analysisMode, TString inDir )
{
  /// Space separated list of libraries, par files and classes

  TString aliceSrcDir = gSystem->ExpandPathName("$ALICE_PHYSICS");
  TString aliceBuildDir = gSystem->ExpandPathName("$ALICE_PHYSICS/../build");
  TString command = "";

  Bool_t yesToAll = kTRUE;

  CopyDatasetLocally(inputName,analysisMode);

  TString runMacro = GetRunMacro();
  runMacro.Remove(runMacro.Index("("));
  runMacro.ReplaceAll("+","");
  if ( ! runMacro.BeginsWith("/") ) runMacro.Prepend(Form("%s/",inDir.Data()));

  CopyAdditionalFilesLocally(runMacro);

//  TString outDir = Form("%s/%s",currDir.Data(),GetOutDirName().Data());
//  if ( gSystem->AccessPathName(outDir) ) {
//    if ( runMode == "terminate" || runMode == "merge" ) {
//      if ( gSystem->AccessPathName(outDir) ) {
//        printf("Error: no output directory %s found\n",GetOutDirName().Data());
//        return kFALSE;
//      }
//    }
//    else PerformAction(Form("mkdir %s",outDir.Data()),yesToAll);
//  }



//  else if ( ! gSystem->AccessPathName(outDir) ) {
//    command = Form("rm -rf %s",outDir.Data());
//    PerformAction(command,yesToAll);
//  }

  TString workDirFull = gSystem->pwd(); // Assume we are in the working directory

  TObjArray* libList = libraries.Tokenize(" ");
  TString currName = "";
  Bool_t isOk = kTRUE;
  for ( Int_t ilib=0; ilib<libList->GetEntries(); ilib++) {
    currName = libList->At(ilib)->GetName();
    if ( ! currName.BeginsWith("/") ) currName.Prepend(Form("%s/",inDir.Data()));
    if ( currName.EndsWith(".cxx") || currName.EndsWith(".C") ) {
      TObjArray arr(2);
      arr.SetOwner();
      arr.AddAt(new TObjString(currName),1);
      currName.ReplaceAll(".cxx",".h");
      arr.AddAt(new TObjString(currName),0);
      for ( Int_t ifile=0; ifile<arr.GetEntries(); ifile++ ) {
        currName = arr.At(ifile)->GetName();
        if ( gSystem->AccessPathName(currName.Data()) == 0 ) {
          TString dirName = gSystem->DirName(currName.Data());
          if ( dirName.IsNull() || dirName == "." ) continue;
          command = Form("cp -p %s %s/;", currName.Data(), workDirFull.Data());
        }
        else command = Form("className=`find %s -name %s`; cp -p $className %s/;", aliceSrcDir.Data(), gSystem->BaseName(currName.Data()), workDirFull.Data());
        if ( ! PerformAction(command, yesToAll) ) {
          printf("Error: could not create %s\n", currName.Data());
          isOk = kFALSE;
          break;
        }
        if ( currName.EndsWith(".C") ) break;
      }
    }
    else if ( currName.Contains(".par") ) {
      if ( gSystem->AccessPathName(currName.Data()) ) {
        TString baseName = gSystem->BaseName(currName.Data());
        command = Form("cd %s; make %s; cd %s; find %s -name %s -exec mv -v {} ./ \\;", aliceBuildDir.Data(), baseName.Data(), workDirFull.Data(), aliceBuildDir.Data(), baseName.Data());
        if ( ! PerformAction(command, yesToAll) ) {
          printf("Error: could not create %s\n", currName.Data());
          isOk = kFALSE;
          break;
        }

        // Fixes problem with OADB on proof:
        // the par file only contians the srcs
        // but if you want to access OADB object they must be inside there!
        if ( currName.Contains("OADB") ) {
          command = "tar -xzf OADB.par; rsync -avu --exclude=.svn --exclude=PROOF-INF.OADB $ALICE_ROOT/OADB/ OADB/; tar -czf OADB.par OADB";
          PerformAction(command, yesToAll);
        }
      }
    }
    if (! isOk ) break;
  } // loop on libs
  delete libList;

  return isOk;
}

//_______________________________________________________
Bool_t LoadLibsLocally ( TString libraries, TString includePaths )
{
  TObjArray* pathList = includePaths.Tokenize(" ");
  for ( Int_t ipath=0; ipath<pathList->GetEntries(); ipath++ ) {
    TString currName = pathList->At(ipath)->GetName();
    gSystem->AddIncludePath(Form("-I%s",currName.Data()));
  }
  delete pathList;

  TObjArray* libList = libraries.Tokenize(" ");
  for ( Int_t ilib=0; ilib<libList->GetEntries(); ilib++) {
    TString currName = gSystem->BaseName(libList->At(ilib)->GetName());
    if ( currName.EndsWith(".so") ) gSystem->Load(currName.Data());
    else if ( currName.EndsWith(".cxx") ) gROOT->LoadMacro(Form("%s+g",currName.Data()));
    else if ( currName.EndsWith(".par") ) {
      currName.ReplaceAll(".par","");
      // The following line is needed since we use AliAnalysisAlien::SetupPar in this function.
      // The interpreter reads the following part line-by-line, even if the condition is not satisfied.
      // If the library is not loaded in advance, one gets funny issues at runtime
      TString foundLib = gSystem->GetLibraries("libANALYSISalice.so","",kFALSE);
      if ( foundLib.IsNull() ) gSystem->Load("libANALYSISalice.so");
      AliAnalysisAlien::SetupPar(currName);
    }
  }
  delete libList;
  return kTRUE;
}

//_______________________________________________________
Bool_t LoadAddTasks ( TString libraries )
{
  TObjArray* libList = libraries.Tokenize(" ");
  for ( Int_t ilib=0; ilib<libList->GetEntries(); ilib++) {
    TString currName = gSystem->BaseName(libList->At(ilib)->GetName());
    if ( currName.EndsWith(".C") ) gROOT->LoadMacro(currName.Data());
  }
  delete libList;
  return kTRUE;
}

//_______________________________________________________
Bool_t LoadLibsProof ( TString libraries, TString includePaths, TString aaf, TString softVersions, Bool_t notOnClient = kFALSE, TString alirootMode = "base" )
{
  aaf.ToLower();
  TString proofCluster = GetProofInfo("proofcluster",aaf);
  TString proofServer = GetProofInfo("proofserver",aaf);
  if ( proofCluster == proofServer ) {
    TString rootVersion = GetSoftVersion("root",softVersions);
    rootVersion.Prepend("VO_ALICE@ROOT::");
    TProof::Mgr(proofCluster.Data())->SetROOTVersion(rootVersion.Data());
  }
  TProof::Open(proofCluster.Data());

  if (!gProof) return kFALSE;

  TString extraIncs = "";
  TObjArray* pathList = includePaths.Tokenize(" ");
  for ( Int_t ipath=0; ipath<pathList->GetEntries(); ipath++ ) {
    TString currName = pathList->At(ipath)->GetName();
    if ( ! extraIncs.IsNull() ) extraIncs += ":";
    extraIncs += currName;
  }
  delete pathList;

  TString extraLibs = "";
  TObjArray extraPkgs, extraSrcs;
  extraPkgs.SetOwner();
  extraSrcs.SetOwner();
  TObjArray* libList = libraries.Tokenize(" ");
  for ( Int_t ilib=0; ilib<libList->GetEntries(); ilib++) {
    TString currName = libList->At(ilib)->GetName();
    if ( currName.EndsWith(".cxx") ) extraSrcs.Add(new TObjString(gSystem->BaseName(currName.Data())));
    else if ( currName.EndsWith(".par") ) extraPkgs.Add(new TObjString(currName));
    else if ( currName.EndsWith(".so") ) {
      if ( currName.BeginsWith("lib") ) currName.Remove(0,3);
      currName.ReplaceAll(".so","");
      if ( ! extraLibs.IsNull() ) extraLibs += ":";
      extraLibs += currName;
    }
  }
  delete libList;

  TList* list = new TList();
  list->Add(new TNamed("ALIROOT_MODE", alirootMode.Data()));
  list->Add(new TNamed("ALIROOT_EXTRA_LIBS", extraLibs.Data()));
  list->Add(new TNamed("ALIROOT_EXTRA_INCLUDES", extraIncs.Data()));
  if ( aaf != "saf") // Temporary fix for saf3: REMEMBER TO CUT this line when issue fixed
    list->Add(new TNamed("ALIROOT_ENABLE_ALIEN", "1"));
  TString mainPackage = "";
  if ( IsPod(aaf) ) {
    TString remotePar = ( aaf == "saf" ) ? "https://github.com/aphecetche/aphecetche.github.io/blob/master/saf/saf3/AliceVaf.par?raw=true" : "http://alibrary.web.cern.ch/alibrary/vaf/AliceVaf.par";
    mainPackage = gSystem->BaseName(remotePar.Data());
    mainPackage.Remove(mainPackage.Index("?"));
//    if ( aaf != "saf" || gSystem->AccessPathName(mainPackage) ) {
      printf("Getting package %s\n",remotePar.Data());
      TFile::Cp(remotePar.Data(), mainPackage.Data());
      if ( gSystem->AccessPathName(mainPackage) ) printf("Error: cannot get %s from %s\n",mainPackage.Data(),remotePar.Data());
//    }
//    else {
//    // In principle AliceVaf.par should be always taken from the webpage (constantly updated version)
//    // However, in SAF, one sometimes need to have custom AliceVaf.par
//    // Hence, if an AliceVaf.par is found in the local dir, it is used instead of the official one
//      printf("Using custom %s\n",mainPackage.Data());
//    }
  }
  else if ( proofServer == "localhost" ) mainPackage = "$ALICE_ROOT/ANALYSIS/macros/AliRootProofLite.par";
  else {
    mainPackage = GetSoftVersion("aliphysics",softVersions);
    mainPackage.Prepend("VO_ALICE@AliPhysics::");
  }
  if ( ! mainPackage.BeginsWith("VO_ALICE") ) gProof->UploadPackage(mainPackage.Data());
  gProof->EnablePackage(mainPackage.Data(),list,notOnClient);

  // Optionally add packages
  for ( Int_t ipkg=0; ipkg<extraPkgs.GetEntries(); ipkg++) {
    TString currPkg = extraPkgs.At(ipkg)->GetName();
    gProof->UploadPackage(currPkg.Data());
    gProof->EnablePackage(currPkg.Data(),notOnClient);
  }

  // compile additional tasks on workers
  for ( Int_t isrc=0; isrc<extraSrcs.GetEntries(); isrc++ ) {
    TString currSrc = extraSrcs.At(isrc)->GetName();
    gProof->Load(Form("%s+g",currSrc.Data()),notOnClient);
  }
  return kTRUE;
}


//______________________________________________________________________________
TObject* CreateInputObject ( TString runMode, TString analysisMode, TString inputName, TString inputOptions )
{
  // Create input object
  TString sMode = GetMode(runMode,analysisMode);

  if ( sMode == "local" ) {
    TString treeName = ( IsAOD(inputName,inputOptions) ) ? "aodTree" : "esdTree";

    TChain* chain = new TChain(treeName.Data());
    if ( inputName.EndsWith(".root") ) chain->Add(inputName.Data());
    else {
      ifstream inFile(inputName.Data());
      TString inFileName;
      if (inFile.is_open())
      {
        while (! inFile.eof() )
        {
          inFileName.ReadLine(inFile,kFALSE);
          if(!inFileName.EndsWith(".root")) continue;
          chain->Add(inFileName.Data());
        }
      }
      inFile.close();
    }
    if (chain) chain->GetListOfFiles()->ls();
    return chain;
  }
  else if ( sMode == "proof") {
    TString outName = "";
    TFileCollection* fc = 0x0;
    if ( inputName.EndsWith(".root") ) {
      // Assume this is a collection
      if ( IsPodMachine(analysisMode) ) inputName = gSystem->BaseName(inputName.Data());
      TFile* file = TFile::Open(inputName.Data());
      fc = static_cast<TFileCollection*>(file->FindObjectAny("dataset"));
      file->Close();
    }
    else if ( analysisMode == "prooflite" || runMode == "test" ) {
      fc = new TFileCollection("dataset");
      fc->AddFromFile(inputName.Data());
//      outName = "test_collection";
//      gProof->RegisterDataSet(outName.Data(), coll, "OV");
    }
    if ( fc ) return fc;

    if ( analysisMode == "vaf" ) outName = gSystem->GetFromPipe(Form("cat %s",GetDatasetName().Data()));
    else outName = GetDatasetName().Data();

    TObjString *output = new TObjString(outName);
    return output;
  }
  return NULL;
}

//______________________________________________________________________________
//Bool_t EditVafConf ( TString aaf, TString softVersions )
//{
//  TString localDir = "tmp_Vafconf";
//  TString copyCommand = GetProofInfo("copycommand",aaf);
//  TString rmdirCmd = Form("rm -r %s",localDir.Data());
//  TString remoteDir = GetProofInfo("proofserver",aaf);
//  remoteDir.Append(":.vaf");
//  Bool_t yesToAll = kTRUE;
//  PerformAction(Form("mkdir %s",localDir.Data()),yesToAll);
//  TString command = Form("%s %s/ %s/",copyCommand.Data(),remoteDir.Data(),localDir.Data());
//  PerformAction(command.Data(),yesToAll);
//  TString localFile = Form("%s/vaf.conf",localDir.Data());
//  if ( gSystem->AccessPathName(localFile) ) {
//    printf("Warning: cannot copy from %s\n",remoteDir.Data());
//    PerformAction(rmdirCmd.Data(),yesToAll);
//    return kFALSE;
//  }
//  TString os = gSystem->GetFromPipe("uname");
//  TString sedOpt = ( os == "Darwin" ) ? "-i ''" : "-i";
//  command = Form("sed %s 's/VafAliPhysicsVersion=.*/VafAliPhysicsVersion=%s/' %s",sedOpt.Data(),GetSoftVersion("aliphysics",softVersions).Data(),localFile.Data());
//  PerformAction(command.Data(),yesToAll);
//  command = Form("%s %s/ %s/",copyCommand.Data(),localDir.Data(),remoteDir.Data());
//  PerformAction(command.Data(),yesToAll);
//  PerformAction(rmdirCmd.Data(),yesToAll);
//  return kTRUE;
//}


//______________________________________________________________________________
void WritePodExecutable ( TString analysisOptions )
{
  TString filename = "runPod.sh";
  ofstream outFile(filename.Data());
  analysisOptions.ToUpper();
  Bool_t splitPerRun = analysisOptions.Contains("SPLIT");
  outFile << "#!/bin/bash" << endl;
  outFile << "nWorkers=${1-88}" << endl;
  outFile << "vafctl start" << endl;
  outFile << "vafreq $nWorkers" << endl;
  outFile << "vafwait $nWorkers" << endl;
  outFile << "export TASKDIR=\"$HOME/" << GetPodOutDir().Data() << "\"" << endl;
  outFile << "cd $TASKDIR" << endl;
  TString dsName = GetDatasetName();
  if ( splitPerRun ) {
    outFile << "fileList=$(find . -maxdepth 1 -type f ! -name " << dsName.Data() << " | xargs)" << endl;
    outFile << "while read line; do" << endl;
    outFile << "  runNum=$(echo \"$line\" | grep -oE '[0-9][0-9][0-9][1-9][0-9][0-9][0-9][0-9][0-9]' | xargs)" << endl;
    outFile << "  if [ -z \"$runNum\" ]; then" << endl;
    outFile << "    runNum=$(echo \"$line\" | grep -oE [1-9][0-9][0-9][0-9][0-9][0-9] | xargs)" << endl;
    outFile << "  fi" << endl;
    outFile << "  if [ -z \"$runNum\" ]; then" << endl;
    outFile << "    echo \"Cannot find run number in $line\"" << endl;
    outFile << "    continue" << endl;
    outFile << "   elif [ -e \"$runNum\" ]; then" << endl;
    outFile << "    echo \"Run number already processed: skip\"" << endl;
    outFile << "    continue" << endl;
    outFile << "  fi" << endl;
    outFile << "  echo \"\"" << endl;
    outFile << "  echo \"Analysing run $runNum\"" << endl;
    outFile << "  mkdir $runNum" << endl;
    outFile << "  cd $runNum" << endl;
    outFile << "  for ifile in $fileList; do ln -s ../$ifile; done" << endl;
    outFile << "  echo \"$line\" > " << dsName.Data() << endl;
  }
  TString rootCmd = GetRunMacro();
  rootCmd.Prepend("root -b -q '");
  rootCmd.Append("'");
  outFile << rootCmd.Data() << endl;
  if ( splitPerRun ) {
    outFile << "cd $TASKDIR" << endl;
    outFile << "done < " << dsName.Data() << endl;
    outFile << "outNames=$(find $PWD/*/ -type f -name \"*.root\" -exec basename {} \\; | sort -u | xargs)" << endl;
    outFile << "for ifile in $outNames; do" << endl;
    TString mergeList = "mergeList.txt";
    outFile << "  find $PWD/*/ -name \"$ifile\" > " << mergeList.Data() << endl;
    outFile << "  root -b -q $ALICE_PHYSICS/PWGPP/MUON/lite/mergeGridFiles.C\\(\\\"$ifile\\\",\\\"" << mergeList.Data() << "\\\",\\\"\\\"\\)" << endl;
    outFile << "  rm " << mergeList.Data() << endl;
    outFile << "done" << endl;
  }
//  outFile << "root -b <<EOF" << endl;
//  outFile << rootCmd.Data() << endl;
//  outFile << ".q" << endl;
//  outFile << "EOF" << endl;
  outFile << "vafctl stop" << endl;
  outFile << "exit" << endl;
  outFile.close();
  gSystem->Exec(Form("chmod u+x %s",filename.Data()));
}

//______________________________________________________________________________
void ConnectToPod ( TString aaf, TString softVersions, TString analysisOptions )
{
  if ( ! IsPod(aaf) ) return;

  TString copyCommand = GetProofInfo("copycommand",aaf);
  TString openCommand = GetProofInfo("opencommand",aaf);
//  TString aafEnter = GetProofInfo("aafenter",aaf);

  Int_t nWorkers = 88;
  TString nWorkersStr = analysisOptions(TRegexp("NWORKERS=[0-9]+"));
  if ( ! nWorkersStr.IsNull() ) {
    nWorkersStr.ReplaceAll("NWORKERS=","");
    if ( nWorkersStr.IsDigit() ) nWorkers = nWorkersStr.Atoi();
  }

  Bool_t yesToAll = kTRUE;
  TString remoteDir = GetProofInfo("proofserver",aaf);
  remoteDir += Form(":%s",GetPodOutDir().Data());
  TString baseExclude = "--exclude=\"*/\" --exclude=\"*.log\" --exclude=\"outputs_valid\" --exclude=\"*.xml\" --exclude=\"*.jdl\" --exclude=\"plugin_test_copy\" --exclude=\"*.so\" --exclude=\"*.d\"";
  TString syncOpt = analysisOptions.Contains("resume",TString::kIgnoreCase) ? "--delete" : "--delete-excluded";
  TString command = Form("%s %s %s ./ %s/",copyCommand.Data(),syncOpt.Data(),baseExclude.Data(),remoteDir.Data());
  PerformAction(command,yesToAll);
//  command = Form("%s %s %s %s",baseSync.Data(),baseExclude.Data(),localDir.Data(),remoteDir.Data());
//  PerformAction(command,yesToAll);
//  EditVafConf(aaf,softVersions);
//  remoteDir.ReplaceAll(Form("%s:",remote.Data()),"");
//  printf("Please execute this on the remote machine:\n");
//  printf("\n. %s/runPod.sh [nWorkers]\n\n",GetPodOutDir().Data());
  TString execCommand = GetProofInfo("execcommand",aaf);
  execCommand.ReplaceAll("nworkers",Form("%i",nWorkers));
//  gSystem->Exec(Form("%s '%s %s'", openCommand.Data(),GetProofInfo("aafenter",aaf).Data(),execCommand.Data()));
  TString updateVersion = Form("sed -i \"s/VafAliPhysicsVersion=.*/VafAliPhysicsVersion=%s/\" .vaf/vaf.conf",GetSoftVersion("aliphysics",softVersions).Data());
  gSystem->Exec(Form("%s '%s; %s'",openCommand.Data(),updateVersion.Data(),execCommand.Data()));
//  gSystem->Exec(Form("%s -t %s", openCommand.Data(),GetProofInfo("aafenter",aaf).Data()));
}

//______________________________________________________________________________
void GetPodOutput ( TString aaf )
{
  if ( ! IsPod(aaf) ) return;
  if ( IsPodMachine(aaf) ) return;
  Bool_t yesToAll = kTRUE;
  TString copyCommand = GetProofInfo("copycommand",aaf);
  TString remoteDir = GetProofInfo("proofserver",aaf);
  remoteDir += Form(":%s",GetPodOutDir().Data());
  PerformAction(Form("%s %s/*.root ./",copyCommand.Data(),remoteDir.Data()),yesToAll);
}

//______________________________________________________________________________
TObject* CreateAlienHandler ( TString runMode, TString inputName, TString inputOptions, TString softVersions, TString libraries, TString includePaths, TString workDir )
{
  AliAnalysisAlien *plugin = new AliAnalysisAlien();

  // Set the run mode
  plugin->SetRunMode(runMode.Data());

  plugin->SetAPIVersion("V1.1x");
  plugin->SetAliPhysicsVersion(GetSoftVersion("aliphysics",softVersions));

  if ( runMode == "test" ) plugin->SetFileForTestMode(inputName.Data());

  plugin->SetAdditionalRootLibs("libXMLParser.so libGui.so libProofPlayer.so");

  if ( ! runMode.Contains("terminate") ) plugin->SetMergeViaJDL();

  TString period = GetPeriod(inputOptions);

  // Set run list
  TString sRunList = "", dataDir = "", dataPattern = "";
  if ( gSystem->AccessPathName(inputName.Data()) ) {
    dataDir = GetDataDir(inputName);
    dataPattern = GetDataPattern(inputName);
    sRunList = GetRunNumber(inputName);
    if ( period.IsNull() ) period = GetPeriod(inputName);
  }
  else {
    TString currStr = "";
    ifstream inFile(inputName.Data());
    if ( inFile.is_open() ) {
      while ( ! inFile.eof() ) {
        currStr.ReadLine(inFile);
        if ( dataDir.IsNull() ) dataDir = GetDataDir(currStr);
        if ( dataPattern.IsNull() ) dataPattern = GetDataPattern(currStr);
        if ( period.IsNull() ) period = GetPeriod(currStr);
        TString currRun = GetRunNumber(currStr);
        if ( ! currRun.IsNull() ) {
          if ( ! sRunList.IsNull() ) sRunList.Append(" ");
          sRunList += currRun;
        }
      }
    }
    inFile.close();
  }

  if ( ! IsMC(inputOptions) ) {
    plugin->SetRunPrefix("000");
    if ( ! workDir.IsNull() && ! period.IsNull() ) {
      plugin->SetGridWorkingDir(Form("analysis/%s/%s",period.Data(),workDir.Data()));
    }
  }
  plugin->AddRunList(sRunList.Data());
  plugin->SetGridDataDir(dataDir.Data());
  plugin->SetDataPattern(dataPattern.Data());

  plugin->SetCheckCopy(kFALSE); // Fixes issue with alien_CLOSE_SE

  // Set libraries
  TObjArray* pathList = includePaths.Tokenize(" ");
  for ( Int_t ipath=0; ipath<pathList->GetEntries(); ipath++ ) {
    TString currName = pathList->At(ipath)->GetName();
    plugin->AddIncludePath(Form("-I%s",currName.Data()));
  }
  delete pathList;

  TString extraLibs = "", extraSrcs = "";
  TObjArray* libList = libraries.Tokenize(" ");
  for ( Int_t ilib=0; ilib<libList->GetEntries(); ilib++) {
    TString currName = gSystem->BaseName(libList->At(ilib)->GetName());
    if ( currName.EndsWith(".cxx") ) {
      extraSrcs += Form("%s ", currName.Data());
      TString auxName = currName;
      auxName.ReplaceAll(".cxx",".h");
      extraLibs += Form("%s %s ", auxName.Data(), currName.Data());
    }
    else if ( currName.EndsWith(".par") ) plugin->EnablePackage(currName.Data());
    else if ( currName.EndsWith(".so") ) extraLibs += Form("%s ", currName.Data());
  }
  delete libList;

  plugin->SetAnalysisSource(extraSrcs.Data());
  plugin->SetAdditionalLibs(extraLibs.Data());
  plugin->SetAdditionalRootLibs("libGui.so libProofPlayer.so libXMLParser.so");

  plugin->SetOutputToRunNo();
  plugin->SetNumberOfReplicas(2);
  plugin->SetDropToShell(kFALSE);


  return plugin;
}

//_______________________________________________________
TMap* SetupAnalysis ( TString runMode = "test", TString analysisMode = "grid",
                       TString inputName = "runList.txt",
                       TString inputOptions = "",
                       TString softVersions = "",
                       TString analysisOptions = "",
                       TString libraries = "",
                       TString includePaths = "",
                       TString workDir = "",
                       Bool_t isMuonAnalysis = kTRUE )
{
  analysisMode.ToLower();
  runMode.ToLower();
  analysisOptions.ToUpper();

  if ( IsPodMachine(analysisMode) ) inputName = GetDatasetName();
  gSystem->ExpandPathName(inputName);

  TString currDir = gSystem->pwd();
  Bool_t copyLocal = WorkDir(runMode,analysisMode,workDir);
  if ( copyLocal ) {
    CopyFilesLocally(libraries,inputName,analysisMode,currDir);
    CopyAdditionalFilesLocally("$TASKDIR/runTaskUtilities.C $TASKDIR/BuildMuonEventCuts.C $TASKDIR/SetupMuonBasedTasks.C",kFALSE);
    if ( IsPod(analysisMode) ) {
      if ( inputName.EndsWith(".root") ) CopyAdditionalFilesLocally(inputName);
      WritePodExecutable(analysisOptions);
    }
  }

  if ( ! IsPodMachine(analysisMode) ) LoadLibsLocally(libraries,includePaths);

  TString baseMacroDir = gSystem->Getenv("TASKDIR");
  AliAnalysisAlien* plugin = 0x0;

  TString sMode = GetMode(runMode,analysisMode);

  if ( sMode == "grid" ) {
    // Create and configure the alien handler plugin
    //#endif
    // CAVEAT: use (class*) to cast since CINT does not know static_cast
    plugin = static_cast<AliAnalysisAlien*>(CreateAlienHandler(runMode,inputName,inputOptions,softVersions,libraries,includePaths,workDir));
    TString setAlienIOmacro = gSystem->ExpandPathName(Form("%s/SetAlienIO.C",baseMacroDir.Data()));
    if ( ! gSystem->AccessPathName(setAlienIOmacro.Data()) ) {
      gROOT->LoadMacro(Form("%s+",setAlienIOmacro.Data()));
#ifndef TESTCOMPILATION
      SetAlienIO(inputOptions,GetPeriod(inputOptions),plugin);
#endif
    }
  }
  else if ( sMode == "proof" ) {
    if ( runMode == "test" ) analysisMode = "prooflite";
    if ( ! IsPod(analysisMode) || IsPodMachine(analysisMode) ) LoadLibsProof(libraries,includePaths,analysisMode,softVersions);
    else ConnectToPod(analysisMode,softVersions,analysisOptions);
  }

  /// Some utilities for muon analysis
  TString foundLib = gSystem->GetLibraries("libPWGmuon.so","",kFALSE);
  if ( IsPodMachine(analysisMode) || ! foundLib.IsNull() ) {
    TString macroEventCuts = Form("%s/BuildMuonEventCuts.C",baseMacroDir.Data());
    if ( ! gSystem->AccessPathName(macroEventCuts.Data()) ) gROOT->LoadMacro(Form("%s",macroEventCuts.Data()));

    TString macroSetupMuonBased = gSystem->ExpandPathName(Form("%s/SetupMuonBasedTasks.C",baseMacroDir.Data()));
    if ( ! gSystem->AccessPathName(macroSetupMuonBased.Data()) ) gROOT->LoadMacro(Form("%s",macroSetupMuonBased.Data()));
  }

  //
  // Make the analysis manager
  //
  AliAnalysisManager *mgr = new AliAnalysisManager("testAnalysis");
//  PerformAction(Form("cd %s",outDir.Data()),yesToAll);

  if ( plugin ) mgr->SetGridHandler(plugin);

  Bool_t isAOD = IsAOD(inputName,inputOptions);
  Bool_t isESD = IsESD(inputName,inputOptions);
  Bool_t isMC = IsMC(inputOptions);
  Bool_t isEmbed = IsEMBED(inputOptions);


  if ( ! isAOD && ! isESD ) {
    printf("Error: cannot determine if it is AOD or ESD\n");
    return 0x0;
  }

  TMap* map = ParseInfo(inputName,inputOptions,analysisOptions);
  printf("\nParsed parameters:\n");
  map->Print();
  printf("\n");
  TString physSel = map->GetValue("physicsSelection")->GetName();
  TString centr = map->GetValue("centrality")->GetName();

  AliMultiInputEventHandler* multiHandler = 0x0;
  if ( analysisOptions.Contains("MIXED") ) {
    multiHandler = new AliMultiInputEventHandler();
    mgr->SetInputEventHandler(multiHandler);
  }


  // input handler
  if ( isAOD ) {
    AliAODInputHandler* aodH = new AliAODInputHandler();
    //aodH->SetCheckStatistics(kTRUE); // Force to get statistics info from EventStat_temp.root // REMEMBER TO CHECK
    if ( multiHandler ) multiHandler->AddInputEventHandler(aodH);
    else mgr->SetInputEventHandler(aodH);
  }
  else {
    AliESDInputHandler* esdH = new AliESDInputHandler();
    if ( isMuonAnalysis ) {
      esdH->SetReadFriends(kFALSE);
      esdH->SetInactiveBranches("*");
      esdH->SetActiveBranches("MuonTracks MuonClusters MuonPads AliESDRun. AliESDHeader. AliMultiplicity. AliESDFMD. AliESDVZERO. AliESDTZERO. SPDVertex. PrimaryVertex. AliESDZDC. SPDPileupVertices");
    }

    if ( multiHandler ) multiHandler->AddInputEventHandler(esdH);
    else mgr->SetInputEventHandler(esdH);
    
    if ( isMC ){
      // Monte Carlo handler
      AliMCEventHandler* mcHandler = new AliMCEventHandler();
      if ( multiHandler ) multiHandler->AddInputEventHandler(mcHandler);
      else mgr->SetMCtruthEventHandler(mcHandler);
      printf("\nMC event handler requested\n\n");
    }

    Bool_t treatAsMC = ( isMC && ! isEmbed );
    if ( physSel == "yes" ) {
      printf("Adding physics selection task\n");
      gROOT->LoadMacro("$ALICE_PHYSICS/OADB/macros/AddTaskPhysicsSelection.C");
#ifndef TESTCOMPILATION
      AliPhysicsSelectionTask* physSelTask = AddTaskPhysicsSelection(treatAsMC);
      if ( ! treatAsMC ) physSelTask->GetPhysicsSelection()->SetUseBXNumbers(kFALSE); // Needed if you want to merge runs with different running scheme
      physSelTask->GetPhysicsSelection()->SetPassName(GetPass(inputName,inputOptions));
#endif
    }

    // Old centrality framework
    if ( centr == "OLD" ) {
      printf("Adding old centrality task\n");
      gROOT->LoadMacro("$ALICE_PHYSICS/OADB/macros/AddTaskCentrality.C");
#ifndef TESTCOMPILATION
      AliCentralitySelectionTask* centralityTask = AddTaskCentrality();
      if ( treatAsMC ) centralityTask->SetMCInput();
#endif
    }
  }

  if ( centr == "YES" ) {
    printf("Adding centrality task\n");
    gROOT->LoadMacro("$ALICE_PHYSICS/OADB/COMMON/MULTIPLICITY/macros/AddTaskMultSelection.C");
#ifndef TESTCOMPILATION
    AliMultSelectionTask* centralityTask = AddTaskMultSelection(kFALSE);
    if ( isMC ) centralityTask->SetUseDefaultMCCalib(kTRUE); // MC
    else centralityTask->SetUseDefaultCalib(kTRUE); // data
#endif
  }

//  if ( analysisOptions.Contains("TEST") ) {
//    gROOT->LoadMacro("$ALICE_ROOT/ANALYSIS/macros/train/AddTaskBaseLine.C");
//    AliAnalysisTaskBaseLine* baseLineTask = AddTaskBaseLine();
//  }
//  else {
//    TString macroSetupMuonBased = gSystem->ExpandPathName(Form("%s/SetupMuonBasedTasks.C",baseMacroDir.Data()));
//    if ( ! gSystem->AccessPathName(macroSetupMuonBased.Data()) ) {
//      gROOT->LoadMacro(Form("%s%s",macroSetupMuonBased.Data(),compileSuffix.Data()));
////      SetupMuonBasedTasks(map,taskOptions);
//    }
  
//  AliLog::SetClassDebugLevel("AliMCEvent",-1); // REMEMBER TO UNCOMMENT
//  AliLog::SetClassDebugLevel("AliAODHandler",-1); // REMEMBER TO UNCOMMENT
//  //mgr->SetNSysInfo(10); // REMEMBER TO COMMENT (test memory)

  if ( sMode.IsNull() ) {
    printf("Unimplemented options chosen!\n");
    PrintOptions();
    return 0x0;
  }

  LoadAddTasks(libraries);

  printf("Analyzing %s  MC %i\n", isAOD ? "AODs" : "ESDs", isMC);

  return map;
}


//______________________________________________________________________________
void StartAnalysis ( TString runMode, TString analysisMode, TString inputName, TString inputOptions )
{
  //
  // Run the analysis
  //

  if ( IsPod(analysisMode) && ! IsPodMachine(analysisMode) ) {
    GetPodOutput(analysisMode);
    runMode = "terminate";
  }
  TString sMode = GetMode(runMode,analysisMode);

  if ( sMode.IsNull() ) return;

  AliAnalysisManager* mgr = AliAnalysisManager::GetAnalysisManager();
  if ( ! mgr->InitAnalysis()) {
    printf("Fatal: Cannot initialize analysis\n");
    return;
  }
  mgr->PrintStatus();

  if ( sMode == "terminateonly" && gSystem->AccessPathName(mgr->GetCommonFileName())) {
    printf("Cannot find %s : noting done\n",mgr->GetCommonFileName());
    return;
  }

  TObject* inputObj = CreateInputObject(runMode,analysisMode,inputName,inputOptions);

  TString mgrMode =( sMode == "terminateonly" ) ? "grid terminate" : sMode.Data();

  if ( ! inputObj || inputObj->IsA() == TChain::Class() ) mgr->StartAnalysis(mgrMode.Data(), static_cast<TChain*>(inputObj));
  else if ( inputObj->IsA() == TFileCollection::Class() ) mgr->StartAnalysis(mgrMode.Data(), static_cast<TFileCollection*>(inputObj));
  else mgr->StartAnalysis(mgrMode.Data(), inputObj->GetName());

  //mgr->ProfileTask("SingleMuonAnalysisTask"); // REMEMBER TO COMMENT (test memory)
}


//______________________________________________________________________________
void WriteTemplateRunTask ( TString outputDir = "." )
{
  TString macroName = Form("%s/runTask.C",outputDir.Data());
  ofstream outFile(macroName.Data());
  outFile << "void runTask ( TString runMode, TString analysisMode," << endl;
  outFile << "  TString inputName," << endl;
  outFile << "  TString inputOptions = \"\"," << endl;
  outFile << "  TString softVersions = \"\"," << endl;
  outFile << "  TString analysisOptions = \"\"," << endl;
  outFile << "  TString taskOptions = \"\" )" << endl;
  outFile << "{" << endl;
  outFile << endl;
  outFile << "  gROOT->LoadMacro(gSystem->ExpandPathName(\"$TASKDIR/runTaskUtilities.C\"));" << endl;
  outFile << endl;
  outFile << "  TMap* map = SetupAnalysis(runMode,analysisMode,inputName,inputOptions,softVersions,analysisOptions, \"yourLibs\",\"$ALICE_ROOT/include $ALICE_PHYSICS/include\",\"\");" << endl;
  outFile << endl;
  outFile << "  Bool_t isMC = IsMC(inputOptions);" << endl;
  outFile << endl;

  outFile << "//  Uncomment following line if yourAddTask is not in ALICE_PHYSICS and you need to run on pod" << endl;
  outFile << "//  CopyAdditionalFilesLocally(\"yourAddTask.C\");" << endl;
  outFile << "  gROOT->LoadMacro(\"yourAddTask.C\");" << endl;
  outFile << "  AliAnalysisTask* task = yourAddTask();" << endl;
  outFile << endl;
  outFile << "//  AliMuonEventCuts* eventCuts = BuildMuonEventCuts(map);" << endl;
  outFile << endl;
  outFile << "//  SetupMuonBasedTask(task,eventCuts,taskOptions,map);" << endl;
  outFile << endl;
  outFile << "  StartAnalysis(analysisMode,inputName,inputOptions);" << endl;
  outFile << "}" << endl;
  outFile.close();
}
