#include "AliTaskSubmitter.h"

#include <sstream>
#include <algorithm>

#include <Riostream.h>

// ROOT includes
#include "TSystem.h"
#include "TString.h"
#include "TFile.h"
#include "TMacro.h"
#include "TList.h"
#include "TPRegexp.h"
#include "TRegexp.h"
#include "TDatime.h"
#include "TChain.h"
#include "TFileCollection.h"
#include "TObjString.h"
#include "TInterpreter.h"
// #include "TROOT.h"
// #include "TEnv.h"
// #include "TStopwatch.h"
// #include "TProof.h"
// #include "TGrid.h"
// #include "TApplication.h"
// #include "TInterpreter.h"
//
// // STEER includes
#include "AliESDInputHandler.h"
#include "AliAODInputHandler.h"
#include "AliAODHandler.h"
#include "AliMCEventHandler.h"
#include "AliMultiInputEventHandler.h"
// //#include "AliLog.h"

// ANALYSIS includes
#include "AliAnalysisManager.h"
#include "AliAnalysisTaskSE.h"
#include "AliAnalysisAlien.h"
#include "AliAnalysisTaskCfg.h"

// #include "AliPhysicsSelection.h"
// #include "AliPhysicsSelectionTask.h"
// #include "AliCentralitySelectionTask.h"
// #include "AliMultSelectionTask.h"

//_______________________________________________________
AliTaskSubmitter::AliTaskSubmitter() :
fHasCentralityInfo(false),
fHasPhysSelInfo(false),
fIsEmbed(false),
fIsInputFileCollection(false),
fIsMC(false),
fFileType(kAOD),
fRunMode(kLocal),
fGridDataDir(),
fGridDataPattern(),
fPass(),
fPeriod(),
fPodOutDir(),
fProofCluster(),
fProofDatasetMode(),
fProofServer(),
fProofCopyCommand(),
fProofOpenCommand(),
fProofExecCommand(),
fSoftVersion(),
fSubmitterDir(),
fTaskOptions(),
fWorkDir(),
fAdditionalFiles(),
fLibraries(),
fMacros(),
fPackages(),
fSources(),
fUtilityMacros(),
fTasks(),
fKeywords(),
fPlugin()
// fInputObject(nullptr)
{
  /// Ctr
  fSubmitterDir = gSystem->DirName(gSystem->GetLibraries("AliTaskSubmitter","",kFALSE));

  fUtilityMacros.push_back("BuildMuonEventCuts.C");
  fUtilityMacros.push_back("SetAlienIO.C");
  fUtilityMacros.push_back("SetupMuonBasedTasks.C");
}

//_______________________________________________________
AliTaskSubmitter::~AliTaskSubmitter()
{
  /// Dtor
  // UnloadMacros();
  // delete fInputObject;
}

//_______________________________________________________
void AliTaskSubmitter::AddObjects ( const char* objnameList, std::vector<std::string>& objlist )
{
  /// Add object avoiding duplication
  TString sObjnameList(objnameList);
  sObjnameList.ReplaceAll(" ","");
  TObjArray* arr = sObjnameList.Tokenize(",");
  TIter next(arr);
  TObject* obj = nullptr;
  while ( (obj=next()) ) {
    if ( std::find(objlist.begin(),objlist.end(),obj->GetName()) == objlist.end() ) objlist.push_back(obj->GetName());
  }
  delete arr;
}

//_______________________________________________________
bool AliTaskSubmitter::AddTask ( const char* configFilename )
{
  /// Parse configuration file
  std::string cfgFilename = gSystem->ExpandPathName(configFilename);
  if ( gSystem->AccessPathName(cfgFilename.c_str()) ) {
    std::cout << "Fatal: cannot open " << configFilename << std::endl;
    return false;
  }
  TObjArray* arr = AliAnalysisTaskCfg::ExtractModulesFrom(cfgFilename.c_str());
  TIter next(arr);
  AliAnalysisTaskCfg* cfg = nullptr;
  while ( (cfg = static_cast<AliAnalysisTaskCfg*>(next())) ) {
    if ( ! cfg->GetMacro() ) cfg->OpenMacro();
    fTasks.push_back(*cfg);
    for ( Int_t ilib=0; ilib<cfg->GetNlibs(); ++ilib ) {
      AddObjects(cfg->GetLibrary(ilib),fLibraries);
    }
    AddObjects(cfg->GetMacroName(),fMacros);
  }

  std::ifstream inFile (cfgFilename );
  TString line;
  while ( line.ReadLine(inFile) ) {
    if ( line.BeginsWith("#Module.Par") ) {
      fPackages.push_back(AliAnalysisTaskCfg::DecodeValue(line));
    }
    else if ( line.BeginsWith("#Module.AdditionalFiles") ) {
      fAdditionalFiles.push_back(AliAnalysisTaskCfg::DecodeValue(line));
    }
    else if ( line.BeginsWith("#Module.Sources") ) {
      fSources.push_back(AliAnalysisTaskCfg::DecodeValue(line));
    }
  }
  return true;
}

//_______________________________________________________
bool AliTaskSubmitter::CopyFile ( const char* inFilename, const char* outFilename ) const
{
  /// Copy file
  std::string expfname = gSystem->ExpandPathName(inFilename);
  if ( gSystem->AccessPathName(expfname.c_str()) ) {
    std::cout << "Error: cannot find " << inFilename << std::endl;
    return false;
  }

  std::string sOutFilename = fWorkDir;
  sOutFilename += "/";
  sOutFilename += ( outFilename ) ? outFilename : gSystem->BaseName(expfname.c_str());

  TFile::Cp(expfname.c_str(), sOutFilename.c_str(),false);

  return true;
}

//_______________________________________________________
void AliTaskSubmitter::CreateAlienHandler ()
{
  /// Create the alien plugin
  fPlugin = new AliAnalysisAlien();

  fPlugin->SetAPIVersion("V1.1x");
  fPlugin->SetAliPhysicsVersion(fSoftVersion.c_str());
  fPlugin->SetOutputToRunNo();
  fPlugin->SetNumberOfReplicas(2);
  fPlugin->SetDropToShell(kFALSE);

  fPlugin->SetCheckCopy(kFALSE); // Fixes issue with alien_CLOSE_SE

  switch ( fRunMode ) {
    case kGridTest:
    case kProofLite:
    fPlugin->SetRunMode("test");
    break;
    case kGridMerge:
    case kGridTerminate:
    fPlugin->SetRunMode("merge");
    break;
    default:
    fPlugin->SetRunMode("full");
  }

  if ( fRunMode != kGridTerminate ) fPlugin->SetMergeViaJDL();

  // Set run list
  if ( ! fIsMC ) fPlugin->SetRunPrefix("000");
  int nRuns = 0;
  for ( auto& filename : fInputData ) {
    std::string currRun = GetRunNumber(filename.c_str());
    if ( currRun.empty() ) continue;
    fPlugin->AddRunNumber(std::atoi(currRun.c_str()));
    ++nRuns;
  }

  if ( nRuns == 0 ) {
    std::cout << std::endl;
    std::cout << "ERROR: the alien plugin expects a run list. This could not be found in the input" << std::endl;
    std::cout << "This might be a custom production...but the plugin will not be able to handle it." << std::endl << std::endl;
  }

  // Set grid work dir (tentatively)
  std::string gridWorkDir = "analysis";
  if ( fIsMC ) gridWorkDir = "mcAna";

  if ( IsGrid() ) {
    std::string currDirName = gSystem->pwd();
    currDirName.erase(0,currDirName.find_last_of("/")+1);
    if ( ! fPeriod.empty() ) {
      std::stringstream ss;
      ss << "/" << fPeriod << "/" << currDirName;
      gridWorkDir += ss.str();
      std::cout << "WARNING: setting a custom grid working dir:" << gridWorkDir << std::endl;
      fPlugin->SetGridWorkingDir(gridWorkDir.c_str());
    }
    else {
      std::cout << std::endl;
      std::cout << "WARNING: GridWorkDir is not set. You have to do it in your macro:" << std::endl;
      std::cout << "AliAnalysisAlien* plugin = static_cast<AliAnalysisAlien*>(AliAnalysisManager::GetAnalysisManager()->GetGridHandler());" << std::endl;
      std::cout << "if ( plugin ) plugin->SetGridWorkingDir(\"workDirRelativeToHome\");" << std::endl << std::endl;
    }

    fPlugin->SetGridDataDir(fGridDataDir.c_str());
    fPlugin->SetDataPattern(fGridDataPattern.c_str());
  }

  // Set libraries
  fPlugin->SetAdditionalRootLibs("libGui.so libProofPlayer.so libXMLParser.so");

  std::stringstream extraLibs;
  for ( auto& str : fLibraries ) extraLibs << str << " ";
  fPlugin->SetAdditionalLibs(extraLibs.str().c_str());

  std::stringstream extraSrcs;
  for ( auto& str : fSources ) {
    std::string from = ".cxx";
    std::string header = str;
    header.replace(str.find(from),from.length(),".h");
    extraSrcs << header << " " << str << " ";
  }
  fPlugin->SetAnalysisSource(extraSrcs.str().c_str());

  for ( auto& str : fPackages ) fPlugin->EnablePackage(str.c_str());

  // if ( fRunMode == kLocal || fRunMode == kProofLite )
  fPlugin->SetFileForTestMode("dataset.txt");
  fPlugin->SetProofCluster(fProofCluster.c_str());

  // for ( std::string str : fIncludePaths ) plugin->AddIncludePath(Form("-I%s",str.c_str()));

  AliAnalysisManager::GetAnalysisManager()->SetGridHandler(fPlugin);
}

//_______________________________________________________
std::string AliTaskSubmitter::GetAbsolutePath ( const char* path ) const
{
  /// Get absolute path
  std::string currDir = gSystem->pwd();
  if ( gSystem->AccessPathName(path) ) {
    printf("Error: cannot access %s\n",path);
    return "";
  }
  return gSystem->GetFromPipe(Form("cd %s; pwd; cd %s",path,currDir.c_str())).Data()
  ;
}

//______________________________________________________________________________
std::string AliTaskSubmitter::GetGridDataDir ( const char* queryString ) const
{
  /// Get data dir for grid
  std::string basePath = GetGridQueryVal(queryString,"BasePath");
  if ( ! basePath.empty() ) {
    std:string runNum = GetRunNumber(queryString);
    if ( ! runNum.empty() ) basePath.erase(basePath.find(runNum));
  }
  return basePath;
}

//______________________________________________________________________________
std::string AliTaskSubmitter::GetGridDataPattern ( const char* queryString ) const
{
  /// Get data pattern for grid
  std::string basePath = GetGridQueryVal(queryString,"BasePath");
  std::string fileName = GetGridQueryVal(queryString,"FileName");
  if ( basePath.empty() || fileName.empty() ) return "";
  fileName.insert(0,"*");
  std::string runNum = GetRunNumber(queryString);
  if ( ! runNum.empty() ) {
    basePath.erase(basePath.begin(),basePath.begin()+basePath.find(runNum) + runNum.length()+1);
    if ( ! basePath.empty() ) {
      if ( basePath.back() != '/' ) basePath.append("/");
      fileName.insert(0,basePath.c_str());
    }
  }
  return fileName;
}


//______________________________________________________________________________
std::string AliTaskSubmitter::GetGridQueryVal ( const char* queryString, const char* keyword ) const
{
  std::string found = "";
  std::stringstream qs(queryString);
  std::string str;
  while ( std::getline(qs,str,';') ) {
    if ( str.find(keyword) != std::string::npos ) {
      size_t idx = str.find("=");
      if ( idx != std::string::npos ) {
        str.erase(0,idx+1);
        found = str;
        break;
      }
    }
  }

  if ( found.empty() ) std::cout << "Warning: cannot find " << keyword << " in " << queryString << std::endl;
  return found;
}

//______________________________________________________________________________
TMap* AliTaskSubmitter::GetMap ()
{
  /// Get map with values to be passed to macros
  /// This is kept for backward compatibility
  if ( fMap.IsEmpty() ) {
    fMap.SetOwner();
    fMap.Add(new TObjString("period"),new TObjString(fPeriod.c_str()));
    TString dataType = fIsMC ? "MC" : "DATA";
    fMap.Add(new TObjString("dataType"),new TObjString(dataType));
    TString dataTypeDetails = fIsEmbed ? "EMBED" : "FULL";
    fMap.Add(new TObjString("mcDetails"),new TObjString(dataTypeDetails));
    TString physSel = fHasPhysSelInfo ? "YES" : "NO";
    fMap.Add(new TObjString("physicsSelection"),new TObjString(physSel));
    TString centr = fHasCentralityInfo ? "YES" : "NO";
    AliAnalysisManager* mgr = AliAnalysisManager::GetAnalysisManager();
    fMap.Add(new TObjString("centrality"),new TObjString(centr));
  }

  return &fMap;
}

//______________________________________________________________________________
std::string AliTaskSubmitter::GetPass ( const char* checkString ) const
{
  /// Get pass name in string
  TPRegexp re("(^|/| )(pass|muon_calo).*?(/|;| |$)");
  TString cs(checkString);
  cs.ReplaceAll("*",""); // Add protection for datasets with a star inside
  TString found = cs(re);
  if ( ! found.IsNull() ) found = found(TRegexp("[a-zA-Z_0-9]+"));
  std::string out = found.Data();
  return out;
}

//______________________________________________________________________________
std::string AliTaskSubmitter::GetPeriod ( const char* checkString ) const
{
  /// Get period in string
  TString cs(checkString);
  TString substring = cs(TRegexp("LHC[0-9][0-9][a-z]"));
  return substring.Data();
}

//______________________________________________________________________________
std::string AliTaskSubmitter::GetRunNumber ( const char* checkString ) const
{
  /// Get run number in string
  TPRegexp re("(^|/)[0-9][0-9][0-9][0-9][0-9][0-9]+(/|;|$)");
  TString cs(checkString);
  cs.ReplaceAll("*",""); // Add protection for datasets with a star inside
  TString found = cs(re);
  std::string out;
  if ( ! found.IsNull() ) {
    found = found(TRegexp("[0-9]+"));
    if ( found.Length() == 6 || (found.Length() == 9 && found.BeginsWith("0")) ) out = found.Data();
  }
  return out;
}

//_______________________________________________________
bool AliTaskSubmitter::Load() const
{
  /// Load everything needed
  for ( auto& str : fSources ) gInterpreter->ProcessLine(Form(".L %s+",str.c_str()));

  for ( auto& str : fUtilityMacros ) gInterpreter->ProcessLine(Form(".L %s+",str.c_str()));

  return true;
}

//_______________________________________________________
int AliTaskSubmitter::ReplaceKeywords ( std::string& input ) const
{
  /// Replace kewyord
  if ( input.find("__VAR") == std::string::npos ) return 0;

  for ( auto& key : fKeywords ) {
    input.replace(input.find(key.first),key.first.length(),key.second);
  }

  if ( input.find("__VAR_") != std::string::npos ) {
    std::cout << "Error: cannot replace variable in " << input << std::endl;
    return -1;
  }

  return 0;
}

//_______________________________________________________
int AliTaskSubmitter::ReplaceKeywords ( TObjString* input ) const
{
  /// Replace kewyord
  std::string str = input->GetName();
  int outCode = ReplaceKeywords (str);
  if ( outCode == 1 ) {
    input->SetString(str.c_str());
  }
  return outCode;
}

//_______________________________________________________
bool AliTaskSubmitter::Run ( int runMode, const char* inputName, const char* inputOptions, const char* analysisOptions, const char* taskOptions, const char* softVersions, bool isMuonAnalysis )
{
  /// Run analysis
  /// The command should be issued inside the working directory

  if ( gSystem->AccessPathName("AliTaskSubmitter.cxx") ) {
    std::cout << "The run command should be issued inside a working directory" << std::endl;
    std::cout << "If you need to build it, please use SetupAndRun instead" << std::endl;
    return false;
  }

  fRunMode = runMode;
  fWorkDir = ".";
  fTaskOptions = taskOptions;
  SetSoftVersion ( softVersions );
  // Parse inputs
  if ( ! SetInput(inputName,inputOptions) ) return false;
  SetupProof();

  AliAnalysisManager *mgr = new AliAnalysisManager("testAnalysis");
  CreateAlienHandler();

  SetupHandlers(analysisOptions, isMuonAnalysis);

  // Setup train
  std::string anOpts(analysisOptions);
  std::transform(anOpts.begin(), anOpts.end(), anOpts.begin(), ::tolower);

  if ( anOpts.find("NOPHYSSEL") == std::string::npos ) {
    fHasPhysSelInfo = true;
    if ( fFileType != kAOD ) AddTask(Form("%s/physSelTask.cfg",fSubmitterDir.c_str()));
  }
  if ( anOpts.find("CENTR") != std::string::npos ) {
    fHasCentralityInfo = true;
    // AddCentrality(anOptions.Contains("OLDCENTR"));
  }
  AddTask("train.cfg");

  SetupTasks();

  std::cout << "Analyzing " << (( fFileType == kAOD ) ? "AODs" : "ESDs") << "  MC " << fIsMC << std::endl;

  StartAnalysis();

  return true;
}

//_______________________________________________________
void AliTaskSubmitter::SetKeywords ()
{
  /// Set keywords
  fKeywords["__VAR_ISEMBED__"] = fIsEmbed ? "true" : "false";
  fKeywords["__VAR_ISAOD__"] = IsAOD() ? "true" : "false";
  fKeywords["__VAR_ISMC__"] = fIsMC ? "true" : "false";
  fKeywords["__VAR_PASS__"] = fPeriod;
  fKeywords["__VAR_PERIOD__"] = fPass;
  fKeywords["__VAR_TASKOPTIONS"] = fTaskOptions;
}

//_______________________________________________________
void AliTaskSubmitter::SetupHandlers ( const char* analysisOptions, bool isMuonAnalysis )
{
  /// Setup data handlers

  AliInputEventHandler* handler = nullptr;
  AliMCEventHandler* mcHandler = nullptr;
  if ( fFileType == kAOD ) handler = new AliAODInputHandler();
  else {
    AliESDInputHandler* esdH = new AliESDInputHandler();
    if ( isMuonAnalysis ) {
      esdH->SetReadFriends(kFALSE);
      esdH->SetInactiveBranches("*");
      esdH->SetActiveBranches("MuonTracks MuonClusters MuonPads AliESDRun. AliESDHeader. AliMultiplicity. AliESDFMD. AliESDVZERO. AliESDTZERO. SPDVertex. PrimaryVertex. AliESDZDC. SPDPileupVertices");
    }
    handler = esdH;

    if ( fIsMC ){
      // Monte Carlo handler
      mcHandler = new AliMCEventHandler();
      std::cout << std::endl << "MC event handler requested" << std::endl << std::endl;
    }
  }

  AliAnalysisManager* mgr = AliAnalysisManager::GetAnalysisManager();

  std::string anOpts(analysisOptions);
  std::transform(anOpts.begin(), anOpts.end(), anOpts.begin(), ::tolower);

  if ( anOpts.find("MIXING") != std::string::npos ) {
    AliMultiInputEventHandler* multiHandler = new AliMultiInputEventHandler();
    mgr->SetInputEventHandler(multiHandler);
    multiHandler->AddInputEventHandler(handler);
    if ( mcHandler ) multiHandler->AddInputEventHandler(mcHandler);
  }
  else {
    mgr->SetInputEventHandler(handler);
    if ( mcHandler ) mgr->SetMCtruthEventHandler(mcHandler);
  }
}

//_______________________________________________________
bool AliTaskSubmitter::SetupLocalWorkDir ( const char* cfgList )
{
  /// Setup local directory
  if ( gSystem->AccessPathName(fWorkDir.c_str()) == 0 ) {
    if ( fRunMode != kLocalTerminate ) {
      std::cout << "Directory " << fWorkDir << " already exists. Overwrite? [y/n]" << std::endl;
      std::string answer;
      std::cin >> answer;
      if ( answer == "y" ) gSystem->Exec(Form("rm -rf %s",fWorkDir.c_str()));
    }
  }

  if ( gSystem->AccessPathName(fWorkDir.c_str()) ) {
    if ( fRunMode == kLocalTerminate ) {
      std::cout << "Directory " << fWorkDir << " must exist in kLocalTerminate mode " << std::endl;
      return false;
    }
    gSystem->mkdir(fWorkDir.c_str());
  }

  std::map<std::string,bool> usedUtilityMacros;
  for ( auto& str : fUtilityMacros ) {
    usedUtilityMacros[str] = false;
  }

  // Copy necessary add tasks and utility macros
  TString sCfgList(cfgList);
  TObjArray* arr = sCfgList.Tokenize(",");
  TIter nextCfgFile(arr);
  TObject* cfgFilename = 0x0;
  std::ofstream outFile(Form("%s/train.cfg",fWorkDir.c_str()));
  while ( (cfgFilename = nextCfgFile()) ) {
    std::ifstream inFile(cfgFilename->GetName());
    TString currLine;
    while ( currLine.ReadLine(inFile) ) {
      outFile <<  currLine.Data() << std::endl;
      // Search for AddTask that are not in ALICE_PHYSICS and copy them locally
      if ( currLine.BeginsWith("#Module.MacroName") ) {
        std::string currMacro = AliAnalysisTaskCfg::DecodeValue(currLine);
        if ( currMacro.find("$ALICE_") == std::string::npos ) {
          if ( ! CopyFile(currMacro.c_str()) ) return false;
        }
      }
      else if ( currLine.BeginsWith("#Module.Sources") || currLine.BeginsWith("#Module.Par") || currLine.BeginsWith("#Module.AdditionalFiles") ) {
        std::vector<std::string> fileList;
        AddObjects(AliAnalysisTaskCfg::DecodeValue(currLine), fileList);
        for ( auto& str : fileList ) {
          if ( ! CopyFile(str.c_str()) ) return false;
          std::string from = ".cxx";
          size_t idx = str.find(from);
          if ( idx != std::string::npos ) {
            str.replace(idx,from.length(),".h");
            if ( ! CopyFile(str.c_str()) ) return false;
          }
        }
      }

      // Check if the task uses some utility macro (and copy it locally)
      for ( auto& str : fUtilityMacros ) {
        std::string macroName = str;
        macroName.erase(macroName.find_last_of("."));
        if ( currLine.Contains(macroName.c_str()) ) {
          usedUtilityMacros[str] = true;
        }
      }
    }
    inFile.close();
  }
  outFile.close();
  delete arr;

  for ( auto& entry : usedUtilityMacros ) {
    if ( entry.second == true) continue;
    fUtilityMacros.erase(std::find(fUtilityMacros.begin(),fUtilityMacros.end(),entry.first));
  }

  if ( ! CopyFile(Form("%s/AliTaskSubmitter.cxx",fSubmitterDir.c_str())) ) return false;
  if ( ! CopyFile(Form("%s/AliTaskSubmitter.h",fSubmitterDir.c_str())) ) return false;
  for ( auto& str : fUtilityMacros ) {
    if ( ! CopyFile(Form("%s/%s",fSubmitterDir.c_str(),str.c_str())) ) return false;
  }

  // AliAnalysisTaskCfg* cfg = nullptr;
  // while ( (cfg = static_cast<AliAnalysisTaskCfg*>(next())) ) {
  //   if ( ! cfg->GetMacro() ) cfg->OpenMacro();
  //   fTasks.push_back(*cfg);
  //   for ( Int_t ilib=0; ilib<cfg->GetNlibs(); ++ilib ) {
  //     AddObject(cfg->GetLibrary(ilib),fLibraries);
  //   }
  //   AddObject(cfg->GetMacroName(),fMacros);
  // }

  // std::ifstream inFile (cfgFilename );
  // TString line;
  // while ( line.ReadLine(inFile) ) {
  //   if ( line.BeginsWith("#Module.Par") ) {
  //     fPackages.push_back(AliAnalysisTaskCfg::DecodeValue(line));
  //   }
  //   else if ( line.BeginsWith("#Module.AdditionalFiles") ) {
  //     fAdditionalFiles.push_back(AliAnalysisTaskCfg::DecodeValue(line));
  //   }
  // }
  // return true;
  // }
  //
  // std::string sCfgList(cfgList);
  // size_t idx = sCfgList.find(",");
  // if ( idx != std::string::npos ) sCfgList.replace(idx,1," ");
  // gSystem->Exec(Form("cat %s > %s/train.cfg",sCfgList.c_str(),fWorkDir.c_str()))
  // ;

  // WriteLoadLibs();
  // WriteAnalysisMacro();
  // WriteRunScript();

  return true;
}

//_______________________________________________________
bool AliTaskSubmitter::SetInput ( const char* inputName, const char* inputOptions )
{
  /// Set input
  std::string inName = gSystem->ExpandPathName(inputName);
  std::string sOpt(inputOptions);

  if ( gSystem->AccessPathName(inName.c_str()) == 0 ) {
    if ( inName.find(".root") != std::string::npos ) {
      fInputData.push_back(inName);
      TFile* file = TFile::Open(inName.c_str());
      fIsInputFileCollection = ( file->GetListOfKeys()->FindObject("dataset") ) ? true : false;
      delete file;
    }
    else {
      std::ifstream inFile(inName.c_str());
      std::string currLine;
      while ( std::getline(inFile,currLine) ) {
        if ( currLine.empty() ) continue;
        fInputData.push_back(currLine);
      }
    }
  }
  else {
    if ( inName.find("Find;") == std::string::npos ) {
      std::cout << "Error: inputName (" << inName << ") must be a local file or a remote file in the form: Find;BasePath=...;FileName=..." << std::endl;
      return false;
    }
    fInputData.push_back(inName);
  }

  std::string checkString = fInputData[0];

  if ( sOpt.find("ESD") != std::string::npos ) fFileType = kESD;
  else if ( sOpt.find("AOD") != std::string::npos ) fFileType = kAOD;
  else if ( checkString.find("AliESDs") != std::string::npos ) fFileType = kESD;
  else if ( checkString.find("AliAOD") != std::string::npos ) fFileType = kAOD;
  else {
    std::cout << "Error: cannot determine if it is ESD or AOD" << std::endl;
    return false;
  }

  fPeriod = GetPeriod(sOpt.c_str());
  if ( fPeriod.empty() ) fPeriod = GetPeriod(checkString.c_str());
  if ( fPeriod.empty() ) printf("Warning: cannot determine period\n");

  fPass = GetPass(sOpt.c_str());
  if ( fPass.empty() ) fPass = GetPeriod(checkString.c_str());
  if ( fPass.empty() ) printf("Warning: cannot determine pass\n");

  fIsMC = ( sOpt.find("MC") != std::string::npos );
  fIsEmbed = ( sOpt.find("EMBED") != std::string::npos );

  if ( IsGrid() ) {
    fGridDataDir = GetGridDataDir(fInputData[0].c_str());
    fGridDataPattern = GetGridDataPattern(fInputData[0].c_str());
  }

  // Write input
  std::string outFilename = fIsInputFileCollection ? "dataset.root" : "dataset.txt";
  if ( gSystem->AccessPathName(inName.c_str()) == 0 ) {
    // Input file exist, let's check it does not coincide with the output
    std::string inputPath = GetAbsolutePath(gSystem->DirName(inName.c_str()));
    if ( inputPath != gSystem->pwd() ) {
      CopyFile(inName.c_str(), outFilename.c_str());
    }
  }
  else {
    std::ofstream outFile("dataset.txt");
    // if ( fIsInputFileCollection ) outFile << "dataset.root" << std::endl;
    // else {
      for ( auto& str : fInputData ) outFile << str << std::endl;
    // }
    outFile.close();
  }

  return true;
}


//_______________________________________________________
// void AliTaskSubmitter::SetLibraries ( const char* libraries )
// {
//   /// Set list of libraries
//   std::istringstream stm(libraries);
// 	std::string token;
// 	while ( std::getline( stm, token, ',' ) ) fLibraries.push_back(token);
// }

// //_______________________________________________________
// void AliTaskSubmitter::SetIncludePaths ( const char* includePaths )
// {
//   /// Set include paths
//   std::istringstream stm(includePaths);
// 	std::string token;
// 	while ( std::getline( stm, token, ',' ) ) fIncludePaths.push_back(token);
// }

// //_______________________________________________________
// void AliTaskSubmitter::SetMacros ( const char* macros )
// {
//   /// Set include paths
//   std::istringstream stm(macros);
// 	std::string token;
// 	while ( std::getline( stm, token, ',' ) ) fMacros.push_back(token);
// }

//_______________________________________________________
void AliTaskSubmitter::SetSoftVersion ( const char* softVersion )
{
  /// Set software version for proof or grid

  std::string sVersion(softVersion);

  if ( sVersion.empty() ) {
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
    sVersion = Form("vAN-%i%02i%02i-1",year,month,day);
  }
//  if ( ! softVersion.BeginsWith("VO_ALICE") ) softVersion.Prepend("VO_ALICE@AliPhysics::");
  fSoftVersion = sVersion;
}

//_______________________________________________________
// void AliTaskSubmitter::SetSources ( const char* sources )
// {
//   /// Set include paths
//   std::istringstream stm(sources);
// 	std::string token;
// 	while ( std::getline( stm, token, ',' ) ) fSources.push_back(token);
// }

//_______________________________________________________
bool AliTaskSubmitter::SetupAndRun ( const char* workDir, const char* cfgList, int runMode, const char* inputName, const char* inputOptions, const char* analysisOptions, const char* taskOptions )
{
  /// Setup analysis working dir
  fWorkDir = workDir;
  fRunMode = runMode;
  if ( ! SetupLocalWorkDir(cfgList) ) return false;

  std::string currDir = gSystem->pwd();
  gSystem->cd(fWorkDir.c_str());
  bool isOk = Run(runMode, inputName, inputOptions, analysisOptions, taskOptions);
  gSystem->cd(currDir.c_str());
  return isOk;
}

//_______________________________________________________
bool AliTaskSubmitter::SetupProof()
{
  /// Setup proof info
  fPodOutDir = "taskDir";
  std::string runPodCommand = Form("\"%s/runPod.sh nworkers\"",fPodOutDir.c_str());

  std::string userName = "";
  if ( gSystem->Getenv("alice_API_USER") ) userName = gSystem->Getenv("alice_API_USER");
  userName.append("@");
  if ( fRunMode == kProofSaf2 ) {
    fProofCluster = "nansafmaster2.in2p3.fr";
    fProofCluster.insert(0,userName.c_str());
    fProofServer = fProofCluster;
  }
  else if ( fRunMode == kProofSaf ) {
    fProofCluster = "pod://";
    fProofServer = "nansafmaster3.in2p3.fr";
    fProofCopyCommand = "rsync -avcL -e 'gsissh -p 1975'";
    fProofOpenCommand = Form("gsissh -p 1975 -t %s",fProofServer.c_str());
    fProofExecCommand = Form("/opt/SAF3/bin/saf3-enter \"\" %s",runPodCommand.c_str());
    fProofDatasetMode = "cache";
  }
  else if ( fRunMode == kProofVaf ) {
    Int_t lxplusTunnelPort = 5501;
    fProofCluster = "pod://";
    fProofServer = "localhost";
    fProofCopyCommand = Form("rsync -avcL -e 'ssh -p %i'",lxplusTunnelPort);
    fProofOpenCommand = Form("ssh %slocalhost -p %i -t",userName.c_str(),lxplusTunnelPort);
    fProofExecCommand = Form("echo %s | /usr/bin/vaf-enter",runPodCommand.c_str());
    fProofDatasetMode = "remote";
    std::cout << "This mode requires that you have an open ssh tunnel to CERN machine over port 5501" << std::endl;
    std::cout << "If you don't, you can set one with:" << std::endl;
    std::cout << "ssh your_user_name@lxplus.cern.ch -L 5501:alivaf-001:22" << std::endl;
  }
  else if ( fRunMode == kProofLite ) {
    fProofCluster = "";
    fProofServer = "localhost";
  }
  else {
    return false;
  }

  // TString hostname = gSystem->GetFromPipe("hostname");
  // fIsPodMachine = ( fProofServer == hostname || hostname.BeginsWith("alivaf") );

  return true;
}

//_______________________________________________________
bool AliTaskSubmitter::SetupTasks ()
{
  /// Setup the tasks

  SetKeywords();

  for ( auto& cfg : fTasks ) {
    std::string macroArgs = cfg.GetMacroArgs();
    int outCode = ReplaceKeywords(macroArgs);
    if ( outCode == -1 ) return false;
    else if ( outCode == 1 ) cfg.SetMacroArgs(macroArgs.c_str());

    TList* lines = cfg.GetConfigMacro()->GetListOfLines();
    TIter next(lines);
    TObjString* objString = nullptr;
    while ( (objString = static_cast<TObjString*>(next())) ) {
      if ( ReplaceKeywords(objString) == -1 ) return false;
    }

    fPlugin->AddModule(&cfg);
  }

  return true;
}

//______________________________________________________________________________
void AliTaskSubmitter::StartAnalysis () const
{
  /// Run the analysis
  // UnloadMacros();

  bool terminateOnly = ( fRunMode == kLocalTerminate );
  // Bool_t terminateOnly = IsTerminateOnly();
  // if ( fIsPod && ! fIsPodMachine ) {
  //   if ( ! ConnectToPod() ) return;
  //   if ( ! CopyPodOutput() ) return;
  //   terminateOnly = kTRUE;
  // }

  Load();
  fPlugin->LoadModules();
  // fPlugin->Print();
  AliAnalysisManager* mgr = AliAnalysisManager::GetAnalysisManager();
  if ( ! mgr->InitAnalysis()) {
    std::cout << "Fatal: Cannot initialize analysis" << std::endl;
    return;
  }
  mgr->PrintStatus();

  if ( terminateOnly && gSystem->AccessPathName(mgr->GetCommonFileName())) {
    std::cout << "Cannot find " << mgr->GetCommonFileName() << " : nothing done" << std::endl;
    return;
  }

  if ( IsGrid() ) mgr->StartAnalysis("grid");
  else if ( terminateOnly ) mgr->StartAnalysis("grid terminate");
  else if ( fRunMode == kLocal ) {
    std::string treeName = ( fFileType == kAOD ) ? "aodTree" : "esdTree";

    TChain* chain = new TChain(treeName.c_str());
    for ( auto& str : fInputData ) chain->Add(str.c_str());
    if (chain) chain->GetListOfFiles()->ls();
    mgr->StartAnalysis("local",chain);
  }
  else if ( fRunMode == kProofLite ) mgr->StartAnalysis("proof");
  else {
    TFileCollection* fc = nullptr;
    if ( fRunMode == kProofOnPod && fIsInputFileCollection ) {
      TFile* file = TFile::Open(fInputData[0].c_str());
      fc = static_cast<TFileCollection*>(file->FindObjectAny("dataset"));
      delete file;
    }
    else if ( fRunMode == kProofLite ) {
      fc = new TFileCollection("dataset");
      fc->AddFromFile("dataset.txt");
    }
    if ( fc ) mgr->StartAnalysis ("proof",fc);
    else mgr->StartAnalysis("proof","dataset.txt");
  }
}

// //_______________________________________________________
// bool AliTaskSubmitter::Submit ( int runMode, const char* inputName, const char* inputOptions, analysisOptions ) const
// {
//   /// Submit task
//   if ( gSystem->AccessPathName(fWorkDir.c_str()) ) {
//     printf("No working directory found. Please run SetupAnalysis first");
//     return false;
//   }
//   std::string currDir = gSystem->pwd();
//   gSystem->cd(fWorkDir.c_str());
//   gSystem->Exec(Form("./runAnalysis.sh %i",runMode));
//   gSystem->cd(currDir.c_str());
//   return true;
// }

//_______________________________________________________
// void AliTaskSubmitter::WriteAnalysisMacro() const
// {
//   /// Write analysis macro
//   std::string outFilename = fWorkDir;
//   outFilename.append("/runTask.C");
//   std::ofstream outFile(outFilename);
//   outFile << "#include \"AliTaskSubmitter.h\"" << std::endl;
//   outFile << "void runTask ( int runMode, const char* inputName = \"dataset.txt\", const char* inputOptions = \"\", const char* analysisOptions = \"\", const char* taskOptions = \"\", const char* softVersions = \"\" )" << std::endl;
//   outFile << "{" << std::endl;
//   outFile << "  AliTaskSubmitter sub;" << std::endl;
//   outFile << "  if ( ! sub.SetupAnalysis(runMode,inputName,inputOptions,analysisOptions,softVersions) ) return;" << std::endl;
//   outFile << std::endl;
//   for ( auto& cfg : fTasks ) {
//     TList* lines = cfg.GetConfigMacro()->GetListOfLines();
//     for ( int iline=1; iline<lines->GetEntries()-1; ++iline ) {
//       TString currLine = lines->At(iline)->GetName();
//       if ( currLine.Contains("__R_ADDTASK__") ) {
//         currLine.ReplaceAll("__R_ADDTASK__",Form("%s(%s)",cfg.GetMacro()->GetName(),cfg.GetMacroArgs()));
//       }
//       outFile << currLine.Data() << std::endl;
//     }
//   }
//   outFile << std::endl;
//   outFile << "  sub.StartAnalysis();" << std::endl;
//   outFile << "}" << std::endl;
//   outFile.close();
// }

//_______________________________________________________
// void AliTaskSubmitter::WriteLoadLibs () const
// {
//   /// Write load libraries
//   std::string outFilename = fWorkDir;
//   outFilename.append("/loadLibs.C");
//   std::ofstream outFile(outFilename);
//   outFile << "#include \"TSystem.h\"" << std::endl;
//   outFile << "#include \"TROOT.h\"" << std::endl;
//   outFile << "void loadLibs ( int runMode )" << std::endl;
//   outFile << "{" << std::endl;
//   outFile << "  gInterpreter->ProcessLine(\".L AliTaskSubmitter.cxx+\");" << std::endl;
//   outFile << "  if ( runMode == " << kProofOnPod << " ) return;" << std::endl;
//   for ( auto& str : fLibraries ) outFile << "  gSystem->Load(\"lib" << str << ".so\");" << std::endl;
//   // for ( auto& str : fIncludePaths ) outFile << "gSystem->AddIncludePath(\"-I" << str << "\")" << std::endl;
//   for ( auto& str : fSources ) outFile << "  gInterpreter->ProcessLine(\".L " << gSystem->BaseName(str.c_str()) <<"+\");" << std::endl;
//   for ( auto& str : fMacros ) {
//     std::string currStr = gSystem->BaseName(str.c_str());
//     if ( str.find("$ALICE_") != std::string::npos ) {
//       currStr = str;
//     }
//     outFile << "  gROOT->LoadMacro(\"" << currStr <<"\");" << std::endl;
//   }
//   outFile << "}" << std::endl;
//   outFile.close();
// }

//_______________________________________________________
// void AliTaskSubmitter::WriteRunScript () const
// {
//   // Write run script
//   std::string outFilename = fWorkDir;
//   outFilename.append("/runTask.sh");
//   std::ofstream outFile(outFilename
//   );
//   outFile << "#!/bin/bash" << std::endl;
//   // outFile << "nWorkers=${1-80}" << endl;
//   // outFile << "vafctl start" << endl;
//   // outFile << "vafreq $nWorkers" << endl;
//   // outFile << "vafwait $nWorkers" << endl;
//   // outFile << "export TASKDIR=\"$HOME/" << fPodOutDir.Data() << "\"" << endl;
//   // outFile << "cd $TASKDIR" << endl;
//   // if ( fProofSplitPerRun ) {
//   //   outFile << "fileList=$(find . -maxdepth 1 -type f ! -name " << fDatasetName.Data() << " | xargs)" << endl;
//   //   outFile << "while read line; do" << endl;
//   //   outFile << "  runNum=$(echo \"$line\" | grep -oE '[0-9][0-9][0-9][1-9][0-9][0-9][0-9][0-9][0-9]' | xargs)" << endl;
//   //   outFile << "  if [ -z \"$runNum\" ]; then" << endl;
//   //   outFile << "    runNum=$(echo \"$line\" | grep -oE [1-9][0-9][0-9][0-9][0-9][0-9] | xargs)" << endl;
//   //   outFile << "  fi" << endl;
//   //   outFile << "  if [ -z \"$runNum\" ]; then" << endl;
//   //   outFile << "    echo \"Cannot find run number in $line\"" << endl;
//   //   outFile << "    continue" << endl;
//   //   outFile << "   elif [ -e \"$runNum\" ]; then" << endl;
//   //   outFile << "    echo \"Run number already processed: skip\"" << endl;
//   //   outFile << "    continue" << endl;
//   //   outFile << "  fi" << endl;
//   //   outFile << "  echo \"\"" << endl;
//   //   outFile << "  echo \"Analysing run $runNum\"" << endl;
//   //   outFile << "  mkdir $runNum" << endl;
//   //   outFile << "  cd $runNum" << endl;
//   //   outFile << "  for ifile in $fileList; do ln -s ../$ifile; done" << endl;
//   //   outFile << "  echo \"$line\" > " << fDatasetName.Data() << endl;
//   // }
//   // TString rootCmd = GetRunMacro();
//   //  rootCmd.Prepend("root -b -q '");
//   //  rootCmd.Append("'");
//   //  outFile << rootCmd.Data() << endl;
//   outFile << "root -b << EOF" << endl;
//   outFile << ".x loadLibs.C+($1)" << endl;
//   outFile << ".x runTask.C($1,\"$2\",\"$3\",\"$4\",\"$5\",\"$6\")" << endl;
//   outFile << ".q" << endl;
//   outFile << "EOF" << endl;
//   // if ( fProofSplitPerRun ) {
//   //   outFile << "cd $TASKDIR" << endl;
//   //   outFile << "done < " << fDatasetName.Data() << endl;
//   //   outFile << "outNames=$(find $PWD/*/ -type f -name \"*.root\" -exec basename {} \\; | sort -u | xargs)" << endl;
//   //   outFile << "for ifile in $outNames; do" << endl;
//   //   TString mergeList = "mergeList.txt";
//   //   outFile << "  find $PWD/*/ -name \"$ifile\" > " << mergeList.Data() << endl;
//   //   outFile << "  root -b -q $ALICE_PHYSICS/PWGPP/MUON/lite/mergeGridFiles.C\\(\\\"$ifile\\\",\\\"" << mergeList.Data() << "\\\",\\\"\\\"\\)" << endl;
//   //   outFile << "  rm " << mergeList.Data() << endl;
//   //   outFile << "done" << endl;
//   // }
//   //  outFile << "root -b <<EOF" << endl;
//   //  outFile << rootCmd.Data() << endl;
//   //  outFile << ".q" << endl;
//   //  outFile << "EOF" << endl;
//   // outFile << "vafctl stop" << endl;
//   // outFile << "exit" << endl;
//   outFile.close();
//   gSystem->Exec(Form("chmod u+x %s",outFilename.c_str()));
// }

// //_______________________________________________________
// Bool_t AliTaskSubmitter::AddCentrality ( Bool_t oldFramework )
// {
//   /// Add centrality in the train
//
//   // Old centrality framework
//   Bool_t treatAsMC = ( fIsMC && ! fIsEmbed );
//   if ( oldFramework ) {
//     if ( gROOT->LoadMacro("$ALICE_PHYSICS/OADB/macros/AddTaskCentrality.C") < 0 ) return kFALSE;
//     printf("Adding old centrality task\n");
//     AliCentralitySelectionTask* centralityTask = reinterpret_cast<AliCentralitySelectionTask*>(gInterpreter->ProcessLine("AddTaskCentrality()"));
//     if ( treatAsMC ) centralityTask->SetMCInput();
//   }
//   else {
//     if ( gROOT->LoadMacro("$ALICE_PHYSICS/OADB/COMMON/MULTIPLICITY/macros/AddTaskMultSelection.C") < 0 ) return kFALSE;
//     printf("Adding centrality task\n");
//     AliMultSelectionTask* centralityTask = reinterpret_cast<AliMultSelectionTask*>(gInterpreter->ProcessLine("AddTaskMultSelection(kFALSE)"));
//     if ( treatAsMC ) centralityTask->SetUseDefaultMCCalib(kTRUE); // MC
//     else centralityTask->SetUseDefaultCalib(kTRUE); // data
//   }
//
//   return kTRUE;
// }
//
// //_______________________________________________________
// Bool_t AliTaskSubmitter::AddPhysicsSelection ()
// {
//   /// Add physics selection task in the train
//
//   if ( gROOT->LoadMacro("$ALICE_PHYSICS/OADB/macros/AddTaskPhysicsSelection.C") < 0 ) return kFALSE;
//
//   printf("Adding physics selection task\n");
//   Bool_t treatAsMC = ( fIsMC && ! fIsEmbed );
//
//   AliPhysicsSelectionTask* physSelTask = reinterpret_cast<AliPhysicsSelectionTask*>(gInterpreter->ProcessLine(Form("AddTaskPhysicsSelection(%i)",treatAsMC)));
//   if ( ! treatAsMC ) physSelTask->GetPhysicsSelection()->SetUseBXNumbers(kFALSE); // Needed if you want to merge runs with different running scheme
//   physSelTask->GetPhysicsSelection()->SetPassName(fPass.Data());
//
//   fHasPhysSelInfo = kTRUE;
//
//   return kTRUE;
// }
//
// //_______________________________________________________
// Bool_t AliTaskSubmitter::CopyDatasetLocally ()
// {
//   /// Copy dataset locally
//   if ( ! fIsPod ) return kFALSE;
//   TString inFilename = fInputName;
//   TString tmpFilename = Form("%s/tmp_dataset.txt",fWorkDir.Data());
//   if ( gSystem->AccessPathName(fInputName) ) {
//     inFilename = tmpFilename;
//     gSystem->Exec(Form("echo '%s' > %s",fInputName.Data(),inFilename.Data()));
//   }
//   else if ( fInputName.EndsWith(".root") ) {
//     fInputName = "dataset.root";
//     TFile::Cp(inFilename.Data(),Form("%s/%s",fWorkDir.Data(),fInputName.Data()));
//     return kFALSE;
//   }
//
//   fInputName = "dataset.txt";
//
//   TString fullFilename = Form("%s/%s",fWorkDir.Data(),fInputName.Data());
//   ofstream outFile(fullFilename.Data());
//   ifstream inFile(inFilename.Data());
//   TString currLine = "";
//   while ( ! inFile.eof() ) {
//     currLine.ReadLine(inFile);
//     if ( currLine.IsNull() ) continue;
//     if ( currLine.Contains("Find;") ) {
//       Int_t index = currLine.Index(";Find");
//       TObjArray findCommands;
//       findCommands.SetOwner();
//       while ( index >= 0 ) {
//         TString currDataset = currLine;
//         currDataset.Remove(0,index);
//         currLine.Remove(index);
//         findCommands.Add(new TObjString(currDataset));
//         index = currDataset.Index(";Find");
//       }
//       findCommands.Add(new TObjString(currLine));
//       for ( Int_t iarr=0; iarr<findCommands.GetEntries(); iarr++ ) {
//         TString currFind = findCommands.At(iarr)->GetName();
//         Int_t index = currFind.Index("Mode=");
//         if ( index>=0 ) {
//           TString currMode = currFind;
//           currMode.Remove(0,index+5);
//           index = currMode.Index(";");
//           if ( index>=0 ) currMode.Remove(index);
//           currFind.ReplaceAll(currMode.Data(),fProofDatasetMode.Data());
//         }
//         else currFind.Append(Form(";Mode=%s;",fProofDatasetMode.Data()));
//         currFind.ReplaceAll("Mode=;","");
//         currFind.ReplaceAll(";;",";");
//         outFile << currFind.Data() << endl;
//       }
//     }
//     else outFile << currLine.Data() << endl;
//   }
//   inFile.close();
//   outFile.close();
//
//   if ( gSystem->AccessPathName(tmpFilename) == 0 ) gSystem->Exec(Form("rm %s",tmpFilename.Data()));
//
//   return kTRUE;
// }
//
// //______________________________________________________________________________
// Bool_t AliTaskSubmitter::ConnectToPod () const
// {
//   if ( ! fIsPod ) return kFALSE;
//
//
//   Bool_t yesToAll = kTRUE;
//   TString remoteDir = Form("%s:%s",fProofServer.Data(),fPodOutDir.Data());
//   TString baseExclude = "--exclude=\"*/\" --exclude=\"*.log\" --exclude=\"outputs_valid\" --exclude=\"*.xml\" --exclude=\"*.jdl\" --exclude=\"plugin_test_copy\" --exclude=\"*.so\" --exclude=\"*.d\"";
//   TString syncOpt = fProofResume ? "--delete" : "--delete-excluded";
//   TString command = Form("%s %s %s ./ %s/",fProofCopyCommand.Data(),syncOpt.Data(),baseExclude.Data(),remoteDir.Data());
//   PerformAction(command,yesToAll);
//   TString execCommand = fProofExecCommand;
//   execCommand.ReplaceAll("nworkers",Form("%i",fProofNworkers));
//   TString updateVersion = Form("sed -i \"s/VafAliPhysicsVersion=.*/VafAliPhysicsVersion=%s/\" .vaf/vaf.conf",fSoftVersion.Data());
//   gSystem->Exec(Form("%s '%s; %s'",fProofOpenCommand.Data(),updateVersion.Data(),execCommand.Data()));
//
//   return kTRUE;
// }
//
//
// //_______________________________________________________
// Bool_t AliTaskSubmitter::CopyFile ( const char* filename ) const
// {
//   /// Copy file to workdir
//   for ( std::string path : fSearchPaths ) {
//     TString fullName = Form("%s/%s",path.c_str(),filename);
//     if ( gSystem->AccessPathName(fullName.Data()) == 0 ) {
//       gSystem->Exec(Form("cp %s %s/",fullName.Data(),fWorkDir.Data()));
//       return kTRUE;
//     }
//   }
//   return kFALSE;
// }
//
// //______________________________________________________________________________
// Bool_t AliTaskSubmitter::CopyPodOutput () const
// {
//   /// Get Pod output from the server and copy it locally
//   if ( fIsPodMachine ) return kFALSE;
//   Bool_t yesToAll = kTRUE;
//   TString remoteDir = Form("%s:%s",fProofServer.Data(),fPodOutDir.Data());
//   PerformAction(Form("%s %s/*.root ./",fProofCopyCommand.Data(),remoteDir.Data()),yesToAll);
// //  printf("*/"); // This line is for xcode that gets confused when it sees the slash star on the previous line and breaks the comment shortcut
//   return kTRUE;
// }
//

//
//
//
// //_______________________________________________________
// TString AliTaskSubmitter::GetRunMacro ( ) const
// {
//   /// Get name of runnin macro
//   TString rootCmd = gSystem->GetFromPipe("tail -n 1 $HOME/.root_hist");
//   rootCmd.ReplaceAll("  "," ");
//   rootCmd.ReplaceAll(".x ","");
//   rootCmd.Remove(TString::kLeading,' ');
//   rootCmd.Remove(TString::kTrailing,' ');
//   return rootCmd;
// }
//
//

//
// //_______________________________________________________
// Bool_t AliTaskSubmitter::Init ( const char* workDir )
// {
//   /// Initialize analysis
//
//   if ( fFileType < 0 ) return kFALSE;
//
//   fUtilityMacros.clear();
//   fUtilityMacros.push_back("SetAlienIO");
//   fUtilityMacros.push_back("BuildMuonEventCuts");
//   fUtilityMacros.push_back("SetupMuonBasedTasks");
//
//   if ( ! SetupLocalWorkDir ( workDir ) ) return kFALSE;
//
//   for ( auto& str : fAdditionalFiles ) CopyFile(str.c_str());
//
//   if ( fIsPodMachine ) fInputName = gSystem->GetFromPipe("ls dataset.*");
//
//   gSystem->cd(fWorkDir.Data());
//   if ( ! fIsPodMachine ) LoadLibsLocally();
//
//   AliAnalysisAlien* plugin = 0x0;
//   if ( fAnalysisMode == kGridAnalysis ) {
//     plugin = CreateAlienHandler();
//     if ( LoadMacro(fUtilityMacros[0].c_str()) ) {
//       gInterpreter->ProcessLine(Form("%s(\"\",\"%s\",(AliAnalysisAlien*)%p)",fUtilityMacros[0].c_str(),fPeriod.Data(),plugin));
//       UnloadMacros();
//     }
//   }
//   else if ( fAnalysisMode == kAAFAnalysis ) {
//     if ( fProofNworkers == 0 ) SetProofNworkers();
//     if ( ! fIsPod || fIsPodMachine ) LoadLibsProof();
//   }
//
//   /// Some utilities for muon analysis
//   if ( ! fLoadAllBranches ) {
//     LoadMacro(fUtilityMacros[1].c_str());
//     LoadMacro(fUtilityMacros[2].c_str());
//   }
//
//   //
//   // Make the analysis manager
//   AliAnalysisManager *mgr = new AliAnalysisManager("testAnalysis");
//   ////  PerformAction(Form("cd %s",outDir.Data()),yesToAll);
//   if ( plugin ) mgr->SetGridHandler(plugin);
//
//   AliInputEventHandler *handler = 0x0;
//   AliMCEventHandler *mcHandler = 0x0;
//
//   if ( fFileType == kAOD ) handler = new AliAODInputHandler();
//   else {
//     AliESDInputHandler* esdH = new AliESDInputHandler();
//     if ( ! fLoadAllBranches ) {
//       esdH->SetReadFriends(kFALSE);
//       esdH->SetInactiveBranches("*");
//       esdH->SetActiveBranches("MuonTracks MuonClusters MuonPads AliESDRun. AliESDHeader. AliMultiplicity. AliESDFMD. AliESDVZERO. AliESDTZERO. SPDVertex. PrimaryVertex. AliESDZDC. SPDPileupVertices");
//     }
//     handler = esdH;
//
//     if ( fIsMC ){
//       // Monte Carlo handler
//       mcHandler = new AliMCEventHandler();
//       printf("\nMC event handler requested\n\n");
//     }
//   }
//   if ( fEventMixing ) {
//     AliMultiInputEventHandler* multiHandler = new AliMultiInputEventHandler();
//     mgr->SetInputEventHandler(multiHandler);
//     multiHandler->AddInputEventHandler(handler);
//     if ( mcHandler ) multiHandler->AddInputEventHandler(mcHandler);
//   }
//   else {
//     mgr->SetInputEventHandler(handler);
//     if ( mcHandler ) mgr->SetMCtruthEventHandler(mcHandler);
//   }
//
//   Bool_t treatAsMC = ( fIsMC && ! fIsEmbed );
//
//   printf("Analyzing %s  MC %i\n", ( fFileType == kAOD ) ? "AODs" : "ESDs", fIsMC);
//
//   fIsInitOk = kTRUE;
//   return kTRUE;
// }
//
// //_______________________________________________________
// Bool_t AliTaskSubmitter::LoadLibsLocally ( ) const
// {
//   /// Load libraries and classes
//   for ( std::string str : fIncludePaths ) gSystem->AddIncludePath(Form("-I%s",str.c_str()));
//
//   for ( std::string str : fLibraries ) {
//     TString currName = gSystem->BaseName(str.c_str());
//     if ( currName.EndsWith(".so") ) gSystem->Load(currName.Data());
//     else if ( currName.EndsWith(".cxx") ) gROOT->LoadMacro(Form("%s+g",currName.Data()));
//     else if ( currName.EndsWith(".par") ) {
//       currName.ReplaceAll(".par","");
//       // The following line is needed since we use AliAnalysisAlien::SetupPar in this function.
//       // The interpreter reads the following part line-by-line, even if the condition is not satisfied.
//       // If the library is not loaded in advance, one gets funny issues at runtime
//       TString foundLib = gSystem->GetLibraries("libANALYSISalice.so","",kFALSE);
//       if ( foundLib.IsNull() ) gSystem->Load("libANALYSISalice.so");
// //      // Then search for the library. If it is already loades, unload it
// //      TString libName = Form("lib%s.so",currName.Data());
// //      foundLib = gSystem->GetLibraries(libName.Data(),"",kFALSE);
// //      if ( ! foundLib.IsNull() ) gSystem->Unload(libName.Data());
//       AliAnalysisAlien::SetupPar(currName.Data());
//     }
//   }
//   return kTRUE;
// }
//
// //_______________________________________________________
// Bool_t AliTaskSubmitter::LoadLibsProof ( ) const
// {
//   /// Load libraries on proof
//   //  if ( ! fIsPod ) {
//   //    TString rootVersion = GetSoftVersion("root",softVersions);
//   //    rootVersion.Prepend("VO_ALICE@ROOT::");
//   //    TProof::Mgr(fProofCluster.Data())->SetROOTVersion(rootVersion.Data());
//   //  }
//   TProof::Open(fProofCluster.Data());
//
//   if (!gProof) return kFALSE;
//
//   TString extraIncs = "";
//   for ( std::string str : fIncludePaths ) extraIncs += Form("%s:",str.c_str());
//   extraIncs.Remove(TString::kTrailing,':');
//
//   TString extraLibs = "";
//   std::vector<std::string> extraPkgs, extraSrcs;
//   for ( std::string str : fLibraries ) {
//     TString currName = str.c_str();
//     if ( ! currName.EndsWith(".so") ) continue;
//     if ( currName.BeginsWith("lib") ) currName.Remove(0,3);
//     currName.ReplaceAll(".so","");
//     extraLibs += Form("%s:",currName.Data());
//   }
//   extraLibs.Remove(TString::kTrailing,':');
//
//   TString alirootMode = "base";
//   Bool_t notOnClient = kFALSE;
//
//   TList* list = new TList();
//   list->Add(new TNamed("ALIROOT_MODE", alirootMode.Data()));
//   list->Add(new TNamed("ALIROOT_EXTRA_LIBS", extraLibs.Data()));
//   list->Add(new TNamed("ALIROOT_EXTRA_INCLUDES", extraIncs.Data()));
//   if ( fAAF != kSAF ) // Temporary fix for saf3: REMEMBER TO CUT this line when issue fixed
//     list->Add(new TNamed("ALIROOT_ENABLE_ALIEN", "1"));
//   TString mainPackage = "";
//   if ( fIsPod ) {
//     TString remotePar = ( fAAF == kSAF ) ? "https://github.com/aphecetche/aphecetche.github.io/blob/master/page/saf3-usermanual/AliceVaf.par?raw=true" : "http://alibrary.web.cern.ch/alibrary/vaf/AliceVaf.par";
//     mainPackage = gSystem->BaseName(remotePar.Data());
//     mainPackage.Remove(mainPackage.Index("?"));
//     printf("Getting package %s\n",remotePar.Data());
//     TFile::Cp(remotePar.Data(), mainPackage.Data());
//     if ( gSystem->AccessPathName(mainPackage) ) printf("Error: cannot get %s from %s\n",mainPackage.Data(),remotePar.Data());
//     //    }
//     //    else {
//     //    // In principle AliceVaf.par should be always taken from the webpage (constantly updated version)
//     //    // However, in SAF, one sometimes need to have custom AliceVaf.par
//     //    // Hence, if an AliceVaf.par is found in the local dir, it is used instead of the official one
//     //      printf("Using custom %s\n",mainPackage.Data());
//     //    }
//   }
//   else if ( fRunMode == kTestMode ) mainPackage = "$ALICE_ROOT/ANALYSIS/macros/AliRootProofLite.par";
//   else {
//     mainPackage = fSoftVersion;
//     mainPackage.Prepend("VO_ALICE@AliPhysics::");
//   }
//   if ( ! mainPackage.BeginsWith("VO_ALICE") ) gProof->UploadPackage(mainPackage.Data());
//   gProof->EnablePackage(mainPackage.Data(),list,notOnClient);
//
//   for ( std::string str : fLibraries ) {
//     if ( str.rfind(".par") != std::string::npos ) {
//       gProof->UploadPackage(str.c_str());
//       gProof->EnablePackage(str.c_str(),notOnClient);
//     }
//     else if ( str.rfind(".cxx") != std::string::npos ) {
//       gProof->Load(Form("%s++g",str.c_str()),notOnClient);
//     }
//   }
//   return kTRUE;
// }
//
// //_______________________________________________________
// Bool_t AliTaskSubmitter::LoadMacro ( const char* macroName ) const
// {
//   /// Load macro
//   TString foundLib = gSystem->GetLibraries(macroName,"",kFALSE);
//   if ( ! foundLib.IsNull() ) return kTRUE;
//   TString macroFile = Form("%s/%s.C",fWorkDir.Data(),macroName);
//   if ( gSystem->AccessPathName(macroFile.Data()) ) return kFALSE;
//   gROOT->LoadMacro(Form("%s+",macroFile.Data()));
//   return kTRUE;
// }
//
// //_______________________________________________________
// Bool_t AliTaskSubmitter::PerformAction ( TString command, Bool_t& yesToAll ) const
// {
//   /// Do something
//
//   TString decision = "y";
//   if ( gROOT->IsBatch() ) yesToAll = kTRUE; // To run with crontab
//
//   if ( ! yesToAll ) {
//     printf("%s ? [y/n/a]\n", command.Data());
//     cin >> decision;
//   }
//
//   Bool_t goOn = kFALSE;
//
//   if ( ! decision.CompareTo("y") )
//     goOn = kTRUE;
//   else if ( ! decision.CompareTo("a") ) {
//     yesToAll = kTRUE;
//     goOn = kTRUE;
//   }
//
//   if ( goOn ) {
//     printf("Executing: %s\n", command.Data());
//     gSystem->Exec(command.Data());
//   }
//
//   return goOn;
// }
//
// //_______________________________________________________
// void AliTaskSubmitter::SetAdditionalFiles ( const char* fileList )
// {
//   /// Set additional files to be copied to the workong directory
//   std::string flist(fileList);
//   std::istringstream flists(flist);
//   for ( std::string str; flists >> str; ) {
//     fAdditionalFiles.push_back(gSystem->BaseName(str.c_str()));
//     if ( std::find(fAdditionalFiles.begin(), fAdditionalFiles.end(), str) == fAdditionalFiles.end() ) {
//       fSearchPaths.push_back(gSystem->DirName(gSystem->ExpandPathName(str.c_str())));
//     }
//   }
// }
//
// //_______________________________________________________
// Bool_t AliTaskSubmitter::SetAliPhysicsBuildDir ( const char* aliphysicsBuildDir )
// {
//   /// Setup aliphysics build dir
//   /// This will be used to produce the par file if needed
//   TString buildDir(aliphysicsBuildDir);
//   gSystem->ExpandPathName(buildDir);
//   fAliPhysicsBuildDir = GetAbsolutePath(buildDir);
//   return ( ! fAliPhysicsBuildDir.IsNull() );
// }
//
// //_______________________________________________________
// void AliTaskSubmitter::SetLibraries ( const char* libraries, const char* includePaths )
// {
//   /// Set libraries and include paths
//   fIncludePaths.clear();
//   std::string incs(includePaths);
//   std::istringstream sincs(incs);
//   for ( std::string str; sincs >> str; ) fIncludePaths.push_back(str);
//
//   fLibraries.clear();
//   std::string libs(libraries);
//   std::istringstream slibs(libs);
//   for ( std::string str; slibs >> str; ) fLibraries.push_back(str);
// }
//
// //_______________________________________________________
// Bool_t AliTaskSubmitter::SetMode ( const char* runMode, const char* analysisMode )
// {
//   /// Set run mode and analysis mode
//   TString rMode(runMode), anMode(analysisMode);
//   rMode.ToLower();
//   anMode.ToLower();
//
//   TString runModeNames[] = {"test","full","merge","terminate"};
//   Int_t nRunModes = sizeof(runModeNames)/sizeof(runModeNames[0]);
//
//   fRunMode = -1;
//   fAnalysisMode = -1;
//
//   for ( Int_t imode=0; imode<nRunModes; imode++ ) {
//     if ( rMode == runModeNames[imode] ) {
//       fRunMode = imode;
//       break;
//     }
//   }
//
//   TString analysisModeNames[3] = {"local","grid","proof"};
//
//   if ( anMode == analysisModeNames[kLocalAnalysis] ) fAnalysisMode = kLocalAnalysis;
//   else if ( anMode == analysisModeNames[kGridAnalysis] ) fAnalysisMode = kGridAnalysis;
//   else if ( anMode == "terminate" ) {
//     fAnalysisMode = kLocalAnalysis;
//     fRunMode = kTerminateMode;
//   }
//   else {
//     SetupProof(analysisMode);
//   }
//
//   if ( fRunMode < 0 ) {
//     printf("Error: cannot recognize runMode: %s\n",runMode);
//     printf("Possible values: test full merge terminate\n");
//     return kFALSE;
//   }
//
//   if ( fAnalysisMode < 0 ) {
//     printf("Error: cannor recognize analysisMode: %s\n",analysisMode);
//     printf("Possible values: local grid terminate vaf saf saf2\n");
//     return kFALSE;
//   }
//
//   fRunModeName = runModeNames[fRunMode];
//   fAnalysisModeName = analysisModeNames[fAnalysisMode];
//
//   return kTRUE;
// }
//
// //_______________________________________________________
// // void AliTaskSubmitter::AddFile ( const char* filename )
// // {
// //   /// Add file
// //   std::string sPath(searchPath);
// //   std::istringstream ssPath(sPath);
// //   for ( std::string str; ssPath >> str; ) {
// //     TString absPath = GetAbsolutePath(str.c_str());
// //     fSearchPaths.push_back(absPath.Data());
// //   }
// // }
//
// //_______________________________________________________
// void AliTaskSubmitter::SetTaskDir ()
// {
//   /// Set task dir
//   TString foundLib = gSystem->GetLibraries(ClassName(),"",kFALSE);
//   fTaskDir = gSystem->DirName(foundLib.Data());
//   //  fTaskDir = taskDir;
//   //  if ( fTaskDir.IsNull() ) fTaskDir = gSystem->Getenv("TASKDIR");
//   //  if ( gSystem->AccessPathName(Form("%s/%s.cxx",fTaskDir.Data(),ClassName())) ) {
//   //    printf("Error: cannot find %s in %s",ClassName(),fTaskDir.Data());
//   //    fTaskDir = "";
//   //    return kFALSE;
//   //  }
//   //  return kTRUE;
// }
//
// //_______________________________________________________
// Bool_t AliTaskSubmitter::SetupAnalysis ( const char* runMode, const char* analysisMode,
//                                         const char* inputName, const char* inputOptions,
//                                         const char* softVersion,
//                                         const char* analysisOptions,
//                                         const char* libraries, const char* includePaths,
//                                         const char* workDir,
//                                         const char* additionalFiles,
//                                         Bool_t isMuonAnalysis )
// {
//   /// Setup analysis
//   if ( ! SetMode(runMode,analysisMode) ) return kFALSE;
//   if ( ! SetInput(inputName,inputOptions) ) return kFALSE;
//   SetSoftVersion(softVersion);
//   SetLibraries(libraries,includePaths);
//   SetAdditionalFiles(additionalFiles);
//   if ( ! isMuonAnalysis ) fLoadAllBranches = kTRUE;
//   TString anOptions(analysisOptions);
//   anOptions.ToUpper();
//   SetMixingEvent(anOptions.Contains("MIXED"));
//   Init(workDir);
//   if ( ! anOptions.Contains("NOPHYSSEL") ) {
//     fHasPhysSelInfo = kTRUE;
//     if ( fFileType != kAOD ) AddPhysicsSelection();
//   }
//   if ( anOptions.Contains("CENTR") ) AddCentrality(anOptions.Contains("OLDCENTR"));
//
//   TString nWorkersStr = anOptions(TRegexp("NWORKERS=[0-9]+"));
//   if ( ! nWorkersStr.IsNull() ) {
//     nWorkersStr.ReplaceAll("NWORKERS=","");
//     if ( nWorkersStr.IsDigit() ) SetProofNworkers(nWorkersStr.Atoi());
//   }
//
//   return kTRUE;
// }
//
// //_______________________________________________________
// Bool_t AliTaskSubmitter::SetupLocalWorkDir ( TString workDir )
// {
//   /// Setup local working directory
//
//   fCurrentDir = gSystem->pwd();
//
//   SetTaskDir();
//   std::string sTaskDir(fTaskDir.Data());
//   fSearchPaths.push_back(sTaskDir);
//   std::string sCurrDir(fCurrentDir.Data());
//   fSearchPaths.push_back(sCurrDir);
//
//   TString classFilename = ClassName();
//   classFilename.Append(".cxx");
//   if ( gSystem->AccessPathName(classFilename.Data()) == 0 ) {
//     if ( ! IsTerminateOnly() ) {
//       printf("Found %s in the current working directory\n",classFilename.Data());
//       printf("Assume you want to re-run local: do not copy files\n");
//     }
//     fWorkDir = fCurrentDir;
//     return kTRUE;
//   }
//
//   if ( workDir.IsNull() ) {
//     workDir = "tmpDir";
//     printf("No workdir specified: creating default %s\n",workDir.Data());
//   }
//
//   Bool_t yesToAll = kFALSE;
//   TString command = "";
//
//   Bool_t makeDir = ( gSystem->AccessPathName(workDir) != 0 );
//   if ( IsTerminateOnly() ) {
//     if ( makeDir ) {
//       printf("\nError: mode %s requires an existing workDir containing the analysis results!\n\n",fRunModeName.Data());
//       return kFALSE;
//     }
//   }
//   else if ( ! makeDir ) {
//     TString workDirFull = GetAbsolutePath(workDir);
//     if ( workDirFull == fCurrentDir.Data() ) return kFALSE;
//     printf("Workdir %s already exist:\n",workDir.Data());
//     command = Form("rm -rf %s",workDir.Data());
//     makeDir = PerformAction(command,yesToAll);
//   }
//
//   if ( makeDir ) {
//     yesToAll = kTRUE;
//     command = Form("mkdir %s",workDir.Data());
//     PerformAction(command,yesToAll);
//   }
//
//   fWorkDir = GetAbsolutePath(workDir);
//   if ( fWorkDir == fCurrentDir.Data() || IsTerminateOnly() ) return kTRUE;
//
//   for ( std::string str : fLibraries ) {
//     TString currFile = str.c_str();
//     if ( currFile.EndsWith(".so") ) continue;
//     else {
//       Bool_t isOk = CopyFile(currFile.Data());
//       if ( ! isOk && currFile.EndsWith(".par") ) {
//         TString command = Form("cd %s; make %s; cd %s; find %s -name %s -exec mv -v {} ./ \\;", fAliPhysicsBuildDir.Data(), currFile.Data(), fWorkDir.Data(), fAliPhysicsBuildDir.Data(), currFile.Data());
//         if ( PerformAction(command, yesToAll) ) {
//           isOk = kTRUE;
//           // Fixes problem with OADB on proof:
//           // the par file only contians the srcs
//           // but if you want to access OADB object they must be inside there!
//           if ( currFile.Contains("OADB") ) {
//             command = "tar -xzf OADB.par; rsync -avu --exclude=.svn --exclude=PROOF-INF.OADB $ALICE_PHYSICS/OADB/ OADB/; tar -czf OADB.par OADB";
//             PerformAction(command, yesToAll);
//           }
//         }
//         else {
//           printf("Error: could not create %s\n", currFile.Data());
//           isOk = kFALSE;
//         }
//         gSystem->Exec(Form("cd %s",fCurrentDir.Data()));
//       }
//       else if ( currFile.EndsWith(".cxx") ) {
//         currFile.ReplaceAll(".cxx",".h");
//         CopyFile(currFile.Data());
//       }
//       if ( ! isOk ) break;
//     }
//   }
//
//   CopyDatasetLocally();
//
//   TString runMacro = GetRunMacro();
//   runMacro.Remove(runMacro.Index("("));
//   runMacro.ReplaceAll("+","");
//   CopyFile(runMacro.Data());
//
//   CopyFile(classFilename.Data());
//   classFilename.ReplaceAll(".cxx",".h");
//   CopyFile(classFilename.Data());
//
//   for ( std::string str : fUtilityMacros ) {
//     CopyFile(Form("%s.C",str.c_str()));
//   }
//
//   if ( fIsPod ) WritePodExecutable();
//
//   return kTRUE;
// }
//


//
// //______________________________________________________________________________
// void AliTaskSubmitter::WritePodExecutable () const

//
//
// //_______________________________________________________
// void AliTaskSubmitter::UnloadMacros() const
// {
//   /// Unload macros
//   for ( std::string str : fUtilityMacros ) {
//     TString foundLib = gSystem->GetLibraries(str.c_str(),"",kFALSE);
//     if ( foundLib.IsNull() ) continue;
//     gSystem->Unload(foundLib.Data());
//   }
// }
//
// //
// ////_______________________________________________________
// //void AliTaskSubmitter::PrintOptions() const
// //{
// //  /// Print recognised options
// //  printf("\nList of recognised options:\n");
// //  printf("  runMode: test full merge terminate\n");
// //  printf("  analysisMode: local grid saf saf2 vaf terminateonly\n");
// //  printf("  inputName: <runNumber> <fileWithRunList> <rootFileToAnalyse(absolute path)>\n");
// //  printf("  inputOptions: Data/MC FULL/EMBED AOD/ESD <period> <pass> <dataPattern> <dataDir>\n");
// //  printf("  softVersions: aliphysics=version,aliroot=version,root=version\n");
// //  printf("  analysisOptions: NOPHYSSEL CENTR OLDCENTR MIXED SPLIT\n");
// //}
// //
// //
// //
// //
//
// ////_______________________________________________________
// //Bool_t LoadAddTasks ( TString libraries )
// //{
// //  TObjArray* libList = libraries.Tokenize(" ");
// //  for ( Int_t ilib=0; ilib<libList->GetEntries(); ilib++) {
// //    TString currName = gSystem->BaseName(libList->At(ilib)->GetName());
// //    if ( currName.EndsWith(".C") ) gROOT->LoadMacro(currName.Data());
// //  }
// //  delete libList;
// //  return kTRUE;
// //}
// //
// //
// //
// //______________________________________________________________________________
// void AliTaskSubmitter::WriteTemplateRunTask ( const char* outputDir ) const
// {
//   /// Write a template macro
//   TString macroName = Form("%s/runTask.C",outputDir);
//   ofstream outFile(macroName.Data());
//   outFile << "void runTask ( const char* runMode, const char* analysisMode," << endl;
//   outFile << "  const char* inputName," << endl;
//   outFile << "  const char* inputOptions = \"\"," << endl;
//   outFile << "  const char* softVersion = \"\"," << endl;
//   outFile << "  const char* analysisOptions = \"\"," << endl;
//   outFile << "  const char* taskOptions = \"\" )" << endl;
//   outFile << "{" << endl;
//   outFile << endl;
//   outFile << "  gSystem->AddIncludePath(\"-I$ALICE_ROOT/include -I$ALICE_PHYSICS/include\");" << endl;
//   outFile << "  gROOT->LoadMacro(gSystem->ExpandPathName(\"$TASKDIR/" << ClassName() << ".cxx+\"));" << endl;
//   outFile << "  AliTaskSubmitter sub;" << endl;
//   outFile << endl;
//   outFile << "  if ( !  sub.SetupAnalysis(runMode,analysisMode,inputName,inputOptions,softVersion,analysisOptions, \"yourLibs\",\"$ALICE_ROOT/include $ALICE_PHYSICS/include\",\"\") ) return;" << endl;
//   outFile << endl;
//   outFile << "  AliAnalysisAlien* plugin = (AliAnalysisAlien*)AliAnalysisManager::GetAnalysisManager()->GetGridHandler();" << endl;
//   outFile << "  if ( plugin ) plugin->SetGridWorkingDir(\"workDirRelativeToHome\");" << endl;
//   outFile << endl;
//   outFile << "//  Bool_t isMC = sub.IsMC();" << endl;
//   outFile << endl;
//   outFile << "  gROOT->LoadMacro(\"yourAddTask.C\");" << endl;
//   outFile << "  AliAnalysisTask* task = yourAddTask();" << endl;
//   outFile << endl;
//   outFile << "//  AliMuonEventCuts* eventCuts = BuildMuonEventCuts(sub.GetMap());" << endl;
//   outFile << endl;
//   outFile << "//  SetupMuonBasedTask(task,eventCuts,taskOptions,sub.GetMap());" << endl;
//   outFile << endl;
//   outFile << "  sub.StartAnalysis();" << endl;
//   outFile << "}" << endl;
//   outFile.close();
// }
