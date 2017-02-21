#include "AliTaskSubmitter.h"

#include <sstream>

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
#include "TRegexp.h"
#include "TFileCollection.h"
#include "TApplication.h"
#include "TInterpreter.h"

// STEER includes
#include "AliESDInputHandler.h"
#include "AliAODInputHandler.h"
#include "AliAODHandler.h"
#include "AliMCEventHandler.h"
#include "AliMultiInputEventHandler.h"
//#include "AliLog.h"

// ANALYSIS includes
#include "AliAnalysisManager.h"
#include "AliAnalysisTaskSE.h"
#include "AliAnalysisAlien.h"

#include "AliPhysicsSelection.h"
#include "AliPhysicsSelectionTask.h"
#include "AliCentralitySelectionTask.h"
#include "AliMultSelectionTask.h"

/// \cond CLASSIMP
ClassImp(AliTaskSubmitter) // Class implementation in ROOT context
/// \endcond


//_______________________________________________________
AliTaskSubmitter::AliTaskSubmitter() :
TObject(),
fIsInitOk(0),
fRunMode(0),
fRunModeName(),
fAnalysisMode(0),
fAnalysisModeName(),
fAAF(0),
fIsPod(0),
fIsPodMachine(0),
fFileType(0),
fIsMC(0),
fIsEmbed(0),
fLoadAllBranches(0),
fEventMixing(0),
fTaskDir(),
fCurrentDir(),
fInputName(),
fPeriod(),
fPass(),
fSoftVersion(),
fGridDataDir(),
fGridDataPattern(),
fWorkDir(),
fAliPhysicsBuildDir(),
fProofCluster(),
fProofServer(),
fProofCopyCommand(),
fProofOpenCommand(),
fProofExecCommand(),
fProofDatasetMode(),
fDatasetName(),
fPodOutDir(),
fProofNworkers(0),
fProofResume(0),
fProofSplitPerRun(0),
fLibraries(),
fIncludePaths(),
fSearchPaths(),
fUtilityMacros(),
fRunList(),
fMap()
{
  /// Ctr
}

//_______________________________________________________
AliTaskSubmitter::~AliTaskSubmitter()
{
  /// Dtor
  UnloadMacros();
}

//_______________________________________________________
Bool_t AliTaskSubmitter::AddCentrality ( Bool_t oldFramework ) const
{
  /// Add centrality in the train

  // Old centrality framework
  Bool_t treatAsMC = ( fIsMC && ! fIsEmbed );
  if ( oldFramework ) {
    if ( gROOT->LoadMacro("$ALICE_PHYSICS/OADB/macros/AddTaskCentrality.C") < 0 ) return kFALSE;
    printf("Adding old centrality task\n");
    AliCentralitySelectionTask* centralityTask = reinterpret_cast<AliCentralitySelectionTask*>(gInterpreter->ProcessLine("AddTaskCentrality()"));
    if ( treatAsMC ) centralityTask->SetMCInput();
  }
  else {
    if ( gROOT->LoadMacro("$ALICE_PHYSICS/OADB/COMMON/MULTIPLICITY/macros/AddTaskMultSelection.C") < 0 ) return kFALSE;
    printf("Adding centrality task\n");
    AliMultSelectionTask* centralityTask = reinterpret_cast<AliMultSelectionTask*>(gInterpreter->ProcessLine("AddTaskMultSelection(kFALSE)"));
    if ( treatAsMC ) centralityTask->SetUseDefaultMCCalib(kTRUE); // MC
    else centralityTask->SetUseDefaultCalib(kTRUE); // data
  }

  return kTRUE;
}

//_______________________________________________________
Bool_t AliTaskSubmitter::AddPhysicsSelection () const
{
  /// Add physics selection task in the train

  if ( gROOT->LoadMacro("$ALICE_PHYSICS/OADB/macros/AddTaskPhysicsSelection.C") < 0 ) return kFALSE;

  printf("Adding physics selection task\n");
  Bool_t treatAsMC = ( fIsMC && ! fIsEmbed );

  AliPhysicsSelectionTask* physSelTask = reinterpret_cast<AliPhysicsSelectionTask*>(gInterpreter->ProcessLine(Form("AddTaskPhysicsSelection(%i)",treatAsMC)));
  if ( ! treatAsMC ) physSelTask->GetPhysicsSelection()->SetUseBXNumbers(kFALSE); // Needed if you want to merge runs with different running scheme
  physSelTask->GetPhysicsSelection()->SetPassName(fPass.Data());

  return kTRUE;
}

//_______________________________________________________
Bool_t AliTaskSubmitter::CopyDatasetLocally ()
{
  /// Copy dataset locally
  if ( ! fIsPod ) return kFALSE;
  TString inFilename = fInputName;
  TString tmpFilename = Form("%s/tmp_dataset.txt",fWorkDir.Data());
  if ( gSystem->AccessPathName(fInputName) ) {
    inFilename = tmpFilename;
    gSystem->Exec(Form("echo '%s' > %s",fInputName.Data(),inFilename.Data()));
  }
  else if ( fInputName.EndsWith(".root") ) {
    fInputName = "dataset.root";
    TFile::Cp(inFilename.Data(),Form("%s/%s",fWorkDir.Data(),fInputName.Data()));
    return kFALSE;
  }

  fInputName = "dataset.txt";

  TString fullFilename = Form("%s/%s",fWorkDir.Data(),fInputName.Data());
  ofstream outFile(fullFilename.Data());
  ifstream inFile(inFilename.Data());
  TString currLine = "";
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
          currFind.ReplaceAll(currMode.Data(),fProofDatasetMode.Data());
        }
        else currFind.Append(Form(";Mode=%s;",fProofDatasetMode.Data()));
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

//______________________________________________________________________________
Bool_t AliTaskSubmitter::ConnectToPod () const
{
  if ( ! fIsPod ) return kFALSE;


  Bool_t yesToAll = kTRUE;
  TString remoteDir = Form("%s:%s",fProofServer.Data(),fPodOutDir.Data());
  TString baseExclude = "--exclude=\"*/\" --exclude=\"*.log\" --exclude=\"outputs_valid\" --exclude=\"*.xml\" --exclude=\"*.jdl\" --exclude=\"plugin_test_copy\" --exclude=\"*.so\" --exclude=\"*.d\"";
  TString syncOpt = fProofResume ? "--delete" : "--delete-excluded";
  TString command = Form("%s %s %s ./ %s/",fProofCopyCommand.Data(),syncOpt.Data(),baseExclude.Data(),remoteDir.Data());
  PerformAction(command,yesToAll);
  TString execCommand = fProofExecCommand;
  execCommand.ReplaceAll("nworkers",Form("%i",fProofNworkers));
  TString updateVersion = Form("sed -i \"s/VafAliPhysicsVersion=.*/VafAliPhysicsVersion=%s/\" .vaf/vaf.conf",fSoftVersion.Data());
  gSystem->Exec(Form("%s '%s; %s'",fProofOpenCommand.Data(),updateVersion.Data(),execCommand.Data()));

  return kTRUE;
}


//_______________________________________________________
Bool_t AliTaskSubmitter::CopyFile ( const char* filename ) const
{
  /// Copy file to workdir
  for ( std::string path : fSearchPaths ) {
    TString fullName = Form("%s/%s",path.c_str(),filename);
    if ( gSystem->AccessPathName(fullName.Data()) == 0 ) {
      gSystem->Exec(Form("cp %s %s/",fullName.Data(),fWorkDir.Data()));
      return kTRUE;
    }
  }
  return kFALSE;
}

//______________________________________________________________________________
Bool_t AliTaskSubmitter::CopyPodOutput () const
{
  /// Get Pod output from the server and copy it locally
  if ( fIsPodMachine ) return kFALSE;
  Bool_t yesToAll = kTRUE;
  TString remoteDir = Form("%s:%s",fProofServer.Data(),fPodOutDir.Data());
  PerformAction(Form("%s %s/*.root ./",fProofCopyCommand.Data(),remoteDir.Data()),yesToAll);
//  printf("*/"); // This line is for xcode that gets confused when it sees the slash star on the previous line and breaks the comment shortcut
  return kTRUE;
}


//_______________________________________________________
AliAnalysisAlien* AliTaskSubmitter::CreateAlienHandler () const
{
  AliAnalysisAlien *plugin = new AliAnalysisAlien();

  // Set the run mode
  plugin->SetRunMode(fRunModeName.Data());

  plugin->SetAPIVersion("V1.1x");
  plugin->SetAliPhysicsVersion(fSoftVersion.Data());

  if ( fRunMode == kTestMode ) plugin->SetFileForTestMode(fInputName.Data());

  if ( fRunMode != kTerminateMode ) plugin->SetMergeViaJDL();

  // Set run list
  if ( fRunList.empty() ) {
    printf("\nERROR: the alien plugin expects a run list. This could not be found in the input:\n");
    printf("%s\n",fInputName.Data());
    printf("This might be a custom production...but the plugin will not be able to handle it.\n\n");
    //    printf("It might be that this is a special MC production.\n\n");
    //    printf("Assume that the path you're passing is indeed a file...\n\n");
    //    TString dataFile = Form("%s/%s",dataDir.Data(),dataPattern.Data());
    //    dataFile.ReplaceAll("*","");
    //    plugin->AddDataFile(dataFile.Data())
  }
  else {
    if ( ! fIsMC ) plugin->SetRunPrefix("000");
    for ( Int_t run : fRunList ) plugin->AddRunNumber(run);
  }

  // Set grid work dir (tentatively)
  TString gridWorkDir = "analysis";
  if ( fIsMC ) gridWorkDir = "mcAna";
  else gridWorkDir = "analysis";

  if ( ! fWorkDir.IsNull() && ! fPeriod.IsNull() ) {
    gridWorkDir += Form("/%s/%s",fPeriod.Data(),fWorkDir.Data());
    printf("WARNING: setting a custom grid working dir: %s\n",gridWorkDir.Data());
    plugin->SetGridWorkingDir(gridWorkDir.Data());
  }
  else {
    printf("\nWARNING: GridWorkDir is not set. You have to do it in your macro:\n");
    printf("AliAnalysisAlien* plugin = static_cast<AliAnalysisAlien*>(AliAnalysisManager::GetAnalysisManager()->GetGridHandler());\n");
    printf("if ( plugin ) plugin->SetGridWorkingDir(\"workDirRelativeToHome\");\n\n");
  }

  plugin->SetGridDataDir(fGridDataDir.Data());
  plugin->SetDataPattern(fGridDataPattern.Data());

  plugin->SetCheckCopy(kFALSE); // Fixes issue with alien_CLOSE_SE

  // Set libraries
  for ( std::string str : fIncludePaths ) plugin->AddIncludePath(Form("-I%s",str.c_str()));

  TString extraLibs = "", extraSrcs = "";
  for ( std::string str : fLibraries ) {
    TString currName = gSystem->BaseName(str.c_str());
    if ( currName.EndsWith(".cxx") ) {
      extraSrcs += Form("%s ", currName.Data());
      TString auxName = currName;
      auxName.ReplaceAll(".cxx",".h");
      extraLibs += Form("%s %s ", auxName.Data(), currName.Data());
    }
    else if ( currName.EndsWith(".par") ) plugin->EnablePackage(currName.Data());
    else if ( currName.EndsWith(".so") ) extraLibs += Form("%s ", currName.Data());
  }

  plugin->SetAnalysisSource(extraSrcs.Data());
  plugin->SetAdditionalLibs(extraLibs.Data());
  plugin->SetAdditionalRootLibs("libGui.so libProofPlayer.so libXMLParser.so");

  plugin->SetOutputToRunNo();
  plugin->SetNumberOfReplicas(2);
  plugin->SetDropToShell(kFALSE);

  return plugin;
}

//_______________________________________________________
TObject* AliTaskSubmitter::CreateInputObject () const
{
  /// Create input object
  if ( fAnalysisMode == kLocalAnalysis ) {
    TString treeName = ( fFileType == kAOD ) ? "aodTree" : "esdTree";

    TChain* chain = new TChain(treeName.Data());
    if ( fInputName.EndsWith(".root") ) chain->Add(fInputName.Data());
    else {
      ifstream inFile(fInputName.Data());
      TString inFileName;
      if (inFile.is_open())
      {
        while (! inFile.eof() )
        {
          inFileName.ReadLine(inFile,kFALSE);
          if ( !inFileName.EndsWith(".root") ) continue;
          chain->Add(inFileName.Data());
        }
      }
      inFile.close();
    }
    if (chain) chain->GetListOfFiles()->ls();
    return chain;
  }
  else if ( fAnalysisMode == kAAFAnalysis ) {
    if ( fIsPod && ! fIsPodMachine ) return NULL;
    TString outName = "";
    TFileCollection* fc = 0x0;
    if ( fInputName.EndsWith(".root") ) {
      // Assume this is a collection
      TFile* file = TFile::Open(fInputName.Data());
      fc = static_cast<TFileCollection*>(file->FindObjectAny("dataset"));
      file->Close();
    }
    else if ( fRunMode == kTestMode ) {
      fc = new TFileCollection("dataset");
      fc->AddFromFile(fInputName.Data());
      //      outName = "test_collection";
      //      gProof->RegisterDataSet(outName.Data(), coll, "OV");
    }
    if ( fc ) return fc;

    if ( fAAF == kCernVaf ) outName = gSystem->GetFromPipe(Form("cat %s",fInputName.Data()));
    else outName = fInputName.Data();

    TObjString *output = new TObjString(outName);
    return output;
  }
  return NULL;
}


//_______________________________________________________
TString AliTaskSubmitter::GetAbsolutePath ( const char* path ) const
{
  /// Get absolute path
  TString currDir = gSystem->pwd();
  if ( gSystem->AccessPathName(path) ) {
    printf("Error: cannot access %s\n",path);
    return "";
  }
  return gSystem->GetFromPipe(Form("cd %s; pwd; cd %s",path,currDir.Data()));
}

//______________________________________________________________________________
TString AliTaskSubmitter::GetGridDataDir ( TString queryString ) const
{
  /// Get data dir for grid
  TString basePath = GetGridQueryVal(queryString,"BasePath");
  if ( basePath.IsNull() ) return "";
  TString runNum = GetRunNumber(queryString);
  if ( ! runNum.IsNull() ) {
    Int_t idx = basePath.Index(runNum);
    basePath.Remove(idx-1);
  }
  return basePath;
}

//______________________________________________________________________________
TString AliTaskSubmitter::GetGridDataPattern ( TString queryString ) const
{
  /// Get data pattern for grid
  TString basePath = GetGridQueryVal(queryString,"BasePath");
  TString fileName = GetGridQueryVal(queryString,"FileName");
  if ( basePath.IsNull() || fileName.IsNull() ) return "";
  fileName.Prepend("*");
  TString runNum = GetRunNumber(queryString);
  if ( ! runNum.IsNull() ) {
    Int_t idx = basePath.Index(runNum) + runNum.Length();
    basePath.Remove(0,idx+1);
    if ( ! basePath.IsNull() ) {
      if ( ! basePath.EndsWith("/") ) basePath.Append("/");
      fileName.Prepend(basePath.Data());
    }
  }

  return fileName;
}

//______________________________________________________________________________
TString AliTaskSubmitter::GetGridQueryVal ( TString queryString, TString keyword ) const
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
  if ( found.IsNull() && fAnalysisMode == kGridAnalysis ) printf("Warning: cannot find %s in %s\n",keyword.Data(),queryString.Data());
  return found;
}

//______________________________________________________________________________
TMap* AliTaskSubmitter::GetMap ()
{
  /// Get map with values to be passed to macros
  /// This is kept for backward compatibility
  if ( fMap.IsEmpty() ) {
    fMap.SetOwner();
    fMap.Add(new TObjString("period"),new TObjString(fPeriod));
    TString dataType = fIsMC ? "MC" : "DATA";
    fMap.Add(new TObjString("dataType"),new TObjString(dataType));
    TString dataTypeDetails = fIsEmbed ? "EMBED" : "FULL";
    fMap.Add(new TObjString("mcDetails"),new TObjString(dataTypeDetails));
    TString physSel = "NO", centr = "NO";
    AliAnalysisManager* mgr = AliAnalysisManager::GetAnalysisManager();
    if ( mgr ) {
      if ( mgr->GetTask("AliPhysicsSelectionTask") ) physSel = "YES";
      if ( mgr->GetTask("taskMultSelection") || mgr->GetTask("CentralitySelection") ) centr = "YES";
    }
    fMap.Add(new TObjString("physicsSelection"),new TObjString(physSel));
    fMap.Add(new TObjString("centrality"),new TObjString(centr));
  }

  return &fMap;
}

//______________________________________________________________________________
TString AliTaskSubmitter::GetPass ( TString checkString ) const
{
  /// Get pass name in string
  TPRegexp re("(^|/| )(pass|muon_calo).*?(/|;| |$)");
  checkString.ReplaceAll("*",""); // Add protection for datasets with a star inside
  TString found = checkString(re);
  if ( ! found.IsNull() ) found = found(TRegexp("[a-zA-Z_0-9]+"));
  return found;
}

//______________________________________________________________________________
TString AliTaskSubmitter::GetPeriod ( TString checkString ) const
{
  /// Get period in string
  return checkString(TRegexp("LHC[0-9][0-9][a-z]"));
}

//_______________________________________________________
TString AliTaskSubmitter::GetRunMacro ( ) const
{
  /// Get name of runnin macro
  TString rootCmd = gSystem->GetFromPipe("tail -n 1 $HOME/.root_hist");
  rootCmd.ReplaceAll("  "," ");
  rootCmd.ReplaceAll(".x ","");
  rootCmd.Remove(TString::kLeading,' ');
  rootCmd.Remove(TString::kTrailing,' ');
  return rootCmd;
}


//______________________________________________________________________________
TString AliTaskSubmitter::GetRunNumber ( TString checkString ) const
{
  /// Get run number in string
  TPRegexp re("(^|/)[0-9][0-9][0-9][0-9][0-9][0-9]+(/|;|$)");
  checkString.ReplaceAll("*",""); // Add protection for datasets with a star inside
  TString found = checkString(re);
  if ( ! found.IsNull() ) {
    found = found(TRegexp("[0-9]+"));
    if ( found.Length() == 6 || (found.Length() == 9 && found.BeginsWith("0")) ) return found;
  }
  return "";
}

//_______________________________________________________
Bool_t AliTaskSubmitter::Init ( const char* workDir )
{
  /// Initialize analysis

  if ( fFileType < 0 ) return kFALSE;

  fUtilityMacros.clear();
  fUtilityMacros.push_back("SetAlienIO");
  fUtilityMacros.push_back("BuildMuonEventCuts");
  fUtilityMacros.push_back("SetupMuonBasedTasks");

  if ( ! SetupLocalWorkDir ( workDir ) ) return kFALSE;

  if ( fIsPodMachine ) fInputName = gSystem->GetFromPipe("ls dataset.*");

  gSystem->cd(fWorkDir.Data());
  if ( ! fIsPodMachine ) LoadLibsLocally();

  AliAnalysisAlien* plugin = 0x0;
  if ( fAnalysisMode == kGridAnalysis ) {
    plugin = CreateAlienHandler();
    if ( LoadMacro(fUtilityMacros[0].c_str()) ) {
      gInterpreter->ProcessLine(Form("%s(\"\",\"%s\",(AliAnalysisAlien*)%p)",fUtilityMacros[0].c_str(),fPeriod.Data(),plugin));
      UnloadMacros();
    }
  }
  else if ( fAnalysisMode == kAAFAnalysis ) {
    if ( fProofNworkers == 0 ) SetProofNworkers();
    if ( ! fIsPod || fIsPodMachine ) LoadLibsProof();
  }

  /// Some utilities for muon analysis
  if ( ! fLoadAllBranches ) {
    LoadMacro(fUtilityMacros[1].c_str());
    LoadMacro(fUtilityMacros[2].c_str());
  }

  //
  // Make the analysis manager
  AliAnalysisManager *mgr = new AliAnalysisManager("testAnalysis");
  ////  PerformAction(Form("cd %s",outDir.Data()),yesToAll);
  if ( plugin ) mgr->SetGridHandler(plugin);


  AliInputEventHandler *handler = 0x0;
  AliMCEventHandler *mcHandler = 0x0;

  if ( fFileType == kAOD ) handler = new AliAODInputHandler();
  else {
    AliESDInputHandler* esdH = new AliESDInputHandler();
    if ( ! fLoadAllBranches ) {
      esdH->SetReadFriends(kFALSE);
      esdH->SetInactiveBranches("*");
      esdH->SetActiveBranches("MuonTracks MuonClusters MuonPads AliESDRun. AliESDHeader. AliMultiplicity. AliESDFMD. AliESDVZERO. AliESDTZERO. SPDVertex. PrimaryVertex. AliESDZDC. SPDPileupVertices");
    }
    handler = esdH;

    if ( fIsMC ){
      // Monte Carlo handler
      mcHandler = new AliMCEventHandler();
      printf("\nMC event handler requested\n\n");
    }
  }
  if ( fEventMixing ) {
    AliMultiInputEventHandler* multiHandler = new AliMultiInputEventHandler();
    mgr->SetInputEventHandler(multiHandler);
    multiHandler->AddInputEventHandler(handler);
    if ( mcHandler ) multiHandler->AddInputEventHandler(mcHandler);
  }
  else {
    mgr->SetInputEventHandler(handler);
    if ( mcHandler ) mgr->SetMCtruthEventHandler(mcHandler);
  }

  Bool_t treatAsMC = ( fIsMC && ! fIsEmbed );

  printf("Analyzing %s  MC %i\n", ( fFileType == kAOD ) ? "AODs" : "ESDs", fIsMC);

  fIsInitOk = kTRUE;
  return kTRUE;
}

//_______________________________________________________
Bool_t AliTaskSubmitter::LoadLibsLocally ( ) const
{
  /// Load libraries and classes
  for ( std::string str : fIncludePaths ) gSystem->AddIncludePath(Form("-I%s",str.c_str()));

  for ( std::string str : fLibraries ) {
    TString currName = gSystem->BaseName(str.c_str());
    if ( currName.EndsWith(".so") ) gSystem->Load(currName.Data());
    else if ( currName.EndsWith(".cxx") ) gROOT->LoadMacro(Form("%s+g",currName.Data()));
    else if ( currName.EndsWith(".par") ) {
      currName.ReplaceAll(".par","");
      // The following line is needed since we use AliAnalysisAlien::SetupPar in this function.
      // The interpreter reads the following part line-by-line, even if the condition is not satisfied.
      // If the library is not loaded in advance, one gets funny issues at runtime
      TString foundLib = gSystem->GetLibraries("libANALYSISalice.so","",kFALSE);
      if ( foundLib.IsNull() ) gSystem->Load("libANALYSISalice.so");
//      // Then search for the library. If it is already loades, unload it
//      TString libName = Form("lib%s.so",currName.Data());
//      foundLib = gSystem->GetLibraries(libName.Data(),"",kFALSE);
//      if ( ! foundLib.IsNull() ) gSystem->Unload(libName.Data());
      AliAnalysisAlien::SetupPar(currName.Data());
    }
  }
  return kTRUE;
}

//_______________________________________________________
Bool_t AliTaskSubmitter::LoadLibsProof ( ) const
{
  /// Load libraries on proof
  //  if ( ! fIsPod ) {
  //    TString rootVersion = GetSoftVersion("root",softVersions);
  //    rootVersion.Prepend("VO_ALICE@ROOT::");
  //    TProof::Mgr(fProofCluster.Data())->SetROOTVersion(rootVersion.Data());
  //  }
  TProof::Open(fProofCluster.Data());

  if (!gProof) return kFALSE;

  TString extraIncs = "";
  for ( std::string str : fIncludePaths ) extraIncs += Form("%s:",str.c_str());
  extraIncs.Remove(TString::kTrailing,':');

  TString extraLibs = "";
  std::vector<std::string> extraPkgs, extraSrcs;
  for ( std::string str : fLibraries ) {
    TString currName = str.c_str();
    if ( ! currName.EndsWith(".so") ) continue;
    if ( currName.BeginsWith("lib") ) currName.Remove(0,3);
    currName.ReplaceAll(".so","");
    extraLibs += Form("%s:",currName.Data());
  }
  extraLibs.Remove(TString::kTrailing,':');

  TString alirootMode = "base";
  Bool_t notOnClient = kFALSE;

  TList* list = new TList();
  list->Add(new TNamed("ALIROOT_MODE", alirootMode.Data()));
  list->Add(new TNamed("ALIROOT_EXTRA_LIBS", extraLibs.Data()));
  list->Add(new TNamed("ALIROOT_EXTRA_INCLUDES", extraIncs.Data()));
  if ( fAAF != kSAF ) // Temporary fix for saf3: REMEMBER TO CUT this line when issue fixed
    list->Add(new TNamed("ALIROOT_ENABLE_ALIEN", "1"));
  TString mainPackage = "";
  if ( fIsPod ) {
    TString remotePar = ( fAAF == kSAF ) ? "https://github.com/aphecetche/aphecetche.github.io/blob/master/page/saf3-usermanual/AliceVaf.par?raw=true" : "http://alibrary.web.cern.ch/alibrary/vaf/AliceVaf.par";
    mainPackage = gSystem->BaseName(remotePar.Data());
    mainPackage.Remove(mainPackage.Index("?"));
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
  else if ( fRunMode == kTestMode ) mainPackage = "$ALICE_ROOT/ANALYSIS/macros/AliRootProofLite.par";
  else {
    mainPackage = fSoftVersion;
    mainPackage.Prepend("VO_ALICE@AliPhysics::");
  }
  if ( ! mainPackage.BeginsWith("VO_ALICE") ) gProof->UploadPackage(mainPackage.Data());
  gProof->EnablePackage(mainPackage.Data(),list,notOnClient);

  for ( std::string str : fLibraries ) {
    if ( str.rfind(".par") != std::string::npos ) {
      gProof->UploadPackage(str.c_str());
      gProof->EnablePackage(str.c_str(),notOnClient);
    }
    else if ( str.rfind(".cxx") != std::string::npos ) {
      gProof->Load(Form("%s++g",str.c_str()),notOnClient);
    }
  }
  return kTRUE;
}

//_______________________________________________________
Bool_t AliTaskSubmitter::LoadMacro ( const char* macroName ) const
{
  /// Load macro
  TString foundLib = gSystem->GetLibraries(macroName,"",kFALSE);
  if ( ! foundLib.IsNull() ) return kTRUE;
  TString macroFile = Form("%s/%s.C",fWorkDir.Data(),macroName);
  if ( gSystem->AccessPathName(macroFile.Data()) ) return kFALSE;
  gROOT->LoadMacro(Form("%s+",macroFile.Data()));
  return kTRUE;
}

//_______________________________________________________
Bool_t AliTaskSubmitter::PerformAction ( TString command, Bool_t& yesToAll ) const
{
  /// Do something

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
Bool_t AliTaskSubmitter::SetAliPhysicsBuildDir ( const char* aliphysicsBuildDir )
{
  /// Setup aliphysics build dir
  /// This will be used to produce the par file if needed
  TString buildDir(aliphysicsBuildDir);
  gSystem->ExpandPathName(buildDir);
  fAliPhysicsBuildDir = GetAbsolutePath(buildDir);
  return ( ! fAliPhysicsBuildDir.IsNull() );
}

//_______________________________________________________
Bool_t AliTaskSubmitter::SetInput ( const char* inputName, const char* inputOptions )
{
  /// Set input
  fInputName = inputName;
  gSystem->ExpandPathName(fInputName);
  TString sOpt(inputOptions);

  fRunList.clear();

  std::vector<TString> fileList;

  if ( gSystem->AccessPathName(fInputName.Data()) == 0 ) {
    if ( fInputName.EndsWith(".root") ) fileList.push_back(fInputName);
    else {
      ifstream inFile(fInputName.Data());
      TString currLine = "";
      while (!inFile.eof()) {
        currLine.ReadLine(inFile);
        if ( currLine.IsNull() ) continue;
        fileList.push_back(currLine);
      }
    }
  }
  else {
    if ( ! fInputName.BeginsWith("Find;") ) {
      if ( ! ( fIsPod && fIsPodMachine ) ) {
        printf("Error: inputName (%s) must be a local file or a remote file in the form: Find;BasePath=...;FileName=...\n",fInputName.Data());
        return kFALSE;
      }
    }
    fileList.push_back(fInputName);
  }

  for ( TString filename : fileList ) {
    TString currRun = GetRunNumber(filename);
    if ( ! currRun.IsNull() ) fRunList.push_back(currRun.Atoi());
  }

  TString checkString = fileList[0];

  fGridDataDir = GetGridDataDir(checkString);
  fGridDataPattern = GetGridDataPattern(checkString);

  if ( sOpt.Contains("ESD") ) fFileType = kESD;
  else if ( sOpt.Contains("AOD") ) fFileType = kAOD;
  else if ( checkString.Contains("AliESDs") ) fFileType = kESD;
  else if ( checkString.Contains("AliAOD") ) fFileType = kAOD;
  else {
    printf("Error: cannot determine if it is ESD or AOD\n");
    return kFALSE;
  }

  fPeriod = GetPeriod(sOpt);
  if ( fPeriod.IsNull() ) fPeriod = GetPeriod(checkString);
  if ( fPeriod.IsNull() ) printf("Warning: cannot determine period\n");

  fPass = GetPass(sOpt);
  if ( fPass.IsNull() ) fPass = GetPeriod(checkString);
  if ( fPass.IsNull() ) printf("Warning: cannot determine pass\n");

  fIsMC = sOpt.Contains("MC");
  fIsEmbed = sOpt.Contains("EMBED");

  return kTRUE;
}

//_______________________________________________________
void AliTaskSubmitter::SetLibraries ( const char* libraries, const char* includePaths )
{
  /// Set libraries and include paths
  fIncludePaths.clear();
  std::string incs(includePaths);
  std::istringstream sincs(incs);
  for ( std::string str; sincs >> str; ) fIncludePaths.push_back(str);

  fLibraries.clear();
  std::string libs(libraries);
  std::istringstream slibs(libs);
  for ( std::string str; slibs >> str; ) fLibraries.push_back(str);
}

//_______________________________________________________
Bool_t AliTaskSubmitter::SetMode ( const char* runMode, const char* analysisMode )
{
  /// Set run mode and analysis mode
  TString rMode(runMode), anMode(analysisMode);
  rMode.ToLower();
  anMode.ToLower();

  TString runModeNames[] = {"test","full","merge","terminate"};
  Int_t nRunModes = sizeof(runModeNames)/sizeof(runModeNames[0]);

  fRunMode = -1;
  fAnalysisMode = -1;

  for ( Int_t imode=0; imode<nRunModes; imode++ ) {
    if ( rMode == runModeNames[imode] ) {
      fRunMode = imode;
      break;
    }
  }

  TString analysisModeNames[3] = {"local","grid","proof"};

  if ( anMode == analysisModeNames[kLocalAnalysis] ) fAnalysisMode = kLocalAnalysis;
  else if ( anMode == analysisModeNames[kGridAnalysis] ) fAnalysisMode = kGridAnalysis;
  else if ( anMode == "terminate" ) {
    fAnalysisMode = kLocalAnalysis;
    fRunMode = kTerminateMode;
  }
  else {
    SetupProof(analysisMode);
  }

  if ( fRunMode < 0 ) {
    printf("Error: cannot recognize runMode: %s\n",runMode);
    printf("Possible values: test full merge terminate\n");
    return kFALSE;
  }

  if ( fAnalysisMode < 0 ) {
    printf("Error: cannor recognize analysisMode: %s\n",analysisMode);
    printf("Possible values: local grid terminate vaf saf saf2\n");
    return kFALSE;
  }

  fRunModeName = runModeNames[fRunMode];
  fAnalysisModeName = analysisModeNames[fAnalysisMode];

  return kTRUE;
}

//_______________________________________________________
void AliTaskSubmitter::SetSearchPaths ( const char* searchPath )
{
  /// Set space-separated path list where it searches for additional source files
  fSearchPaths.clear();
  std::string sPath(searchPath);
  std::istringstream ssPath(sPath);
  for ( std::string str; ssPath >> str; ) {
    TString absPath = GetAbsolutePath(str.c_str());
    fSearchPaths.push_back(absPath.Data());
  }
}

//_______________________________________________________
void AliTaskSubmitter::SetSoftVersion ( TString softVersion )
{
  /// Set software version for proof or grid

  if ( softVersion.IsNull() ) {
    TDatime dt;
    Int_t day = dt.GetDay()-1;
    Int_t month = dt.GetMonth();
    Int_t year = dt.GetYear();
    if ( day == 0 ) {
      day = 28;
      month--;
      if ( month == 0 ) {
        month = 12;
        year--;
      }
    }
    softVersion = Form("vAN-%i%02i%02i-1",year,month,day);
  }
//  if ( ! softVersion.BeginsWith("VO_ALICE") ) softVersion.Prepend("VO_ALICE@AliPhysics::");
  fSoftVersion = softVersion;
}

//_______________________________________________________
void AliTaskSubmitter::SetTaskDir ()
{
  /// Set task dir
  TString foundLib = gSystem->GetLibraries(ClassName(),"",kFALSE);
  fTaskDir = gSystem->DirName(foundLib.Data());
  //  fTaskDir = taskDir;
  //  if ( fTaskDir.IsNull() ) fTaskDir = gSystem->Getenv("TASKDIR");
  //  if ( gSystem->AccessPathName(Form("%s/%s.cxx",fTaskDir.Data(),ClassName())) ) {
  //    printf("Error: cannot find %s in %s",ClassName(),fTaskDir.Data());
  //    fTaskDir = "";
  //    return kFALSE;
  //  }
  //  return kTRUE;
}

//_______________________________________________________
Bool_t AliTaskSubmitter::SetupAnalysis ( const char* runMode, const char* analysisMode,
                                        const char* inputName, const char* inputOptions,
                                        const char* softVersion,
                                        const char* analysisOptions,
                                        const char* libraries, const char* includePaths,
                                        const char* workDir,
                                        Bool_t isMuonAnalysis )
{
  /// Setup analysis
  if ( ! SetMode(runMode,analysisMode) ) return kFALSE;
  if ( ! SetInput(inputName,inputOptions) ) return kFALSE;
  SetSoftVersion(softVersion);
  SetLibraries(libraries,includePaths);
  if ( ! isMuonAnalysis ) fLoadAllBranches = kTRUE;
  TString anOptions(analysisOptions);
  anOptions.ToUpper();
  SetMixingEvent(anOptions.Contains("MIXED"));
  Init(workDir);
  if ( ! anOptions.Contains("NOPHYSSEL") ) AddPhysicsSelection();
  if ( anOptions.Contains("CENTR") ) AddCentrality(anOptions.Contains("OLDCENTR"));
  return kTRUE;
}

//_______________________________________________________
Bool_t AliTaskSubmitter::SetupLocalWorkDir ( TString workDir )
{
  /// Setup local working directory

  fCurrentDir = gSystem->pwd();

  SetTaskDir();
  std::string sTaskDir(fTaskDir.Data());
  fSearchPaths.push_back(sTaskDir);
  std::string sCurrDir(fCurrentDir.Data());
  fSearchPaths.push_back(sCurrDir);

  TString classFilename = ClassName();
  classFilename.Append(".cxx");
  if ( gSystem->AccessPathName(classFilename.Data()) == 0 ) {
    if ( ! IsTerminateOnly() ) {
      printf("Found %s in the current working directory\n",classFilename.Data());
      printf("Assume you want to re-run local: do not copy files\n");
    }
    return kTRUE;
  }

  if ( workDir.IsNull() ) {
    workDir = "tmpDir";
    printf("No workdir specified: creating default %s\n",workDir.Data());
  }

  Bool_t yesToAll = kFALSE;
  TString command = "";

  Bool_t makeDir = ( gSystem->AccessPathName(workDir) != 0 );
  if ( IsTerminateOnly() ) {
    if ( makeDir ) {
      printf("\nError: mode %s requires an existing workDir containing the analysis results!\n\n",fRunModeName.Data());
      return kFALSE;
    }
  }
  else if ( ! makeDir ) {
    TString workDirFull = GetAbsolutePath(workDir);
    if ( workDirFull == fCurrentDir.Data() ) return kFALSE;
    printf("Workdir %s already exist:\n",workDir.Data());
    command = Form("rm -rf %s",workDir.Data());
    makeDir = PerformAction(command,yesToAll);
  }

  if ( makeDir ) {
    yesToAll = kTRUE;
    command = Form("mkdir %s",workDir.Data());
    PerformAction(command,yesToAll);
  }

  fWorkDir = GetAbsolutePath(workDir);
  if ( fWorkDir == fCurrentDir.Data() || IsTerminateOnly() ) return kTRUE;

  for ( std::string str : fLibraries ) {
    TString currFile = str.c_str();
    if ( currFile.EndsWith(".so") ) continue;
    else {
      Bool_t isOk = CopyFile(currFile.Data());
      if ( ! isOk && currFile.EndsWith(".par") ) {
        TString command = Form("cd %s; make %s; cd %s; find %s -name %s -exec mv -v {} ./ \\;", fAliPhysicsBuildDir.Data(), currFile.Data(), fWorkDir.Data(), fAliPhysicsBuildDir.Data(), currFile.Data());
        if ( PerformAction(command, yesToAll) ) {
          isOk = kTRUE;
          // Fixes problem with OADB on proof:
          // the par file only contians the srcs
          // but if you want to access OADB object they must be inside there!
          if ( currFile.Contains("OADB") ) {
            command = "tar -xzf OADB.par; rsync -avu --exclude=.svn --exclude=PROOF-INF.OADB $ALICE_PHYSICS/OADB/ OADB/; tar -czf OADB.par OADB";
            PerformAction(command, yesToAll);
          }
        }
        else {
          printf("Error: could not create %s\n", currFile.Data());
          isOk = kFALSE;
        }
        gSystem->Exec(Form("cd %s",fCurrentDir.Data()));
      }
      else if ( currFile.EndsWith(".cxx") ) {
        currFile.ReplaceAll(".cxx",".h");
        CopyFile(currFile.Data());
      }
      if ( ! isOk ) break;
    }
  }

  CopyDatasetLocally();

  TString runMacro = GetRunMacro();
  runMacro.Remove(runMacro.Index("("));
  runMacro.ReplaceAll("+","");
  CopyFile(runMacro.Data());

  CopyFile(classFilename.Data());
  classFilename.ReplaceAll(".cxx",".h");
  CopyFile(classFilename.Data());

  for ( std::string str : fUtilityMacros ) {
    CopyFile(Form("%s.C",str.c_str()));
  }

  if ( fIsPod ) WritePodExecutable();

  return kTRUE;
}

//_______________________________________________________
Bool_t AliTaskSubmitter::SetupProof ( const char* analysisMode )
{
  /// Setup proof info

  TString anMode(analysisMode);
  anMode.ToLower();

  fPodOutDir = "taskDir";
  TString runPodCommand = Form("\"%s/runPod.sh nworkers\"",fPodOutDir.Data());

  TString userName = gSystem->Getenv("alice_API_USER");
  userName.Append("@");
  fAAF = -1;
  fIsPod = kFALSE;
  if ( anMode == "saf2" ) {
    fAAF = kSAF2;
    fProofCluster = "nansafmaster2.in2p3.fr";
    fProofCluster.Prepend(userName.Data());
    fProofServer = fProofCluster;
  }
  else if ( anMode == "saf" ) {
    fAAF = kSAF;
    fIsPod = kTRUE;
    fProofCluster = "pod://";
    fProofServer = "nansafmaster3.in2p3.fr";
    fProofCopyCommand = "rsync -avcL -e 'gsissh -p 1975'";
    fProofOpenCommand = Form("gsissh -p 1975 -t %s",fProofServer.Data());
    fProofExecCommand = Form("/opt/SAF3/bin/saf3-enter \"\" %s",runPodCommand.Data());
    fProofDatasetMode = "cache";
  }
  else if ( anMode == "vaf" ) {
    fAAF = kCernVaf;
    fIsPod = kTRUE;
    Int_t lxplusTunnelPort = 5501;
    fProofCluster = "pod://";
    fProofServer = "localhost";
    fProofCopyCommand = Form("rsync -avcL -e 'ssh -p %i'",lxplusTunnelPort);
    fProofOpenCommand = Form("ssh %slocalhost -p %i -t",userName.Data(),lxplusTunnelPort);
    fProofExecCommand = Form("echo %s | /usr/bin/vaf-enter",runPodCommand.Data());
    fProofDatasetMode = "remote";
  }
  else if ( anMode == "prooflite" ) {
    fProofCluster = "";
    fProofServer = "localhost";
  }
  else {
    return kFALSE;
  }

  fAnalysisMode = kAAFAnalysis;

  TString hostname = gSystem->GetFromPipe("hostname");
  fIsPodMachine = ( fProofServer == hostname || hostname.BeginsWith("alivaf") );

  return kTRUE;
}


//______________________________________________________________________________
void AliTaskSubmitter::StartAnalysis () const
{
  /// Run the analysis

  UnloadMacros();

  if ( ! fIsInitOk ) return;

  Bool_t terminateOnly = IsTerminateOnly();
  if ( fIsPod && ! fIsPodMachine ) {
    if ( ! ConnectToPod() ) return;
    if ( ! CopyPodOutput() ) return;
    terminateOnly = kTRUE;
  }

  AliAnalysisManager* mgr = AliAnalysisManager::GetAnalysisManager();
  if ( ! mgr->InitAnalysis()) {
    printf("Fatal: Cannot initialize analysis\n");
    return;
  }
  mgr->PrintStatus();

  if ( terminateOnly && gSystem->AccessPathName(mgr->GetCommonFileName())) {
    printf("Cannot find %s : noting done\n",mgr->GetCommonFileName());
    return;
  }

  TObject* inputObj = CreateInputObject();

  TString mgrMode = terminateOnly ? "grid terminate" : fAnalysisModeName.Data();

  if ( ! inputObj || inputObj->IsA() == TChain::Class() ) mgr->StartAnalysis(mgrMode.Data(), static_cast<TChain*>(inputObj)); // Local or grid
  else if ( inputObj->IsA() == TFileCollection::Class() ) mgr->StartAnalysis(mgrMode.Data(), static_cast<TFileCollection*>(inputObj)); // proof
  else mgr->StartAnalysis(mgrMode.Data(), inputObj->GetName()); // proof

  gSystem->cd(fCurrentDir.Data());

  //mgr->ProfileTask("SingleMuonAnalysisTask"); // REMEMBER TO COMMENT (test memory)
}

//______________________________________________________________________________
void AliTaskSubmitter::WritePodExecutable () const
{
  /// Write PoD executable
  TString filename = Form("%s/runPod.sh",fWorkDir.Data());
  ofstream outFile(filename.Data());
  outFile << "#!/bin/bash" << endl;
  outFile << "nWorkers=${1-88}" << endl;
  outFile << "vafctl start" << endl;
  outFile << "vafreq $nWorkers" << endl;
  outFile << "vafwait $nWorkers" << endl;
  outFile << "export TASKDIR=\"$HOME/" << fPodOutDir.Data() << "\"" << endl;
  outFile << "cd $TASKDIR" << endl;
  if ( fProofSplitPerRun ) {
    outFile << "fileList=$(find . -maxdepth 1 -type f ! -name " << fDatasetName.Data() << " | xargs)" << endl;
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
    outFile << "  echo \"$line\" > " << fDatasetName.Data() << endl;
  }
  TString rootCmd = GetRunMacro();
  //  rootCmd.Prepend("root -b -q '");
  //  rootCmd.Append("'");
  //  outFile << rootCmd.Data() << endl;
  outFile << "root -b << EOF" << endl;
  for ( std::string str : fLibraries ) {
    if ( str.rfind(".par") != std::string::npos )
      outFile << "AliAnalysisAlien::SetupPar(\"" << str.c_str() << "\");" << endl;
  }
  outFile << ".x " << rootCmd.Data() << endl;
  outFile << ".q" << endl;
  outFile << "EOF" << endl;
  if ( fProofSplitPerRun ) {
    outFile << "cd $TASKDIR" << endl;
    outFile << "done < " << fDatasetName.Data() << endl;
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


//_______________________________________________________
void AliTaskSubmitter::UnloadMacros() const
{
  /// Unload macros
  for ( std::string str : fUtilityMacros ) {
    TString foundLib = gSystem->GetLibraries(str.c_str(),"",kFALSE);
    if ( foundLib.IsNull() ) continue;
    gSystem->Unload(foundLib.Data());
  }
}

//
////_______________________________________________________
//void AliTaskSubmitter::PrintOptions() const
//{
//  /// Print recognised options
//  printf("\nList of recognised options:\n");
//  printf("  runMode: test full merge terminate\n");
//  printf("  analysisMode: local grid saf saf2 vaf terminateonly\n");
//  printf("  inputName: <runNumber> <fileWithRunList> <rootFileToAnalyse(absolute path)>\n");
//  printf("  inputOptions: Data/MC FULL/EMBED AOD/ESD <period> <pass> <dataPattern> <dataDir>\n");
//  printf("  softVersions: aliphysics=version,aliroot=version,root=version\n");
//  printf("  analysisOptions: NOPHYSSEL CENTR OLDCENTR MIXED SPLIT\n");
//}
//
//
//
//

////_______________________________________________________
//Bool_t LoadAddTasks ( TString libraries )
//{
//  TObjArray* libList = libraries.Tokenize(" ");
//  for ( Int_t ilib=0; ilib<libList->GetEntries(); ilib++) {
//    TString currName = gSystem->BaseName(libList->At(ilib)->GetName());
//    if ( currName.EndsWith(".C") ) gROOT->LoadMacro(currName.Data());
//  }
//  delete libList;
//  return kTRUE;
//}
//
//
//
//______________________________________________________________________________
void AliTaskSubmitter::WriteTemplateRunTask ( const char* outputDir ) const
{
  /// Write a template macro
  TString macroName = Form("%s/runTask.C",outputDir);
  ofstream outFile(macroName.Data());
  outFile << "void runTask ( const char* runMode, const char* analysisMode," << endl;
  outFile << "  const char* inputName," << endl;
  outFile << "  const char* inputOptions = \"\"," << endl;
  outFile << "  const char* softVersion = \"\"," << endl;
  outFile << "  const char* analysisOptions = \"\"," << endl;
  outFile << "  const char* taskOptions = \"\" )" << endl;
  outFile << "{" << endl;
  outFile << endl;
  outFile << "  gSystem->AddIncludePath(\"-I$ALICE_ROOT/include -I$ALICE_PHYSICS/include\");" << endl;
  outFile << "  gROOT->LoadMacro(gSystem->ExpandPathName(\"$TASKDIR/" << ClassName() << ".cxx+\"));" << endl;
  outFile << "  AliTaskSubmitter sub;" << endl;
  outFile << endl;
  outFile << "  if ( !  sub.SetupAnalysis(runMode,analysisMode,inputName,inputOptions,softVersion,analysisOptions, \"yourLibs\",\"$ALICE_ROOT/include $ALICE_PHYSICS/include\",\"\") ) return;" << endl;
  outFile << endl;
  outFile << "  AliAnalysisAlien* plugin = (AliAnalysisAlien*)AliAnalysisManager::GetAnalysisManager()->GetGridHandler();" << endl;
  outFile << "  if ( plugin ) plugin->SetGridWorkingDir(\"workDirRelativeToHome\");" << endl;
  outFile << endl;
  outFile << "//  Bool_t isMC = sub.IsMC();" << endl;
  outFile << endl;
  outFile << "  gROOT->LoadMacro(\"yourAddTask.C\");" << endl;
  outFile << "  AliAnalysisTask* task = yourAddTask();" << endl;
  outFile << endl;
  outFile << "//  AliMuonEventCuts* eventCuts = BuildMuonEventCuts(sub.GetMap());" << endl;
  outFile << endl;
  outFile << "//  SetupMuonBasedTask(task,eventCuts,taskOptions,sub.GetMap());" << endl;
  outFile << endl;
  outFile << "  sub.StartAnalysis();" << endl;
  outFile << "}" << endl;
  outFile.close();
}
