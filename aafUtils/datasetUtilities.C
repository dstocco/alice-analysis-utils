#if !defined(__CINT__) || defined(__MAKECINT__)

#include <Riostream.h>

// ROOT includes
#include "TString.h"
#include "TSystem.h"
#include "TArrayI.h"
#include "TFileCollection.h"
#include "TMath.h"
#include "TFile.h"
#include "TFileInfo.h"
#include "TObjArray.h"
#include "TObjString.h"
#include "THashList.h"

#include "TProof.h" // FIXME: see later
#endif

//______________________________________________________________________________
TString GetRunNumber ( TString queryString )
{
  TString found = "";
  for ( Int_t ndigits=9; ndigits>=6; ndigits-- ) {
    TString sre = "";
    for ( Int_t idigit=0;idigit<ndigits; idigit++ ) sre += "[0-9]";
    found = queryString(TRegexp(sre.Data()));
    if ( ! found.IsNull() ) break;
  }
  return found;
}

//______________________________________________________________________________
void getFileCollection ( TString inFilename, TString outFileCollection = "fileCollection.root", TString searchString = "%s", TString aaf = "dstocco@nansafmaster2.in2p3.fr", Bool_t forceUpdate = kTRUE, Bool_t stage = kFALSE )
{
  // If inFilename is a list of run, a search string must be provided so that the dataset is built on the fly
  // e.g.: Find;BasePath=/alice/data/2015/LHC15o/000%s/muon_calo_pass1/AOD/;FileName=AliAOD.Muons.root;"
  gSystem->ExpandPathName(inFilename);

  Bool_t isTmp = kFALSE;
  if ( gSystem->AccessPathName(inFilename) ) {
    isTmp = kTRUE;
    inFilename.ReplaceAll(","," ");
    TObjArray* arr = inFilename.Tokenize(" ");
    inFilename = "tmp_list.txt";
    ofstream outFile(inFilename);
    TIter next(arr);
    TObjString* str = 0x0;
    while ( (str = static_cast<TObjString*>(next())) ) {
      outFile << str->GetName() << endl;
    }
    outFile.close();
    delete arr;
  }

  TProof::Open(aaf.Data(),"masteronly");
  if ( forceUpdate && ! searchString.Contains("ForceUpdate") ) {
    searchString.Append(";ForceUpdate");
    searchString.ReplaceAll(";;",";");
  }

  TList inputList;
  inputList.SetOwner();
  ifstream inFile(inFilename.Data());
  TString currLine = "";
  Int_t nRuns = 0;
  while ( ! inFile.eof() ) {
    currLine.ReadLine(inFile);
    if ( currLine.IsNull() ) continue;
    inputList.Add(new TObjString(currLine));
    nRuns++;
  }
  inFile.close();

  if ( isTmp ) gSystem->Exec(Form("rm %s",inFilename.Data()));

//  freopen (outFilename.Data(),"w",stdout);
//  int backup, newstream;
//  fflush(stdout);
//  int backup = dup(1);
//  int newstream = open(outFilename.Data(), O_WRONLY|O_TRUNC);
//  dup2(newstream, 1);
//  close(newstream);

//  std::ofstream out(outFilename.Data());
//  std::streambuf *coutbuf = std::cout.rdbuf(); //save old buf
//  std::cout.rdbuf(out.rdbuf()); //redirect std::cout to out.txt!

  Bool_t ask = kTRUE;
  Double_t byte2GB(1024*1024*1024);
  Long64_t totalSize = 0;

  TIter next(&inputList);
  Int_t nFull=0, nEmpty=0, nPartial=0;
  Float_t limit = 1.e-4;
  TObjString* str = 0x0;
  TString currSearch = "";
  TFileCollection outFc;
  outFc.SetName("dataset");
  while ( (str = static_cast<TObjString*>(next())) ) {
    currSearch = Form(searchString.Data(),str->GetName());
    currSearch.ReplaceAll(";;",";");
    TFileCollection* fc = gProof->GetDataSet(currSearch.Data());
    Float_t stagedPercentage = fc->GetStagedPercentage();
    Long64_t size = fc->GetTotalSize();
    totalSize += size;
    printf("%s   size %g GB  staged %g%%\n",currSearch.Data(),size/byte2GB,stagedPercentage);
    Bool_t isStaged = kFALSE;
    if ( TMath::Abs(stagedPercentage-100.) < limit ) {
      nFull++;
      isStaged = kTRUE;
    }
    else if ( TMath::Abs(stagedPercentage-0.) < limit ) nEmpty++;
    else nPartial++;
    outFc.Add(fc);
    delete fc;

    if ( stage && ! isStaged ) {
      TString answer = "n";
      if ( ask ) {
        printf("Are you really sure you want to stage the dataset? [y/n]");
        TString answer = "";
        std::cin >> answer;
      }
      if ( answer == "y" ) {
        gProof->RequestStagingDataSet(currSearch.Data());
      }
      ask = kFALSE;
    }
  }

  printf("\nTotal runs %i (expected %i). Size %g GB  Full %i  Empty %i  Partial %i\n",nFull+nEmpty+nPartial,nRuns,totalSize/byte2GB,nFull,nEmpty,nPartial);

  if ( outFileCollection.IsNull() ) return;
  printf("\nWriting the collection to file %s\n",outFileCollection.Data());
  outFc.SaveAs(outFileCollection.Data());

//  fclose(stdout);
//  fflush(stdout);
//  dup2(backup, 1);
//  close(backup);

//  std::cout.rdbuf(coutbuf); //reset to standard output again

}

//______________________________________________________________________________
void changeCollection ( TString inFilename, TString removeFiles/*, TString addFiles = ""*/ )
{
  // Change the collection

  gSystem->ExpandPathName(inFilename);
  TString outFilename = inFilename;
  outFilename.ReplaceAll(".root","_modified.root");
  TFile* file = TFile::Open(inFilename.Data());
  if ( ! file ) {
    printf("Fatal: cannot find %s\n",inFilename.Data());
    return;
  }

  TFileCollection* fc = static_cast<TFileCollection*>(file->Get("dataset"));
  if ( ! fc ) {
    printf("Fatal: cannot find dataset in %s\n",inFilename.Data());
    return;
  }
  TFileCollection outFc;
  outFc.SetName(fc->GetName());

  TObjArray* removeList = removeFiles.Tokenize(",");

  TIter next(fc->GetList());
  TFileInfo* info = 0x0;
  TObject* obj = 0x0;
  while ( (info = static_cast<TFileInfo*>(next())) ) {
    TString filename = info->GetCurrentUrl()->GetFile();
    Bool_t addFile = kTRUE;
    TIter nextRemove(removeList);
    while ( (obj = nextRemove()) ) {
      if ( filename.Contains(obj->GetName()) ) {
        addFile = kFALSE;
        removeList->Remove(obj);
        removeList->Compress();
        break;
      }
    }
    if ( addFile ) {
      printf("Adding file %s\n",filename.Data());
      outFc.Add(info);
    }
  }
  if ( ! removeFiles.IsNull() && removeList->GetEntries() > 0 ) {
    printf("Nothing removed. This is the list of files:");
    fc->Print("F");
    return;
  }
  delete removeList;

  /*
  TObjArray* addList = addFiles.Tokenize(",");
  TIter nextAdd(addList);
  while ( (obj = nextAdd()) ) {
    outFc.Add(obj->GetName());
  }
  delete addList;
   */


  outFc.SaveAs(outFilename.Data());
}


//______________________________________________________________________________
void runNumberToDataset ( TString runListFilename, TString searchString, TString outputDatasetName = "dataset.txt" )
{
  gSystem->ExpandPathName(runListFilename);
  if ( runListFilename.Contains("$") ) {
    printf("Error: cannot find %s\n",runListFilename.Data());
    return;
  }

  TObject* obj = 0x0;
  TObjArray* runList = 0x0;
  if ( gSystem->AccessPathName(runListFilename) ) {
    runListFilename.ReplaceAll(","," ");
    runList = runListFilename.Tokenize(" ");
  }
  else {
    runList = new TObjArray();
    runList->SetOwner();
    ifstream inFile(runListFilename.Data());
    TString currLine = "";
    while ( ! inFile.eof() ) {
      currLine.ReadLine(inFile);
      currLine.ReplaceAll(","," ");
      TObjArray* arr = currLine.Tokenize(" ");
      TIter next(arr);
      while ( (obj = next()) )  {
        runList->Add(new TObjString(obj->GetName()));
      }
      delete arr;
    }
    inFile.close();
  }


  TIter next(runList);
  ofstream outFile(outputDatasetName);
  while ( (obj = next()) ) {
    TString runNum = GetRunNumber(obj->GetName());
    if ( ! runNum.IsNull() ) outFile << Form(searchString,runNum.Atoi()) << endl;
  }
  outFile.close();
  delete runList;
}

//______________________________________________________________________________
void datasetToRunNumber ( TString datasetFilename, TString outputRunListName = "runList.txt" )
{
  gSystem->ExpandPathName(datasetFilename);
  if ( gSystem->AccessPathName(datasetFilename) ) {
    printf("Error: cannot find %s\n",datasetFilename.Data());
  }

  ofstream outFile(outputRunListName);
  ifstream inFile(datasetFilename.Data());
  TString currLine = "";
  while ( ! inFile.eof() ) {
    currLine.ReadLine(inFile);
    TString runNum = GetRunNumber(currLine);
    if ( ! runNum.IsNull() ) outFile << runNum.Atoi() << endl;
  }
  inFile.close();
  outFile.close();
}






