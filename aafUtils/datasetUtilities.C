#if !defined(__CINT__) || defined(__MAKECINT__)

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <memory>

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
#include "TKey.h"
#include "TTree.h"

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

  if ( ! gProof ) TProof::Open(aaf.Data(),"masteronly");
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
  TString answer = "n";
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
      if ( ask ) {
        printf("Are you really sure you want to stage the dataset? [y/n]\n");
        std::cin >> answer;
      }
      if ( answer == "y" ) {
        printf("Dataset staging requested\n");
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
bool IsFileGood(const char* filename, bool readTrees) 
{
  std::unique_ptr<TFile> file(TFile::Open(filename));
  if ( file == nullptr || file->IsZombie() ) {
    return false;
  }

  if ( file->TestBit(TFile::kRecovered) ) {
    readTrees = true;
  }

  if ( ! readTrees ) {
    return true;
  }

  TIter next(file->GetListOfKeys());
  TKey* key;
  while ( (key = static_cast<TKey*>(next())) ) {
    std::string className = key->GetClassName();
    if ( className == "TTree" ) {
      TTree* tree = static_cast<TTree*>(file->Get(key->GetName()));
      if ( ! tree ) {
        return false;
      }
      Long64_t nEntries = tree->GetEntries();
      for ( Long64_t ientry=0; ientry<nEntries; ++ientry ) {
        if ( tree->GetEntry(ientry) < 0 ) {
          return false;
        }
      }
    } 
  }

  return true;
}

//______________________________________________________________________________
bool checkCollection(const char* inFilename, bool readTrees = true) 
{
  /// Check each file of the collection.
  /// Remove them in case of problems
  std::string expanded = gSystem->ExpandPathName(inFilename);

  std::string newFilename = expanded;
  newFilename.replace(expanded.find(".root"),5,"_modified.root");
  if ( gSystem->AccessPathName(newFilename.data()) == 0 ) {
    std::cout << "Output file " << newFilename << " already created. Overwrite? [y/n]" << std::endl;
    std::string answer;
    std::cin >> answer;
    if ( answer != "y" ) {
      std::cout << "Nothing done!" << std::endl;
      return false;
    }
  }
  std::unique_ptr<TFile> file(TFile::Open(expanded.data()));
  TFileCollection* fc = static_cast<TFileCollection*>(file->Get("dataset"));
  if ( ! fc ) {
    printf("Fatal: cannot find dataset in %s\n",inFilename);
    return false;
  }

  // Sometimes the algorithm can badly crash during the check
  // In order not to lose everything, let's keep track of the last checked file
  // as well as the bad files, so that we can restart the check from where we left
  std::map<std::string,bool> fileMap;
  std::string checkedFilesName = inFilename;
  checkedFilesName.insert(0,"tmp_");
  checkedFilesName.replace(checkedFilesName.find(".root"),5,".txt");
  if ( gSystem->AccessPathName(checkedFilesName.data()) == 0) {
    std::string line;
    ifstream inFile(checkedFilesName);
    while(std::getline(inFile,line)) {
      bool isGood = false;
      std::string fname;
      auto idx = line.find(",");
      if ( idx != std::string::npos ) {
        fname = line.substr(0,idx);
        isGood = std::atoi(line.substr(idx+1).data());
      }
      else {
        fname = line;
      }
      fileMap[fname] = isGood;
    }
    inFile.close();
  }

  ofstream checkedFilesOut(checkedFilesName);

  std::vector<TFileInfo> goodFiles;
  Long64_t nFiles = fc->GetList()->GetEntries();
  std::cout << "nFiles: " << nFiles << std::endl;
  Long64_t showProgress = nFiles/10;
  Long64_t nBad = 0;
  Long64_t ifile = 0;
  TFileInfo* info = 0x0;
  TObject* obj = 0x0;
  TIter next(fc->GetList());
  while ( (info = static_cast<TFileInfo*>(next())) ) {
    ++ifile;
    if ( ifile % showProgress == 0 ) {
      std::cout << "Checking file " << ifile << " / " << nFiles << std::endl;
    }
    std::string currentUrl = info->GetCurrentUrl()->GetUrl();
    checkedFilesOut << currentUrl;
    bool isOk = false;
    const auto& found = fileMap.find(currentUrl.data());
    if ( found != fileMap.end() ) {
      isOk = found->second;
    }
    else {
      checkedFilesOut.close();
      isOk = IsFileGood(currentUrl.data(),readTrees);
      checkedFilesOut.open(checkedFilesName, std::ofstream::out | std::ofstream::app);
    }

    checkedFilesOut << "," << isOk << "\n";

    if ( isOk ) {
      goodFiles.emplace_back(*info);
    }
    else {
      ++nBad;
    }
  }

  checkedFilesOut.close();

  if ( nBad == 0 ) {
    std::cout << "All files good: nothing done" << std::endl;
    return true;
  }

  std::cout << "Found " << nBad << " bad files\n";
  std::cout << "Removing them in " << newFilename << std::endl;

  TFileCollection outFc;
  outFc.SetName(fc->GetName());
  for ( auto& finfo : goodFiles ) {
    outFc.Add(&finfo);
  }
  outFc.SaveAs(newFilename.data());

  gSystem->Exec(Form("rm %s",checkedFilesName.data()));
  return false;
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
