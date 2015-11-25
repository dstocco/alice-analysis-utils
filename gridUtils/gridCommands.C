#if !defined(__CINT__) || defined(__MAKECINT__)

#include <Riostream.h>

// ROOT includes
#include "TString.h"
#include "TSystem.h"
#include "TGrid.h"
#include "TGridResult.h"
#include "TROOT.h"
#include "TObjArray.h"
#include "TObjString.h"
#include "TMap.h"
#include "TFile.h"
#include "TRegexp.h"
#include "THashList.h"
#include "TArrayI.h"
#endif

enum {kTmpPsMaster, kTmpMasterjob, kTmpPsTrace, kTmpPsJdl, kNtmpFiles};
TString tmpFiles[kNtmpFiles] = {"/tmp/tmpPsMaster.txt", "/tmp/tmpMasterjob.txt", "/tmp/tmpPsTrace.txt", "/tmp/tmpAlienPsJdl.txt"};

Double_t GuessFirstJob(TObjArray*);
TObjArray* GetMasterList(Bool_t redoPs = kTRUE);
TObjArray* GetSubjobInfo(TString, Bool_t redoPs = kTRUE);
TObjArray* GetSubjobList(TString);
Int_t GetNkilledJobs(TString, Bool_t redoPs = kTRUE);
Double_t GetRunNumber(TString, Bool_t redoPs = kTRUE);
void GetOutDirs(TString, TString&, TString outFilename="root_archive.zip");
TString GetOutDirInJdl(TString, Bool_t redoPs = kTRUE);
TString GetToken(Int_t, TString, TString delimiter="/");
TString GetSubPath(Int_t, TString);
Bool_t PerformAction(TString, Bool_t&);
Bool_t FileExists(const char *); // From AliAnalysisAlien
Bool_t DirectoryExists(const char *); // From AliAnalysisAlien
Bool_t PruneEmptyDirs(TString, Bool_t yesToAll = kFALSE);
void CleanTmpFiles();

//////////////////////////////////////////////////////////////////
// The name of functions that can be called by users starts with:
// grid...
// See description in comments within the functions
// The other functions, whose name starts with a capital letter
// are internal
//////////////////////////////////////////////////////////////////

//_______________________________________________________
void gridCopyToSE(TString filesToCopy, TString alienDestination, TString alienCloseSE = "")
{
  //
  // Copy files to a specific SE
  // NB: the filesToCopy accept wildcards, i.e. analysis*.C
  //

  TGrid::Connect("alien://");

  Bool_t yesToAll = kFALSE;

  if ( alienCloseSE.IsNull() ) alienCloseSE = gSystem->Getenv("alien_CLOSE_SE");
  alienCloseSE.ReplaceAll("@","");
  TString tmpFilename = "/tmp/tmpListCopyGrid.txt";
  gSystem->Exec(Form("ls %s > %s", filesToCopy.Data(), tmpFilename.Data()));
  ifstream inFile(tmpFilename.Data());
  TString currLine = "";
  TObjArray fileList(0);
  if ( inFile.is_open() ) {
    while ( ! inFile.eof() ) {
      currLine.ReadLine(inFile);
      if ( currLine.IsNull() ) continue;
      fileList.AddLast(new TObjString(currLine));
    }
    inFile.close();
  }  
  for ( Int_t ifile=0; ifile<fileList.GetEntries(); ifile++ ) {
    TString currFile = fileList.At(ifile)->GetName();
    TString alienName = Form("%s/%s",alienDestination.Data(),currFile.Data());
    Bool_t isRemoved = kFALSE;
    if ( FileExists(alienName.Data()) ) {
      isRemoved = PerformAction(Form("rm %s", alienName.Data()), yesToAll);
      // Do not copy file if it exists and you do not remove it
      if ( ! isRemoved ) continue;
    }
    if ( ! alienCloseSE.IsNull() ) alienName.Append(Form("@%s", alienCloseSE.Data()));
    if ( ! alienName.Contains("alien://") ) alienName.Prepend("alien://");
    Bool_t &yesToCp = ( isRemoved ) ? isRemoved : yesToAll;
    PerformAction(Form("alien_cp %s %s", currFile.Data(), alienName.Data()), yesToCp);
    //printf("cp %s alien://%s\n", currFile.Data(), alienName.Data());
    //TFile::Cp(currFile.Data(), alienName.Data());
  } // loop on files
  gSystem->Exec(Form("rm %s", tmpFilename.Data()));
}

/*
//_______________________________________________________
void gridFindFailed(Double_t minJob = -1., TString errorState = "ALL", Double_t maxJob = -1.)
{
  //
  // Find failing jobs starting for masterjobs in the range:
  // minJob - maxJob.
  // If range is not specified, it automatically find the first job
  // of the last sequence of jobs.
  // Subjobs in the specified errorState are resubmitted.
  // NB. The subjob status is checked and jobs are resubmitted
  //     if ( jobStatus.Contains(errorState.Data()) )
  // So, if you want to resubmit all jobs in error it is enough to set:
  // errorState = "ERROR".
  // The keyoword errorState = "ALL", stands for all
  // all jobs - "*ING" - "DONE" = ERROR*"+"ZOMBIE"+"EXPIRED"
  //

  if ( ! gGrid ) TGrid::Connect("alien://");

  TObjArray* masterList = GetMasterList();
  if ( minJob < 0. ) 
    minJob = GuessFirstJob(masterList);

  TString currJob = "", command = "", currInfo = "", currState = "";

  Bool_t yesToAll = kFALSE;

  for ( Int_t ijob=0; ijob<masterList->GetEntries(); ijob++ ) {
    currJob = ((TObjString*)masterList->At(ijob))->GetString();
    Double_t currJobNum = currJob.Atof();
    if ( currJobNum < minJob ) continue;
    if ( maxJob >= 0 && currJobNum > maxJob ) continue;
    printf("Checking master %s...\n", currJob.Data());
    TObjArray* subjobInfo = GetSubjobInfo(currJob.Data());
    Int_t nSubjobs = subjobInfo->GetEntries();
    Bool_t hasSubjobs = nSubjobs > 1;
    for ( Int_t istatus=0; istatus<nSubjobs; istatus++ ) {

      // The first error state refers to master
      // If master has subjobs, do not check master itself
      if ( istatus == 0 && hasSubjobs ) continue;

      // Check error
      currInfo = ((TObjString*)subjobInfo->At(istatus))->GetString();
      if ( errorState.Contains("ALL") ) {
        if ( currInfo.Contains("ING") ||
             currInfo.Contains("DONE") ) continue;

       //if ( ! currInfo.Contains("EXPIRE") && ! currInfo.Contains("ERROR") && ! currInfo.Contains("ZOMBIE") ) continue;
      }
      else if ( ! currInfo.Contains(errorState.Data()) )
        continue;
      printf("  %s\n", currInfo.Data());

      if ( currInfo.Contains("ERROR_RE") ) {
        printf("ERROR_RE: output should be already created. Not resubmitted.\n");
        continue;
      }

      if ( hasSubjobs ) {
        GetToken(0, currInfo, currState, ":");
        command = Form("gbbox masterJob %s -status %s resubmit", currJob.Data(), currState.Data());
      }
      else
        command = Form("resubmit %s", currJob.Data());

      PerformAction(command, yesToAll);
    } // loop on status
    delete subjobInfo;
  } // loop on job
  delete masterList;
}
*/


//_______________________________________________________
void gridFindFailed(Double_t minJob = -1., TString errorStatus = "ALL", Double_t maxJob = -1., TString mailto = "", Double_t doneJobFractionForAlert = 0.98)
{
  //
  // Find failing jobs starting for masterjobs in the range:
  // minJob - maxJob.
  // If range is not specified, it automatically find the first job
  // of the last sequence of jobs.
  // Subjobs in the specified errorStatus are resubmitted.
  // NB. The subjob status is checked and jobs are resubmitted
  //     if ( jobStatus.Contains(errorStatus.Data()) )
  // So, if you want to resubmit all jobs in error it is enough to set:
  // errorStatus = "ERROR".
  // The keyoword errorStatus = "ALL", stands for
  // ERROR*"+"ZOMBIE"+"EXPIRED"
  //
  // If baseOutDir is set to the alien output, the function
  // checks for each subjob one-by-one and resubmit ONLY if the
  // corresponding output is NOT YET CREATED.
  // This is of course slower, but solves some problems with grid:
  // in particular when the job output is created but the jobs are
  // in a (wrong) error state
  //
  
  if ( ! gGrid ) TGrid::Connect("alien://");
  
  TObjArray* masterList = GetMasterList();
  if ( minJob < 0. ) minJob = GuessFirstJob(masterList);
  
  Bool_t yesToAll = kFALSE;
  
  Int_t maxDim = 50;
  TObjArray outDirs(maxDim);
  outDirs.SetOwner();
  TArrayI nDone(maxDim), nTotal(maxDim);
  
  for ( Int_t ijob=0; ijob<masterList->GetEntries(); ijob++ ) {
    TString masterjobId = ((TObjString*)masterList->At(ijob))->GetString();
    Double_t masterjobIdNum = masterjobId.Atof();
    if ( masterjobIdNum < minJob ) continue;
    if ( maxJob >= 0 && masterjobIdNum > maxJob ) continue;
    printf("Checking master %s...\n", masterjobId.Data());
    TObjArray* subjobInfo = GetSubjobInfo(masterjobId.Data());
    Int_t nSubjobs = subjobInfo->GetEntries();
    Bool_t hasSubjobs = nSubjobs > 1;
    for ( Int_t istatus=0; istatus<nSubjobs; istatus++ ) {
      
      // The first error state refers to master
      // If master has subjobs, do not check master itself
      if ( istatus == 0 && hasSubjobs ) continue;
      
      // Check error
      TString currInfo = ((TObjString*)subjobInfo->At(istatus))->GetString();
      TString printStatus = GetToken(0, currInfo, "|");
      TString currStatus = GetToken(0, printStatus, ":");
      
      Bool_t resubmit = kFALSE;
      
      if ( errorStatus.Contains("ALL") ) {
        resubmit = ( currStatus.Contains("ERROR") || currStatus.Contains("EXPIRED") || currStatus.Contains("ZOMBIE") );
      }
      else resubmit = currStatus.Contains(errorStatus.Data());

      if ( resubmit ) {
        printf("  %s\n", printStatus.Data());
      
        TString command =  ( hasSubjobs ) ? Form("gbbox masterJob %s -status %s resubmit", masterjobId.Data(), currStatus.Data()) : Form("resubmit %s", masterjobId.Data());
        PerformAction(command, yesToAll);
      }
      
      if ( ! mailto.IsNull() ) {
        TString outDir = GetOutDirInJdl(masterjobId);
        for ( Int_t iarr=-1; iarr>=-3; iarr-- ) {
          TString currStr = GetToken(iarr,outDir);
          if ( currStr.Contains("alien_counter") ) continue;
          if ( currStr.IsDigit() ) continue;
          outDir = GetSubPath(iarr+1,outDir);
          break;
        }
        TObject* obj = outDirs.FindObject(outDir.Data());
        if ( ! obj ) {
          obj = new TObjString(outDir);
          outDirs.Add(obj);
        }
        Int_t idx = outDirs.IndexOf(obj);
        Int_t nJobsInStatus = ( istatus == 0 ) ? 1 : GetToken(1,printStatus,":").Atoi();
        nTotal[idx] += nJobsInStatus;
        if ( currStatus.Contains("DONE") ) nDone[idx] += nJobsInStatus;
      }
    } // loop on status
    delete subjobInfo;
  } // loop on job
  delete masterList;
  
  TString summary = "";
  Bool_t sendMail = kFALSE;
  for ( Int_t iarr=0; iarr<outDirs.GetEntries(); iarr++ ) {
    Double_t percentDone = ( nTotal[iarr] == 0 ) ? 1 : (Double_t)nDone[iarr]/((Double_t)nTotal[iarr]);
    if ( percentDone > doneJobFractionForAlert ) sendMail = kTRUE;
    summary += Form("%s  done/total = %i/%i = %g\n",outDirs.At(iarr)->GetName(), nDone[iarr], nTotal[iarr], percentDone);
  }
  
  if ( ! summary.IsNull() ) printf("\nSummary:\n%s",summary.Data());
  if (sendMail) {
    gSystem->Exec(Form("echo \"%s\" | mail -s \"gridFindFailed alert\" %s",summary.Data(),mailto.Data()));
  }
  
  CleanTmpFiles();
}


//_______________________________________________________
void gridKillJobRange(Double_t minJob, Double_t maxJob = -1.)
{
  /// Kill jobs in the specified range
  if ( ! gGrid ) TGrid::Connect("alien://");
  
  TObjArray* masterList = GetMasterList();
  
  Bool_t yesToAll = kFALSE;
  
  for ( Int_t ijob=0; ijob<masterList->GetEntries(); ijob++ ) {
    TString masterjobId = ((TObjString*)masterList->At(ijob))->GetString();
    Double_t masterjobIdNum = masterjobId.Atof();
    if ( masterjobIdNum < minJob ) continue;
    if ( maxJob >= 0 && masterjobIdNum > maxJob ) continue;
    PerformAction(Form("gbbox kill %s",masterjobId.Data()),yesToAll);
  }
  CleanTmpFiles();
}

//_______________________________________________________
//void gridFindFailed(Double_t minJob = -1., TString errorStatus = "ALL", Double_t maxJob = -1., TString baseOutDir = "")
//{
//  //
//  // Find failing jobs starting for masterjobs in the range:
//  // minJob - maxJob.
//  // If range is not specified, it automatically find the first job
//  // of the last sequence of jobs.
//  // Subjobs in the specified errorStatus are resubmitted.
//  // NB. The subjob status is checked and jobs are resubmitted
//  //     if ( jobStatus.Contains(errorStatus.Data()) )
//  // So, if you want to resubmit all jobs in error it is enough to set:
//  // errorStatus = "ERROR".
//  // The keyoword errorStatus = "ALL", stands for
//  // ERROR*"+"ZOMBIE"+"EXPIRED"
//  //
//  // If baseOutDir is set to the alien output, the function
//  // checks for each subjob one-by-one and resubmit ONLY if the 
//  // corresponding output is NOT YET CREATED.
//  // This is of course slower, but solves some problems with grid:
//  // in particular when the job output is created but the jobs are
//  // in a (wrong) error state
//  //
//
//  if ( ! gGrid ) TGrid::Connect("alien://");
//
//  TObjArray* masterList = GetMasterList();
//  if ( minJob < 0. ) 
//    minJob = GuessFirstJob(masterList);
//
//  TString masterjobId = "", subjobId = "";
//  TString command = "", currInfo = "", currStatus = "", printStatus = "";
//  TString outDirList = "", outDir = "";
//  if ( ! baseOutDir.IsNull() )
//    GetOutDirs(baseOutDir, outDirList, "root_archive.zip");
//
//  Bool_t yesToAll = kFALSE;
//
//  TString summary = "";
//
//  for ( Int_t ijob=0; ijob<masterList->GetEntries(); ijob++ ) {
//    masterjobId = ((TObjString*)masterList->At(ijob))->GetString();
//    Double_t masterjobIdNum = masterjobId.Atof();
//    if ( masterjobIdNum < minJob ) continue;
//    if ( maxJob >= 0 && masterjobIdNum > maxJob ) continue;
//    printf("Checking master %s...\n", masterjobId.Data());
//    Int_t nResubmittedJobs = 0;
//    Int_t nFailedJobs = 0;
//    TObjArray* subjobInfo = GetSubjobInfo(masterjobId.Data());
//    Int_t nSubjobs = subjobInfo->GetEntries();
//    Bool_t hasSubjobs = nSubjobs > 1;
//    for ( Int_t istatus=0; istatus<nSubjobs; istatus++ ) {
//
//      // The first error state refers to master
//      // If master has subjobs, do not check master itself
//      if ( istatus == 0 && hasSubjobs ) continue;
//
//      // Check error
//      currInfo = ((TObjString*)subjobInfo->At(istatus))->GetString();
//      printStatus = GetToken(0, currInfo, "|");
//      currStatus = GetToken(0, printStatus, ":");
//      if ( errorStatus.Contains("ALL") ) {
//        /*
//        if ( currStatus.Contains("ING") ||
//             currStatus.Contains("ASSIGNED") ||
//             currStatus.Contains("DONE") ) continue;
//        */
//        if ( ! currStatus.Contains("ERROR") &&
//             ! currStatus.Contains("EXPIRED") &&
//             ! currStatus.Contains("ZOMBIE") ) continue;
//       }
//      else if ( ! currStatus.Contains(errorStatus.Data()) )
//        continue;
//      printf("  %s\n", printStatus.Data());
//
//      // Check if resubmit is needed
//      if ( baseOutDir.IsNull() ) {
//        // Case 1: resubmit all errors
//	/*
//        if ( currStatus.Contains("ERROR_RE") ) {
//          printf("ERROR_RE: output should be already created. Not resubmitted.\n");
//          continue;
//        }
//	*/
//        command =  ( hasSubjobs ) ? Form("gbbox masterJob %s -status %s resubmit", masterjobId.Data(), currStatus.Data()) : Form("resubmit %s", masterjobId.Data());
//        PerformAction(command, yesToAll);
//      }
//      else {
//        // Case 2: check each subjob and outputDir before resubmitting
//
//        // Find output directory for each subjob
//        TObjArray* subjobList = GetSubjobList(currInfo.Data());
//        for ( Int_t isub=0; isub<subjobList->GetEntries(); isub++ ) {
//          subjobId = subjobList->At(isub)->GetName();
//          outDir = GetOutDirInJdl(subjobId);
//
//          // Faster ( find is performed only once )
//          // but does not work for merged jobs
//          // since the output dir is already there!
//          Bool_t outExists = outDirList.Contains(outDir);
//          // This is done for merged jobs
//          if ( ! hasSubjobs ) {
//            outExists = FileExists(Form("%s/root_archive.zip", outDir.Data()));
//          }
//
//          if ( outExists ) {
//            // Error state...but output was already created
//            printf("Warning: job in status %s but output %s is created!\n", currStatus.Data(), outDir.Data());
//          }
//          else {
//            // Resubmit subjob
//            command =  ( hasSubjobs ) ? Form("gbbox masterJob %s -id %s resubmit", masterjobId.Data(), subjobId.Data()) : Form("resubmit %s", masterjobId.Data());
//            if ( PerformAction(command, yesToAll) )
//              nResubmittedJobs++;
//            nFailedJobs++;
//          }
//        } // loop on subjobs
//        delete subjobList;
//      }
//    } // loop on status
//    delete subjobInfo;
//    Int_t nKilledJobs = baseOutDir.IsNull() ? -1 : GetNkilledJobs(masterjobId);
//    Double_t runNumber = baseOutDir.IsNull() ? -1. : GetRunNumber(masterjobId, kFALSE);
//    summary += Form("Master %s  run %.0f:  killed %i  failed %i  re-submitted %i\n", masterjobId.Data(), runNumber, nKilledJobs, nFailedJobs, nResubmittedJobs);
//  } // loop on job
//  delete masterList;
//  if ( ! baseOutDir.IsNull() )
//    printf("\n%s", summary.Data());
//
//  CleanTmpFiles();
//}

/*
//______________________________________________________
void gridCheckFailedOutput( TString baseOutDir, Double_t minJob = -1, Double_t maxJob = -1, TString outFilename="root_archive.zip")
{
  //
  // Same as gridFindFailed, but it checks for each subjob one-by-one
  // and resubmit ONLY if the corresponding outut is NOT YET CREATED
  // This is of course slower than gridFindFailed, but solves some problems
  // with grid: in particular when the job output is created but the jobs are
  // in a (wrong) error state
  // 

  if ( ! gGrid ) TGrid::Connect("alien://");

  TObjArray* masterList = GetMasterList();
  if ( minJob < 0. ) 
    minJob = GuessFirstJob(masterList);

  TString currJob = "", command = "", currLine = ""; //, xmlFilename = "";

  // Check the output dir in search of finished results
  command = Form("find %s %s", baseOutDir.Data(), outFilename.Data());
  printf("Command: %s\n", command.Data());
  TGridResult* outFileList = gGrid->Command(command);

  TIter next(outFileList);
  TMap* map = 0x0;
  TString outDirList = "";
  while ( ( map = (TMap*)next() ) ) {
    TObjString *objs = dynamic_cast<TObjString*>(map->GetValue("turl"));
    if ( ! objs ) continue;
    TString filePath = objs->GetString();
    filePath.ReplaceAll("alien://","");
    filePath.ReplaceAll(outFilename.Data(), "");
    if ( ! outDirList.IsNull() ) outDirList.Append(" ");
    outDirList.Append(filePath.Data());
  } // loop on results

  Bool_t yesToAll = kFALSE;

  TString tmpFilename = "/tmp/tmpAlienPs.txt";

  for ( Int_t ijob=0; ijob<masterList->GetEntries(); ijob++ ) {
    currJob = ((TObjString*)masterList->At(ijob))->GetString();
    Double_t currJobNum = currJob.Atof();
    if ( currJobNum < minJob ) continue;
    if ( maxJob >= 0 && currJobNum > maxJob ) continue;
    printf("Checking master %s...\n", currJob.Data());

    ////////
    Int_t nKilledJobs = 0;
    // ps -trace all: find xml file and killed jobs
    command = Form("alien_ps -trace %s all &> %s", currJob.Data(), tmpFilename.Data());
    gSystem->Exec(command.Data());
    ifstream inFile(tmpFilename.Data());
    if ( inFile.is_open() ) {
      while ( ! inFile.eof() ) {
        currLine.ReadLine(inFile,kTRUE); // Read line
        if ( currLine.Contains(".xml") ) {
          GetToken(4, currLine, xmlFilename, ":");
          currLine = xmlFilename;
          GetToken(0, currLine, xmlFilename, ",");
          continue;
        }
        if ( currLine.Contains("killing") )
          nKilledJobs++;
      } // loop on file lines
      inFile.close();
    }
    printf("Master %s xml %s  nKilledJobs %i\n", currJob.Data(), xmlFilename.Data(), nKilledJobs);
    ///////

    // Find subjobs
    TObjArray* subjobInfo = GetSubjobInfo(currJob.Data());
    Int_t nSubjobs = subjobInfo->GetEntries();
    Bool_t hasSubjobs = nSubjobs > 1;
    TString outDir = "", currInfo = "";
    for ( Int_t istatus=0; istatus<nSubjobs; istatus++ ) {

      // The first error state refers to master
      // If master has subjobs, do not check master itself
      if ( istatus == 0 && hasSubjobs ) continue;
      
      currInfo = subjobInfo->At(istatus)->GetName();
      if ( currInfo.Contains("ING") ||
           currInfo.Contains("DONE") ) continue;
      TObjArray* subJobList = GetSubjobList(currInfo.Data());
      // Find output directory for each subjob
      for ( Int_t isub=0; isub<subJobList->GetEntries(); isub++ ) {
        TString subJob = subJobList->At(isub)->GetName();
        command = Form("alien_ps -jdl %s &> %s", subJob.Data(), tmpFilename.Data());
        gSystem->Exec(command.Data());
        ifstream inFile(tmpFilename.Data());
        if ( inFile.is_open() ) {
          while ( ! inFile.eof() ) {
            currLine.ReadLine(inFile,kTRUE); // Read line
            if ( ! currLine.Contains("OutputDir") ) continue;
            GetToken(1, currLine, outDir, "\"");
            outDir.ReplaceAll("//","/");
            if ( outDirList.Contains(outDir) ) {
              TObjArray* auxArray = currInfo.Tokenize(":");
              auxArray->SetOwner();
              TString jobStatus = auxArray->At(0)->GetName();
              delete auxArray;
              printf("Warning: job in status %s but output %s is created!\n", jobStatus.Data(), outDir.Data());
            }
            else {
              command = Form("gbbox masterJob %s -id %s resubmit", currJob.Data(), subJob.Data());
              PerformAction(command, yesToAll);
            }
            break;
          }
          inFile.close();
        }
      } // loop on subjobs
      delete subJobList;
    } // loop on status
    delete subjobInfo;
  } // loop on master jobs
  //gSystem->Exec(Form("rm %s", tmpFilename.Data()));
}
*/

//______________________________________________________
void gridCheckMerged(TString baseOutDir, TString outFilename="AnalysisResults.root")
{
  //
  // OBSOLETE
  //
  
  if ( ! gGrid ) TGrid::Connect("alien://");

  baseOutDir.ReplaceAll("//","/");

  printf("Querying grid...\n");
  TString command = Form("find %s %s", baseOutDir.Data(), outFilename.Data());
  printf("Command: %s\n", command.Data());
  TGridResult* outFileList = gGrid->Command(command);

  TString runNum = "", subRunNum = "", mapValue = "";

  TIter next(outFileList);
  TMap* map = 0x0;
  TMap outMap;
  while ( ( map = (TMap*)next() ) ) {
    TObjString *objs = dynamic_cast<TObjString*>(map->GetValue("turl"));
    if ( ! objs ) continue;
    TString filePath = objs->GetString();
    filePath.ReplaceAll("alien://","");
    filePath.ReplaceAll(baseOutDir.Data(), "");
    //printf("path: %s\n", filePath.Data()); // REMEMBER TO CUT

    runNum = GetToken(-3, filePath);
    if ( runNum.IsDigit() ) {
      subRunNum = GetToken(-2, filePath);
      mapValue = subRunNum;
    }
    else {
      runNum = GetToken(-2, filePath);
      if ( runNum.IsDigit() ) mapValue = "merged";
      else continue;
    }

    TPair* outPair = dynamic_cast<TPair*>(outMap.FindObject(runNum.Data()));
    if ( ! outPair )
      outMap.Add(new TObjString(runNum.Data()), new TObjString(mapValue.Data()));
    else {
      TObjString* pairVal = dynamic_cast<TObjString*>(outPair->Value());
      TString currName = pairVal->GetString();
      currName.Append(Form(" %s", mapValue.Data()));
      pairVal->SetString(currName.Data());
    }
  }

  printf("Moving files...\n");
  TString answer = "n";
  TString problematicRuns = "";
  TIter nextMap(outMap.MakeIterator());
  TObject* pair = 0x0;
  Bool_t moveAll = kFALSE;
  while ( ( pair = nextMap() ) ) {
    runNum = pair->GetName();
    Bool_t isMerged = kFALSE;
    TString currVal = ((TPair*)outMap.FindObject(runNum.Data()))->Value()->GetName();
    TObjArray* subRunList = currVal.Tokenize(" ");
    Int_t nSubs = 0, maxSubs = 0;
    for ( Int_t isub=0; isub<subRunList->GetEntries(); isub++ ) {
      TString currSub = subRunList->At(isub)->GetName();
      if ( currSub.Contains("merged") )
	isMerged = kTRUE;
      else {
	nSubs++;
	maxSubs = TMath::Max(currSub.Atoi(), maxSubs);
      }
    }
    delete subRunList;
    if ( isMerged ) {
      TString fullFilename = Form("%s/%s/%s",baseOutDir.Data(),runNum.Data(),outFilename.Data());
      fullFilename.ReplaceAll("//","/");
      TString mergedFilename = fullFilename;
      mergedFilename.ReplaceAll(".root","-merged.root");
      command = Form("mv %s %s",fullFilename.Data(),mergedFilename.Data());
      if ( ! moveAll ) {
	cout << endl << command.Data() << " ? [y/n/a]" << endl;
	cin >> answer;
	if ( ! answer.CompareTo("a") ) {
	  moveAll = kTRUE;
	  answer = "y";
	}
      }
      if ( ! answer.CompareTo("y") )
	gGrid->Command(command);
    }
    else
      problematicRuns.Append(Form("%s ", runNum.Data()));
  }
  if ( problematicRuns.IsNull() )
    printf("\nAll files succesfully moved!\n");
  else
    printf("\nThe following runs were not touched:\n%s\n",problematicRuns.Data());
}

//______________________________________________________
void gridDeleteMerged(TString baseOutDir, TString outFilename="root_archive.zip")
{
  //
  // Delete intermediate steps when merging is over
  // The method is safe since it checks that the
  // current run output is fully merged before deleting
  // the intermediate steps
  //
  
  if ( ! gGrid ) TGrid::Connect("alien://");

  baseOutDir.ReplaceAll("alien://","");
  baseOutDir.ReplaceAll("//","/");
  printf("Querying grid...\n");
  TString command = Form("find %s %s", baseOutDir.Data(), outFilename.Data());
  printf("Command: %s\n", command.Data());
  TGridResult* outFileList = gGrid->Command(command);
  baseOutDir.Prepend("alien://");
  TObjArray finalMerge, subFiles;
  TIter nextFile(outFileList);
  TMap* fileMap = 0x0;
  while ( ( fileMap = (TMap*)nextFile() ) ) {
    TObjString* filePathObj = dynamic_cast<TObjString*>(fileMap->GetValue("turl"));
    if ( ! filePathObj ) continue;
    TString filePath = filePathObj->GetString();
    filePath.ReplaceAll(baseOutDir.Data(),"");
    if ( filePath.BeginsWith("/") ) filePath.Remove(0,1);
    TObjArray* dirs = filePath.Tokenize("/");
    dirs->SetOwner();
    if ( dirs->GetEntries() == 2 ) finalMerge.AddLast(filePathObj);
    else if ( dirs->GetEntries() == 3 || dirs->GetEntries() == 4 ) subFiles.AddLast(filePathObj);
    else printf("Strange path found %s\nNothing done\n", filePath.Data());
    delete dirs;
  } // loop on files

  Bool_t yesToAll = kFALSE;
  for ( Int_t ifile=0; ifile<finalMerge.GetEntries(); ifile++ ) {
    TString mergedPath = finalMerge.At(ifile)->GetName();
    mergedPath.ReplaceAll(outFilename.Data(),"");
    TString xmlStageFile = mergedPath;
    xmlStageFile.ReplaceAll("alien://","");
    TString xmlStageFileList = gSystem->GetFromPipe(Form("alien_ls %sStage*.xml 2>/dev/null", xmlStageFile.Data()));
    if ( ! xmlStageFileList.IsNull() ) {
      //xmlStageFileList.ReplaceAll("\n"," ");
      //PerformAction(Form("rm %s", xmlStageFileList.Data()), yesToAll);
      PerformAction(Form("rm %sStage*.xml", xmlStageFile.Data()), yesToAll);
    }
    TString lastStage = "dummy";
    for ( Int_t isub=0; isub<subFiles.GetEntries(); isub++ ) {
      TString subFilePath = subFiles.At(isub)->GetName();
      if ( ! subFilePath.Contains(mergedPath.Data()) ) continue;
      subFilePath.ReplaceAll(outFilename.Data(),"");
      subFilePath.ReplaceAll("alien://","");
      if ( subFilePath.Contains("Stage") ) {
        subFilePath.Remove(subFilePath.Index("Stage")+7);
        if ( subFilePath.Contains(lastStage.Data() ) ) continue;
        lastStage = subFilePath;
      }
      PerformAction(Form("rmdir %s", subFilePath.Data()), yesToAll);
    }
  }
}


//______________________________________________________
void gridCleanPartialOut(TString baseOutDir, TString outFilename="AnalysisResults.root")
{
  //
  // In normal output, the files in the archive are listed
  // together with the root_archive itself.
  // If this is not the case, the files in the archive are useless
  // In that case, remove the archive.
  //

  TString archiveName = "root_archive.zip";

  if ( ! gGrid ) TGrid::Connect("alien://");

  baseOutDir.ReplaceAll("//","/");

  printf("Querying grid...\n");
  TString command = Form("find %s %s", baseOutDir.Data(), outFilename.Data());
  printf("Command: %s\n", command.Data());
  TGridResult* outFileList = gGrid->Command(command);

  command = Form("find %s %s", baseOutDir.Data(), archiveName.Data());
  printf("Command: %s\n", command.Data());
  TGridResult* outArchiveList = gGrid->Command(command);

  printf("\n");

  TIter nextArchive(outArchiveList);
  TMap* archiveMap = 0x0;
  Bool_t yesToAll = kFALSE;

  TIter nextFile(outFileList);
  TMap* fileMap = 0x0;
  while ( ( archiveMap = (TMap*)nextArchive() ) ) {
    TObjString* archivePathObj = dynamic_cast<TObjString*>(archiveMap->GetValue("turl"));
    if ( ! archivePathObj ) continue;
    TString archivePath = archivePathObj->GetString();
    nextFile.Reset();
    Bool_t foundFile = kFALSE;
    while ( ( fileMap = (TMap*)nextFile() ) ) {
      TObjString* filePathObj = dynamic_cast<TObjString*>(fileMap->GetValue("turl"));
      if ( ! filePathObj ) continue;
      TString filePath = filePathObj->GetString();
      filePath.ReplaceAll(outFilename.Data(), archiveName.Data());
      if ( ! filePath.CompareTo(archivePath.Data()) ) {
	foundFile = kTRUE;
	break;
      }
    } // loop on output files
    if ( ! foundFile ) {
      archivePath.ReplaceAll("alien://","");
      command = Form("rm %s", archivePath.Data());
      PerformAction(command, yesToAll);
    }
  } // loop on output archives   
}


//______________________________________________________
void gridGrep ( TString matchPattern, TString baseOutDir, TString outFilename="sim.log" )
{
  //
  // Implement a "grep" command to search for pattern in log
  // CAVEAT: matchPattern must contain wildcards "*"
  //
  
  if ( ! gGrid ) TGrid::Connect("alien://");
  
  TString tmpFilename = "/tmp/tmp_" + outFilename;
  
  baseOutDir.ReplaceAll("alien://","");
  baseOutDir.ReplaceAll("//","/");
  printf("Querying grid...\n");
  TString command = Form("find %s %s", baseOutDir.Data(), outFilename.Data());
  printf("Command: %s\n", command.Data());
  TGridResult* outFileList = gGrid->Command(command);
  baseOutDir.Prepend("alien://");
  TIter nextFile(outFileList);
  TMap* fileMap = 0x0;
  TString currLine;
  TList matchFileList;
  matchFileList.SetOwner();
  while ( ( fileMap = static_cast<TMap*>(nextFile()) ) ) {
    TObjString* filePathObj = dynamic_cast<TObjString*>(fileMap->GetValue("turl"));
    if ( ! filePathObj ) continue;
    TString filePath = filePathObj->GetString();
    gSystem->Exec(Form("alien_cp %s %s",filePath.Data(),tmpFilename.Data()));
    ifstream inFile(tmpFilename.Data());
    if ( ! inFile.is_open() ) continue;
    while ( ! inFile.eof() ) {
      currLine.ReadLine(inFile);
      if ( currLine.Contains(TRegexp(matchPattern.Data(),kTRUE)) ) {
        matchFileList.Add(new TObjString(filePath));
        break;
      }
    }
    inFile.close();
  } // loop on files
  
  if ( ! gSystem->AccessPathName(tmpFilename.Data()) ) {
    gSystem->Exec(Form("rm %s",tmpFilename.Data()));
  }
  
  TIter nextMatched(&matchFileList);
  TObject* currFileName = 0x0;
  printf("\nMatching files list:\n");
  while ( ( currFileName = nextMatched() ) ) {
    printf("%s\n", currFileName->GetName());
  }
}


//______________________________________________________
void gridMv ( TString what, TString into )
{
  //
  // Move a directory in grid
  // While moving file is easy, moving a directory into another is not allowed
  //
  
  if ( ! gGrid ) TGrid::Connect("alien://");
  Bool_t yesToAll = kFALSE;
  
  what.ReplaceAll("alien://","");
  what.ReplaceAll("//","/");
  into.ReplaceAll("alien://","");
  into.ReplaceAll("//","/");
  if ( ! into.EndsWith("/") ) into.Append("/");
  
  TString baseDir = gSystem->DirName(what.Data());
  
//  // Get common path
//  TObjArray* arr = into.Tokenize("/");
//  TString tmpPath = "", commonPath = "";
//  for ( Int_t iarr=0; iarr<arr->GetEntries(); iarr++ ) {
//    tmpPath.Append("/");
//    tmpPath += arr->At(iarr)->GetName();
//    if ( what.Contains(tmpPath.Data()) ) {
//      commonPath = tmpPath;
//    }
//    else break;
//  }
//  delete arr;
  
//  printf("Common path %s\n", commonPath.Data());
  
  printf("Querying grid...\n");
  TString command = Form("find %s *", what.Data());
  printf("Command: %s\n", command.Data());
  TGridResult* outFileList = gGrid->Command(command);
  TIter nextFile(outFileList);
  TMap* fileMap = 0x0;
  THashList outDirList;
  outDirList.SetOwner();
  while ( ( fileMap = static_cast<TMap*>(nextFile()) ) ) {
    TObjString* filePathObj = dynamic_cast<TObjString*>(fileMap->GetValue("turl"));
    if ( ! filePathObj ) continue;
    TString filePath = filePathObj->GetString();
    filePath.ReplaceAll("alien://","");
    TString dirName = gSystem->DirName(filePath.Data());
    TString subPath = dirName;
    subPath.ReplaceAll(baseDir.Data(),"");
    TString outDir = into + subPath;
    outDir.ReplaceAll("//","/");
    
    if ( ! outDirList.FindObject(dirName.Data()) ) {
      TObjArray* arr = outDir.Tokenize("/");
      TString tmpPath = "";
      for ( Int_t iarr=0; iarr<arr->GetEntries(); iarr++ ) {
        tmpPath.Append("/");
        tmpPath += arr->At(iarr)->GetName();
        if ( ! DirectoryExists(tmpPath.Data()) ) {
          PerformAction(Form("mkdir %s",tmpPath.Data()),yesToAll);
          outDirList.Add(new TObjString(tmpPath));
        }
      }
      delete arr;
    }
    if ( ! outDir.EndsWith("/") ) outDir.Append("/");
    PerformAction(Form("mv %s %s",filePath.Data(),outDir.Data()),yesToAll);
  }
  PruneEmptyDirs(gSystem->DirName(what.Data()));
}

//______________________________________________________
void gridCleanMergeStages(TString baseOutDir, TString outFilename="AnalysisResults.root")
{
  //
  // OBSOLETE : consider re-submitting with gridFindFailed
  //            until the merging Stage is completed
  //
  // Delete Stage_N.xml file if merging fails
  // (i.e. output Stage_N was not produced)
  // This is needed by the plugin to re-submit the Stage_N
  //

  TString xmlName = "Stage*.xml";

  TGrid::Connect("alien://");

  baseOutDir.ReplaceAll("//","/");

  printf("Querying grid...\n");
  TString command = Form("find %s %s", baseOutDir.Data(), outFilename.Data());
  printf("Command: %s\n", command.Data());
  TGridResult* outFileList = gGrid->Command(command);

  command = Form("find %s %s", baseOutDir.Data(), xmlName.Data());
  printf("Command: %s\n", command.Data());
  TGridResult* outXmlList = gGrid->Command(command);

  printf("\n");

  Bool_t yesToAll = kFALSE;

  TIter nextXml(outXmlList);
  TMap* xmlMap = 0x0;  

  TIter nextFile(outFileList);
  TMap* fileMap = 0x0;
  while ( ( xmlMap = (TMap*)nextXml() ) ) {
    TObjString* xmlPathObj = dynamic_cast<TObjString*>(xmlMap->GetValue("turl"));
    if ( ! xmlPathObj ) continue;
    TString xmlPath = xmlPathObj->GetString();
    xmlName = GetToken(-1, xmlPath);
    TString stageDir = xmlPath;
    stageDir.ReplaceAll(".xml", "/");
    nextFile.Reset();
    Bool_t foundFile = kFALSE;
    while ( ( fileMap = (TMap*)nextFile() ) ) {
      TObjString* filePathObj = dynamic_cast<TObjString*>(fileMap->GetValue("turl"));
      if ( ! filePathObj ) continue;
      TString filePath = filePathObj->GetString();
      TString fileDir = filePath;
      fileDir.ReplaceAll(outFilename.Data(), "");
      fileDir.Remove(fileDir.Length()-4);
      filePath.ReplaceAll(outFilename.Data(), xmlName.Data());
      //printf("\nstageDir %s\nfileDir %s\nstagePath %s\nfilePath %s\n", stageDir.Data(), fileDir.Data(), xmlPath.Data(), filePath.Data()); // REMEMBER TO CUT
      if ( ! filePath.CompareTo(xmlPath.Data()) ||
	   ! fileDir.CompareTo(stageDir.Data())) {
	foundFile = kTRUE;
	break;
      }
    } // loop on output files
    if ( ! foundFile ) {
      xmlPath.ReplaceAll("alien://","");
      command = Form("rm %s", xmlPath.Data());
      PerformAction(command, yesToAll);
    }
  } // loop on output xmls   
}




////////////////////////////////////////////////////
// Internal functions (not to be called by users) //
////////////////////////////////////////////////////

//_______________________________________________________
Double_t GuessFirstJob(TObjArray* jobList)
{
  printf("Guessing first job...\n");
  Double_t previousJob = -1, currJob = -1;
  for ( Int_t ientry=jobList->GetEntries()-1; ientry>=0; ientry-- ) {
    currJob = ((TObjString*)jobList->At(ientry))->GetString().Atof();
    if ( previousJob > 0. && previousJob - currJob > 3000. ) {
      //printf("First job: %.0f\n", previousJob);
      return previousJob;
    }
    previousJob = currJob;
  }
  return -1;
}

//_______________________________________________________
TObjArray* GetMasterList(Bool_t redoPs)
{
  if ( ! gGrid ) TGrid::Connect("alien://");

  printf("Getting the master list...\n");

  TString tmpFilename = tmpFiles[kTmpPsMaster];

  if ( gSystem->AccessPathName(tmpFilename.Data()) )
    redoPs = kTRUE;

  if ( redoPs )
//    gSystem->Exec(Form("alien_ps -M -b > %s", tmpFilename.Data()));
    gSystem->Exec(Form("gbbox 'ps -A' > %s", tmpFilename.Data()));

  TString currLine = "", runNum = "";
  TObjArray* masterList = new TObjArray(1000);
  masterList->SetOwner();
  ifstream inFile(tmpFilename.Data());
  if ( inFile.is_open() ) {
    while ( ! inFile.eof() ) {
      currLine.ReadLine(inFile); // Read line
      runNum = GetToken(1, currLine, " ");
      if ( ! runNum.IsDigit() ) continue;
      masterList->AddLast(new TObjString(runNum));
    }
    inFile.close();
  }
  masterList->Compress();
  return masterList;
}


//_______________________________________________________
TObjArray* GetSubjobInfo(TString masterJob, Bool_t redoPs)
{
  TString tmpFilename = tmpFiles[kTmpMasterjob];

  if ( gSystem->AccessPathName(tmpFilename.Data()) )
    redoPs = kTRUE;

  if ( redoPs ) {
    if ( ! gGrid ) TGrid::Connect("alien://");
    gSystem->Exec(Form("gbbox masterJob %s -printid &> %s", masterJob.Data(), tmpFilename.Data()));
  }

  TString keyNames[2] = {"is in status:", "Subjobs in "};
  TString currLine = "";
  TObjArray* statusList = new TObjArray(20);
  statusList->SetOwner();
  ifstream inFile(tmpFilename.Data());
  if ( inFile.is_open() ) {
    while ( ! inFile.eof() ) {
      currLine.ReadLine(inFile); // Read line
      for ( Int_t ikey=0; ikey<2; ikey++ ) {
        if ( ! currLine.Contains(keyNames[ikey].Data()) ) continue;
        currLine.Remove(0,currLine.Index(keyNames[ikey].Data())+keyNames[ikey].Length());
        currLine.Remove(TString::kLeading,' ');
        currLine.Remove(TString::kTrailing,' ');
        if ( ikey == 0 ) {
          currLine.Append(Form(" | %s", masterJob.Data()));
        }
        else {
          currLine.ReplaceAll("(ids:","|");
          currLine.ReplaceAll(")","");
        }
        statusList->AddLast(new TObjString(currLine));
        break;
      } // loop on keys
    }
    inFile.close();
  }
  statusList->Compress();
  return statusList;
}


//_______________________________________________________
TObjArray* GetSubjobList(TString subjobInfo)
{
  TString subRuns = GetToken(1, subjobInfo, "|");
  TObjArray* subRunList = subRuns.Tokenize(",");
  subRunList->SetOwner();
  return subRunList;
}


//_______________________________________________________
Int_t GetNkilledJobs(TString masterjobId, Bool_t redoPs)
{
  Int_t nKilledJobs = 0;
  TString tmpFilename = tmpFiles[kTmpPsTrace];

  if ( gSystem->AccessPathName(tmpFilename.Data()) )
    redoPs = kTRUE;

  if ( redoPs ) {
    if ( ! gGrid ) TGrid::Connect("alien://");
    gSystem->Exec(Form("alien_ps -trace %s all &> %s", masterjobId.Data(), tmpFilename.Data()));
  }

  TString currLine = "";
  ifstream inFile(tmpFilename.Data());
  if ( inFile.is_open() ) {
    while ( ! inFile.eof() ) {
      currLine.ReadLine(inFile); // Read line
      /*
        if ( currLine.Contains(".xml") ) {
        GetToken(4, currLine, xmlFilename, ":");
        currLine = xmlFilename;
        GetToken(0, currLine, xmlFilename, ",");
        continue;
        }
      */
      if ( currLine.Contains("killing") )
        nKilledJobs++;
    } // loop on file lines
    inFile.close();
  }
  return nKilledJobs;
}


//_______________________________________________________
Double_t GetRunNumber(TString masterjobId, Bool_t redoPs)
{
  TString runNumber = "";
  TString tmpFilename = tmpFiles[kTmpPsTrace];

  if ( gSystem->AccessPathName(tmpFilename.Data()) )
    redoPs = kTRUE;

  if ( redoPs ) {
    if ( ! gGrid ) TGrid::Connect("alien://");
    gSystem->Exec(Form("alien_ps -trace %s all &> %s", masterjobId.Data(), tmpFilename.Data()));
  }

  TString currLine = "";
  ifstream inFile(tmpFilename.Data());
  if ( inFile.is_open() ) {
    while ( ! inFile.eof() ) {
      currLine.ReadLine(inFile); // Read line
      if ( ! currLine.Contains(".xml") ) continue;
      // Get the full filename
      runNumber = GetToken(4, currLine, ":");
      currLine = runNumber;
      runNumber = GetToken(0, currLine, ",");
      // Strip the path
      currLine = runNumber;
      if ( currLine.Contains("Stage_") )
        runNumber = GetToken(-2, currLine, "/");
      else {
        runNumber = GetToken(-1, currLine, "/");
        // Strip the .xml
        runNumber.ReplaceAll(".xml","");
        break;
      }
    } // loop on file lines
    inFile.close();
  }
  return runNumber.Atof();
}


//_______________________________________________________
void GetOutDirs(TString baseOutDir, TString& outDirList, TString outFilename)
{
  
  TString command = Form("find %s %s", baseOutDir.Data(), outFilename.Data());
  printf("Command: %s\n", command.Data());
  TGridResult* outFileList = gGrid->Command(command);

  TIter next(outFileList);
  TMap* map = 0x0;
  while ( ( map = (TMap*)next() ) ) {
    TObjString *objs = dynamic_cast<TObjString*>(map->GetValue("turl"));
    if ( ! objs ) continue;
    TString filePath = objs->GetString();
    filePath.ReplaceAll("alien://","");
    filePath.ReplaceAll(outFilename.Data(), "");
    if ( ! outDirList.IsNull() ) outDirList.Append(" ");
    outDirList.Append(filePath.Data());
  } // loop on results
  delete outFileList;
}


//_______________________________________________________
TString GetOutDirInJdl(TString subjobId, Bool_t redoPs)
{

  TString outDir = "";
  
  TString tmpFilename = tmpFiles[kTmpPsJdl];

  if ( gSystem->AccessPathName(tmpFilename.Data()) )
    redoPs = kTRUE;

  if ( redoPs ) {
    if ( ! gGrid ) TGrid::Connect("alien://");
    gSystem->Exec(Form("alien_ps -jdl %s &> %s", subjobId.Data(), tmpFilename.Data()));
  }

  TString currLine = "";
  ifstream inFile(tmpFilename.Data());
  if ( inFile.is_open() ) {
    while ( ! inFile.eof() ) {
      currLine.ReadLine(inFile);
      if ( ! currLine.Contains("OutputDir") ) continue;
      outDir = GetToken(1, currLine, "\"");
      outDir.ReplaceAll("//","/");
      break;
    }
    inFile.close();
  }
  
  return outDir;
}


//_______________________________________________________
TString GetToken(Int_t ientry, TString inString, TString delimiter)
{
  TString outString = "";

  TObjArray* objArray = inString.Tokenize(delimiter.Data());

  Int_t currEntry = ientry;
  if ( ientry < 0 ) currEntry = objArray->GetEntries() + ientry;

  if ( currEntry < objArray->GetEntries() && currEntry >= 0 ) {
    outString = ((TObjString*)objArray->At(currEntry))->GetString();
  }

  delete objArray;

  return outString;
}

//_______________________________________________________
TString GetSubPath(Int_t ientry, TString inString)
{
  TString outString = "";
  TObjArray* objArray = inString.Tokenize("/");
  
  Int_t first = ( ientry < 0 ) ? 0 : ientry;
  Int_t last = objArray->GetEntries() - 1;
  if ( ientry < 0 ) last += ientry;
  
  for ( Int_t iarr=first; iarr<=last; iarr++ ) {
    outString += Form("/%s",objArray->At(iarr)->GetName());
  }
  
  delete objArray;

  return outString;
}

//_______________________________________________________
Bool_t PerformAction(TString command, Bool_t& yesToAll)
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
    if ( command.Contains("alien_") || command.Contains("gbbox") )
      gSystem->Exec(command.Data());
    else
      gGrid->Command(command.Data());
  }

  return goOn;
}


//_______________________________________________________
Bool_t FileExists(const char *lfn)
{
// Returns true if file exists.
   if (!gGrid) return kFALSE;
   TString slfn = lfn;
   slfn.ReplaceAll("alien://","");
   TGridResult *res = gGrid->Ls(slfn);
   if (!res) return kFALSE;
   TMap *map = dynamic_cast<TMap*>(res->At(0));
   if (!map) {
      delete res;
      return kFALSE;
   }   
   TObjString *objs = dynamic_cast<TObjString*>(map->GetValue("name"));
   if (!objs || !objs->GetString().Length()) {
      delete res;
      return kFALSE;
   }
   delete res;   
   return kTRUE;
}

//______________________________________________________________________________
Bool_t DirectoryExists(const char *dirname)
{
  // Returns true if directory exists. Can be also a path.
  if (!gGrid) return kFALSE;
  // Check if dirname is a path
  TString dirstripped = dirname;
  dirstripped = dirstripped.Strip();
  dirstripped = dirstripped.Strip(TString::kTrailing, '/');
  TString dir = gSystem->BaseName(dirstripped);
  dir += "/";
  TString path = gSystem->DirName(dirstripped);
  TGridResult *res = gGrid->Ls(path, "-F");
  if (!res) return kFALSE;
  TIter next(res);
  TMap *map;
  TObject *obj;
  while ((map=dynamic_cast<TMap*>(next()))) {
    obj = map->GetValue("name");
    if (!obj) break;
    if (dir == obj->GetName()) {
      delete res;
      return kTRUE;
    }
  }
  delete res;
  return kFALSE;
}


//_______________________________________________________
Bool_t PruneEmptyDirs ( TString dirPath, Bool_t yesToAll )
{
  //
  // Remove directory only if empty
  //
  if ( ! gGrid ) TGrid::Connect("alien://");
  dirPath.ReplaceAll("alien://","");
  TString command = Form("find %s *", dirPath.Data());
  printf("Command: %s\n", command.Data());
  TGridResult* outFileList = gGrid->Command(command);
  if ( ! outFileList || outFileList->GetEntries() == 0 ) {
    PerformAction(Form("rmdir %s",dirPath.Data()),yesToAll);
    return kTRUE;
  }
  
  outFileList = gGrid->Ls(dirPath.Data(),"-F");
  TIter next(outFileList);
  TMap* map = 0x0;
  while ((map=dynamic_cast<TMap*>(next()))) {
    TObjString* objs = dynamic_cast<TObjString*>(map->GetValue("name"));
    if ( ! objs ) continue;
    TString fileName = objs->GetString();
    if ( ! fileName.EndsWith("/") ) continue;
    objs = dynamic_cast<TObjString*>(map->GetValue("path"));
    TString filePath = objs->GetString() + fileName;
    PruneEmptyDirs(filePath, yesToAll);
  }
  
  return kFALSE;
}

//_______________________________________________________
void CleanTmpFiles()
{
  for ( Int_t ifile=0; ifile<kNtmpFiles; ifile++ ) {
    if ( ! gSystem->AccessPathName(tmpFiles[ifile].Data()) )
      gSystem->Exec(Form("rm %s", tmpFiles[ifile].Data()));
  }
}
