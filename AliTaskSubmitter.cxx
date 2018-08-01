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
#include "TProof.h"
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

//_______________________________________________________
AliTaskSubmitter::AliTaskSubmitter() :
fHasCentralityInfo(false),
fHasPhysSelInfo(false),
fIsEmbed(false),
fIsInputFileCollection(false),
fIsMC(false),
fIsPodMachine(false),
fProofResume(false),
fProofSplitPerRun(false),
fFileType(kAOD),
fProofNworkers(80),
fRunMode(kLocal),
fGridTestFiles(1),
fAlienUsername(),
fAliPhysicsBuildDir(),
fGridDataDir(),
fGridDataPattern(),
fGridWorkingDir(),
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
fInputData(),
fLibraries(),
fMacros(),
fPackages(),
fSources(),
fTasks(),
fKeywords(),
fUtilityMacros(),
fMap(),
fPlugin(nullptr)
// fInputObject(nullptr)
{
  /// Ctr
  fSubmitterDir = gSystem->DirName(gSystem->GetLibraries("AliTaskSubmitter","",kFALSE));

  fUtilityMacros["SetAlienIO.C"] = 2;
  fUtilityMacros["BuildMuonEventCuts.C"] = 0;
  fUtilityMacros["SetupMuonBasedTask.C"] = 0;
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

  AliAnalysisManager::GetAnalysisManager()->SetGridHandler(fPlugin);

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

  // Set libraries
  fPlugin->SetAdditionalRootLibs("libGui.so libProofPlayer.so libXMLParser.so");

  std::stringstream extraLibs;
  for ( auto& str : fLibraries ) extraLibs << "lib" << str << ".so ";

  std::stringstream extraSrcs;
  for ( auto& str : fSources ) {
    extraSrcs << str << " ";
    std::string from = ".cxx";
    std::string header = str;
    header.replace(str.find(from),from.length(),".h");
    extraLibs << header << " " << str << " ";
  }

  for ( auto& entry : fUtilityMacros ) {
    if ( entry.second == 1 ) extraSrcs << entry.first << " ";
  }

  fPlugin->SetAdditionalLibs(extraLibs.str().c_str());
  fPlugin->SetAnalysisSource(extraSrcs.str().c_str());

  // if ( fRunMode == kLocal || fRunMode == kProofLite )
  fPlugin->SetFileForTestMode("dataset.txt");
  fPlugin->SetProofCluster(fProofCluster.c_str());

  // for ( std::string str : fIncludePaths ) plugin->AddIncludePath(Form("-I%s",str.c_str()));

  if ( ! IsGrid() ) return;

  fPlugin->SetAPIVersion("V1.1x");
  fPlugin->SetAliPhysicsVersion(fSoftVersion.c_str());
  fPlugin->SetOutputToRunNo();
  fPlugin->SetNumberOfReplicas(2);
  fPlugin->SetDropToShell(kFALSE);
  fPlugin->SetCheckCopy(kFALSE); // Fixes issue with alien_CLOSE_SE
  fPlugin->SetNtestFiles(fGridTestFiles);

  if ( fRunMode != kGridTerminate ) fPlugin->SetMergeViaJDL();

  // Set packages
  for ( auto& str : fPackages ) fPlugin->EnablePackage(str.c_str());

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

  if ( fGridWorkingDir.empty() ) {
    std::cout << std::endl;
    if ( fPeriod.empty() ) {
      std::cout << "WARNING: GridWorkingDir is not set." << std::endl;
    }
    else {
      // Set grid work dir (tentatively)
      std::stringstream ss;
      if ( fIsMC ) ss << "mcAna";
      else ss << "analysis";

      std::string currDirName = gSystem->pwd();
      currDirName.erase(0,currDirName.find_last_of("/")+1);

      ss << "/" << fPeriod << "/" << currDirName;
      fGridWorkingDir = ss.str();
      std::cout << "WARNING: setting a custom grid working dir:" << fGridWorkingDir << std::endl;
    }

    std::cout << " If you want to set it yourself, you have 2 ways to set it:" << std::endl;
    std::cout << "1) with SetWorkingDir method of AliTaskSubmitter" << std::endl;
    std::cout << "2) In your macro with: " << std::endl;
    std::cout << " AliAnalysisAlien* plugin = static_cast<AliAnalysisAlien*>(AliAnalysisManager::GetAnalysisManager()->GetGridHandler());" << std::endl;
    std::cout << "if ( plugin ) plugin->SetGridWorkingDir(\"workDirRelativeToHome\");" << std::endl << std::endl;
  }

  fPlugin->AddIncludePath("-I$ALICE_ROOT/include -I$ALICE_PHYSICS/include");
  fPlugin->SetGridWorkingDir(fGridWorkingDir.c_str());
  fPlugin->SetGridDataDir(fGridDataDir.c_str());
  fPlugin->SetDataPattern(fGridDataPattern.c_str());
}

//_______________________________________________________
std::string AliTaskSubmitter::GetAbsolutePath ( const char* path ) const
{
  /// Get absolute path
  std::string currDir = gSystem->pwd();
  std::string exppath = path;
  if ( exppath.find("$") != std::string::npos ) exppath = gSystem->ExpandPathName(path);
  if ( gSystem->AccessPathName(exppath.c_str()) ) {
    std::cout << "Error: cannot access " << path << std::endl;
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
    basePath.erase(basePath.begin(),basePath.begin()+basePath.find(runNum)+runNum.length());
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

  bool loadProof = ( fRunMode == kProofSaf2 || fIsPodMachine );

  if ( loadProof ) {
    if ( ! LoadProof() ) return false;
  }
  else {
    // Load locally
    for ( auto& str : fPackages ) AliAnalysisAlien::SetupPar(str.c_str());
    for ( auto& str : fSources ) gInterpreter->ProcessLine(Form(".L %s+",str.c_str()));
    for ( auto& entry : fUtilityMacros ) {
      if ( entry.second == 1 ) {
        gInterpreter->ProcessLine(Form(".L %s+",entry.first.c_str()));
      }
    }
  }


  // // Load the tasks in the plugin (and attach them to the manager)
  // See comment in SetupTasks.
  // if ( ! fPlugin->LoadModules() ) return false;
  for ( auto& cfg : fTasks ) {
    if (!cfg.CheckLoadLibraries()) {
      std::cout << "Error: Cannot load all libraries for module " << cfg.GetName() << std::endl;
      return false;
    }
    // Execute the macro
   if (cfg.ExecuteMacro()<0) {
      std::cout << "Error: executing the macro " << cfg.GetMacroName() << " with arguments: " << cfg.GetMacroArgs() << " for module " << cfg.GetName() << " returned a negative value" << std::endl;
      return false;
   }
   // Configure dependencies
   if (cfg.GetConfigMacro() && cfg.ExecuteConfigMacro()<0) {
      std::cout << "Error: there was an error executing the deps config macro " << cfg.GetConfigMacro()->GetTitle() << " for module " << cfg.GetName() << std::endl;
      return kFALSE;
   }
  }

  if ( IsGrid() ) {
    // // In principle, the additional sources should be passed to the jdl
    // // But the plugin does not do this and expects the sources to be included
    // // in the additional libraries.
    // // Unfortunately, the list of libraries is reset at LoadModules
    // // and re-created from the module itself.
    // // This piece of code restores the sources so that they can be written to jdl
    // std::string from = ".cxx";
    // for ( auto& str : fSources ) {
    //   std::string header = str;
    //   header.replace(str.find(from),from.length(),".h");
    //   fPlugin->AddAdditionalLibrary(header.c_str());
    //   fPlugin->AddAdditionalLibrary(str.c_str());
    // }

    // If we are on grid, use the custom macro to setup the alien IO
    if ( ! fPeriod.empty() ) {
      gInterpreter->ProcessLine(".L SetAlienIO.C+");
      gInterpreter->ProcessLine(Form("TString inputOpts; SetAlienIO(inputOpts,\"%s\",(AliAnalysisAlien*)%p)",fPeriod.c_str(),fPlugin));
      gSystem->Unload("SetAlienIO.C");
    }
  }


  // // Unload the utility macros
  // for ( auto& entry : fUtilityMacros ) {
  //   if ( entry.second == 1 || (entry.second == 2 && loadAlienSetupIO) ) {
  //     gSystem->Unload(entry.first.c_str());
  //   }
  // }

  return true;
}

//_______________________________________________________
bool AliTaskSubmitter::LoadProof() const
{
  /// Load libraries on proof
  //  if ( ! fIsPod ) {
  //    TString rootVersion = GetSoftVersion("root",softVersions);
  //    rootVersion.Prepend("VO_ALICE@ROOT::");
  //    TProof::Mgr(fProofCluster.Data())->SetROOTVersion(rootVersion.Data());
  //  }

  if ( fRunMode != kProofSaf2 && ! fIsPodMachine ) return true;

  TProof::Open(fProofCluster.c_str());

  if ( ! gProof ) return false;

  std::string extraIncs = ".";
  // for ( std::string str : fIncludePaths ) extraIncs += Form("%s:",str.c_str());
  // extraIncs.Remove(TString::kTrailing,':');

  std::string extraLibs = "";
  for ( auto& str : fLibraries ) {
    if ( ! extraLibs.empty() ) extraLibs.append(":");
    extraLibs += str;
    // The AliAnalysisTaskCfg already strips lib and .so
    // So the following code is not needed
    // TString currName = str.c_str();
    // if ( currName.BeginsWith("lib") ) currName.Remove(0,3);
    // currName.ReplaceAll(".so","");
    // if ( ! extraLibs.empty() ) extraLibs.append(":");
    // extraLibs += currName.Data();
  }

  std::string alirootMode = "base";
  bool notOnClient = false;

  TList* list = new TList();
  list->Add(new TNamed("ALIROOT_MODE", alirootMode.c_str()));
  list->Add(new TNamed("ALIROOT_EXTRA_LIBS", extraLibs.c_str()));
  list->Add(new TNamed("ALIROOT_EXTRA_INCLUDES", extraIncs.c_str()));
  if ( fRunMode != kProofSaf ) // Temporary fix for saf3: REMEMBER TO CUT this line when issue fixed
    list->Add(new TNamed("ALIROOT_ENABLE_ALIEN", "1"));
  std::string mainPackage = "";
  if ( fIsPodMachine ) {
    std::string remotePar = ( fRunMode == kProofSaf ) ? "https://github.com/aphecetche/hugo-aphecetche/blob/master/static/page/saf3-usermanual/AliceVaf.par?raw=true" : "http://alibrary.web.cern.ch/alibrary/vaf/AliceVaf.par";
    mainPackage = gSystem->BaseName(remotePar.c_str());
    mainPackage.erase(mainPackage.find("?"));
    std::cout << "Getting package: " << remotePar << std::endl;
    // TFile::Cp(remotePar.c_str(), mainPackage.c_str());
    gSystem->Exec(Form("wget '%s' -O %s",remotePar.c_str(), mainPackage.c_str()));
    if ( gSystem->AccessPathName(mainPackage.c_str()) ) {
      std::cout << "Error: cannot get " << mainPackage << " from " << remotePar << std::endl;
      return false;
    }
    //    }
    //    else {
    //    // In principle AliceVaf.par should be always taken from the webpage (constantly updated version)
    //    // However, in SAF, one sometimes need to have custom AliceVaf.par
    //    // Hence, if an AliceVaf.par is found in the local dir, it is used instead of the official one
    //      printf("Using custom %s\n",mainPackage.Data());
    //    }
  }
  else if ( fRunMode == kProofLite ) mainPackage = "$ALICE_ROOT/ANALYSIS/macros/AliRootProofLite.par";
  else {
    mainPackage = fSoftVersion;
    mainPackage.insert(0,"VO_ALICE@AliPhysics::");
  }
  if ( mainPackage.find("VO_ALICE") == std::string::npos ) gProof->UploadPackage(mainPackage.c_str());
  gProof->EnablePackage(mainPackage.c_str(),list,notOnClient);

  for ( auto& str : fPackages ) {
    gProof->UploadPackage(str.c_str());
    gProof->EnablePackage(str.c_str(),notOnClient);
  }

  for ( auto& str : fSources ) {
    gProof->Load(Form("%s+g",str.c_str()),notOnClient);
  }

  for ( auto& entry : fUtilityMacros ) {
    if ( entry.second == 1 ) {
      gProof->Load(Form("%s+g",entry.first.c_str()),notOnClient);
    }
  }

  return true;
}

//_______________________________________________________
int AliTaskSubmitter::ReplaceKeywords ( std::string& input ) const
{
  /// Replace kewyord
  if ( input.find("__VAR_") == std::string::npos ) return 0;

  for ( auto& key : fKeywords ) {
    size_t idx = input.find(key.first);
    while ( idx != std::string::npos ) {
      input.replace(idx,key.first.length(),key.second);
      idx = input.find(key.first);
    }
  }

  if ( input.find("__VAR_") != std::string::npos ) {
    std::cout << "Error: cannot replace variable in " << input << std::endl;
    return -1;
  }

  return 1;
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
  SetupProof(analysisOptions);
  if ( IsPod() && ! fIsPodMachine ) WriteRunScript (runMode, inputOptions, analysisOptions, taskOptions,  isMuonAnalysis);


  // Setup train
  std::string anOpts(analysisOptions);
  std::transform(anOpts.begin(), anOpts.end(), anOpts.begin(), ::toupper);

  // Parse tasks and add them to the list
  if ( anOpts.find("NOPHYSSEL") == std::string::npos ) {
    fHasPhysSelInfo = true;
    if ( fFileType != kAOD ) AddTask(Form("%s/physSelTask.cfg",fSubmitterDir.c_str()));
  }
  if ( anOpts.find("CENTR") != std::string::npos ) {
    fHasCentralityInfo = true;
    std::string centrCfgFilename = ( anOpts.find("OLDCENTR") != std::string::npos ) ? "centralityTask.cfg" : "multSelectionTask.cfg";
    AddTask(Form("%s/%s",fSubmitterDir.c_str(),centrCfgFilename.c_str()) );
    // AddCentrality(anOptions.Contains("OLDCENTR"));
  }
  AddTask("train.cfg");

  AliAnalysisManager *mgr = new AliAnalysisManager("testAnalysis");
  CreateAlienHandler();

  SetupHandlers(analysisOptions, isMuonAnalysis);

  // Setup the tasks and add them to the plugin
  SetupTasks();

  std::cout << "Analyzing " << (( fFileType == kAOD ) ? "AODs" : "ESDs") << "  MC " << fIsMC << std::endl;

  StartAnalysis();

  return true;
}

//______________________________________________________________________________
bool AliTaskSubmitter::RunPod () const
{
  std::string remoteDir = Form("%s:%s",fProofServer.c_str(),fPodOutDir.c_str());
  std::string baseExclude = "--exclude=\"*/\" --exclude=\"*.log\" --exclude=\"outputs_valid\" --exclude=\"*.xml\" --exclude=\"*.jdl\" --exclude=\"plugin_test_copy\" --exclude=\"*.so\" --exclude=\"*.d\" --exclude=\"*.pcm\"";
  std::string syncOpt = fProofResume ? "--delete" : "--delete-excluded";
  std::string command = Form("%s %s %s ./ %s/",fProofCopyCommand.c_str(),syncOpt.c_str(),baseExclude.c_str(),remoteDir.c_str());
  gSystem->Exec(command.c_str());
  std::string updateVersion = Form("sed -i \"s/VafAliPhysicsVersion=.*/VafAliPhysicsVersion=%s/\" .vaf/vaf.conf",fSoftVersion.c_str());
  int exitCode = gSystem->Exec(Form("%s '%s; %s'",fProofOpenCommand.c_str(),updateVersion.c_str(),fProofExecCommand.c_str()));

  if ( exitCode != 0 ) {
    std::cout << "Error in the execution on PoD" << std::endl;
    return false;
  }

  /// Get Pod output from the server and copy it locally
  exitCode = gSystem->Exec(Form("%s %s/*.root ./",fProofCopyCommand.c_str(),remoteDir.c_str()));

  if ( exitCode != 0 ) {
    std::cout << "Cannot get analysis output from PoD" << std::endl;
    return false;
  }

  return true;
}

//_______________________________________________________
void AliTaskSubmitter::SetKeywords ()
{
  /// Set keywords
  fKeywords["__VAR_ISEMBED__"] = fIsEmbed ? "true" : "false";
  fKeywords["__VAR_ISAOD__"] = IsAOD() ? "true" : "false";
  fKeywords["__VAR_ISMC__"] = fIsMC ? "true" : "false";
  fKeywords["__VAR_PASS__"] = Form("\"%s\"",fPeriod.c_str());
  fKeywords["__VAR_PERIOD__"] = Form("\"%s\"",fPass.c_str());
  fKeywords["__VAR_TASKOPTIONS__"] = Form("\"%s\"",fTaskOptions.c_str());
  fKeywords["__VAR_MAP__"] = Form("(TMap*)%p",GetMap());
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
    gSystem->mkdir(fWorkDir.c_str(),true);
  }

  // Copy necessary add tasks and utility macros
  TString sCfgList(cfgList);
  TObjArray* arr = sCfgList.Tokenize(",");
  TIter nextCfgFile(arr);
  TObject* cfgFilename = 0x0;
  std::ofstream outFile(Form("%s/train.cfg",fWorkDir.c_str()));
  while ( (cfgFilename = nextCfgFile()) ) {
    if ( gSystem->AccessPathName(cfgFilename->GetName()) ) {
      std::cout << "Error: cannot find " << cfgFilename->GetName() << std::endl;
      return false;
    }
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
          if ( str.find(".par") != std::string::npos ) {
            if ( gSystem->AccessPathName(str.c_str()) ) {
              // Try to build the par files
              if ( ! SetAliPhysicsBuildDir() ) {
                std::cout << "Cannot find par file and cannot build it" << std::endl;
                return false;
              }
              std::string absWorkDir = GetAbsolutePath(fWorkDir.c_str());
              std::string command = Form("cd %s; make %s; find . -name %s -exec mv -v {} %s/ \\;", fAliPhysicsBuildDir.c_str(), str.c_str(), str.c_str(), absWorkDir.c_str());
              if ( gSystem->Exec(command.c_str()) == 0 ) {
                if ( str.find("OADB") != std::string::npos ) {
                  // Fixes problem with OADB on proof:
                  // the par file only contians the srcs
                  // but if you want to access OADB object they must be inside there!
                  command = Form("cd %s; tar -xzf OADB.par; rsync -avu --exclude=.svn --exclude=PROOF-INF.OADB $ALICE_PHYSICS/OADB/ OADB/; tar -czf OADB.par OADB",fWorkDir.c_str());
                }
              }
              else return false;
            }
          } // is par file
          else {
            if ( ! CopyFile(str.c_str()) ) return false;
            std::string from = ".cxx";
            size_t idx = str.find(from);
            if ( idx != std::string::npos ) {
              str.replace(idx,from.length(),".h");
              if ( ! CopyFile(str.c_str()) ) return false;
            }
          }
        }
      }

      // Check if the task uses some utility macro (and copy it locally)
      for ( auto& entry : fUtilityMacros ) {
        std::string macroName = entry.first;
        macroName.erase(macroName.find_last_of("."));
        if ( currLine.Contains(macroName.c_str()) ) {
          entry.second = 1;
        }
      }
    }
    inFile.close();
  }
  outFile.close();
  delete arr;

  if ( ! CopyFile(Form("%s/AliTaskSubmitter.cxx",fSubmitterDir.c_str())) ) return false;
  if ( ! CopyFile(Form("%s/AliTaskSubmitter.h",fSubmitterDir.c_str())) ) return false;
  for ( auto& entry : fUtilityMacros ) {
    if ( entry.second == 0 ) continue;
    if ( ! CopyFile(Form("%s/%s",fSubmitterDir.c_str(),entry.first
    .c_str())) ) return false;
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
bool AliTaskSubmitter::SetAlienUsername ( const char* username )
{
  /// Set alien username needed to connect to some proof clusters

  if ( username == nullptr ) {
    if ( ! fAlienUsername.empty() ) return true;
    // Username not provided, try to guess it
    if ( gSystem->Getenv("alien_API_USER") ) {
      fAlienUsername = gSystem->Getenv("alien_API_USER");
      return true;
    }


    std::cout << "Please explicitly specify an alien username with SetAlienUsername" << std::endl;
    return false;
  }

  fAlienUsername = username;
  return true;
}

//_______________________________________________________
bool AliTaskSubmitter::SetAliPhysicsBuildDir ( const char* aliphysicsBuildDir )
{
  /// Setup aliphysics build dir
  /// This will be used to produce the par file if needed
  if ( aliphysicsBuildDir == nullptr ) {
    if ( ! fAliPhysicsBuildDir.empty() ) return true;
    // Try to find the location of the build dir
    if ( gSystem->Getenv("ALICE_PHYSICS") ) {
      std::string guessedPath = gSystem->Getenv("ALICE_PHYSICS");
      std::string filename = Form("%s/relocate-me.sh",guessedPath.c_str());
      if ( gSystem->AccessPathName(filename.c_str()) == 0 ) {
        std::ifstream inFile(filename.c_str());
        TString currLine = "", found = "";
        while ( currLine.ReadLine(inFile) ) {
          if ( currLine.BeginsWith("PH=") ) {
            found = currLine;
            found.ReplaceAll("PH=","");
            break;
          }
        }
        inFile.close();
        if ( ! found.IsNull() ) {
          for ( int islash=0; islash<3; ++islash ) guessedPath.erase(guessedPath.find_last_of("/"));

          guessedPath += Form("/BUILD/%s/AliPhysics",found.Data());
          if ( gSystem->AccessPathName(guessedPath.c_str()) == 0 ) {
            fAliPhysicsBuildDir = guessedPath;
            std::cout << "Found aliphysics build dir " << fAliPhysicsBuildDir << std::endl;
            return true;
          }
        }
      } // relocate file exists
    } // ALICE_PHYSICS is defined
    std::cout << "Please define the aliphysics build dir with SetAliPhysicsBuildDir so that the par files can be automatically built" << std::endl;
    return false;
  } // aliphysics build dir is null
  fAliPhysicsBuildDir = GetAbsolutePath(aliphysicsBuildDir);
  return ( ! fAliPhysicsBuildDir.empty() );
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
  if ( fPeriod.empty() ) std::cout << "Warning: cannot determine period" << std::endl;

  fPass = GetPass(sOpt.c_str());
  if ( fPass.empty() ) fPass = GetPeriod(checkString.c_str());
  if ( fPass.empty() ) std::cout << "Warning: cannot determine pass" << std::endl;

  fIsMC = ( sOpt.find("MC") != std::string::npos );
  fIsEmbed = ( sOpt.find("EMBED") != std::string::npos );

  if ( IsGrid() ) {
    fGridDataDir = GetGridDataDir(checkString.c_str());
    fGridDataPattern = GetGridDataPattern(checkString.c_str());
  }

  // Write input
  std::string inputPath = "";
  std::string currDir = gSystem->pwd();
  if ( gSystem->AccessPathName(inName.c_str()) == 0 ) inputPath = GetAbsolutePath(gSystem->DirName(inName.c_str()));

  if ( inputPath != currDir ) {
    // Check that we're not copying the same file
    if ( fIsInputFileCollection ) CopyFile(inName.c_str(), "dataset.root");

    std::ofstream outFile("dataset.txt");
    for ( auto& str : fInputData ) outFile << str << std::endl;
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
bool AliTaskSubmitter::SetupProof ( const char* analysisOptions )
{
  /// Setup proof info
  fPodOutDir = "taskDir";

  TString anOptions(analysisOptions);
  TString nWorkersStr = anOptions(TRegexp("NWORKERS=[0-9]+"));
  if ( ! nWorkersStr.IsNull() ) {
    nWorkersStr.ReplaceAll("NWORKERS=","");
    if ( nWorkersStr.IsDigit() ) SetProofNworkers(nWorkersStr.Atoi());
  }

  std::string runPodCommand = Form("\"%s/runPod.sh %i\"",fPodOutDir.c_str(), fProofNworkers);

  if ( fRunMode == kProofSaf2 ) {
    if ( ! SetAlienUsername() ) return false;
    fProofCluster = Form("%s@nansafmaster2.in2p3.fr",fAlienUsername.c_str());
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
    if ( ! SetAlienUsername() ) return false;
    Int_t lxplusTunnelPort = 5501;
    fProofCluster = "pod://";
    fProofServer = "localhost";
    fProofCopyCommand = Form("rsync -avcL -e 'ssh -p %i'",lxplusTunnelPort);
    fProofOpenCommand = Form("ssh %s@localhost -p %i -t",fAlienUsername.c_str(),lxplusTunnelPort);
    fProofExecCommand = Form("echo %s | /usr/bin/vaf-enter",runPodCommand.c_str());
    fProofDatasetMode = "remote";
    std::cout << "This mode requires that you have an open ssh tunnel to CERN machine over port 5501" << std::endl;
    std::cout << "If you don't, you can set one with:" << std::endl;
    std::cout << "ssh your_user_name@lxplus.cern.ch -L 5501:alivaf-001:22" << std::endl;
  }
  else if ( fRunMode == kProofLite ) {
    fProofCluster = "localhost";
    // fProofServer = "localhost";
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

    if ( cfg.GetConfigMacro() ) {

      TList* lines = cfg.GetConfigMacro()->GetListOfLines();
      TIter next(lines);
      TObjString* objString = nullptr;
      while ( (objString = static_cast<TObjString*>(next())) ) {
        if ( ReplaceKeywords(objString) == -1 ) return false;

        // Check if the task uses some utility macro (and copy it locally)
        for ( auto& entry : fUtilityMacros ) {
          std::string macroName = entry.first;
          macroName.erase(macroName.find_last_of("."));
          if ( objString->String().Contains(macroName.c_str()) ) {
            entry.second = 1;
          }
        }
      }
    }
    // When we add a module to the plugin, it takes over libraries, sources, etc.
    // The modules works only if all of the code is in aliphysics,
    // but it is not optimized for custom code to be deployed.
    // So let's avoid the automatic module loading
    // fPlugin->AddModule(&cfg);
  }

  return true;
}

//______________________________________________________________________________
void AliTaskSubmitter::StartAnalysis () const
{
  /// Run the analysis
  // UnloadMacros();

  // Load everything before running on PoD
  // Indeed, if something goes wrong locally
  // there is no need to try to run it on PoD
  if ( ! Load() ) return;

  bool terminateOnly = ( fRunMode == kLocalTerminate );
  // Bool_t terminateOnly = IsTerminateOnly();
  if ( IsPod() && ! fIsPodMachine ) {
   if ( ! RunPod() ) return;
   terminateOnly = true;
  }

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
    if ( IsPod() && fIsInputFileCollection ) {
      TFile* file = TFile::Open(fInputData[0].c_str());
      fc = static_cast<TFileCollection*>(file->FindObjectAny("dataset"));
      delete file;
    }
    // REMEMBER TO CHECK
    // else if ( fRunMode == kProofLite ) {
    //   fc = new TFileCollection("dataset");
    //   fc->AddFromFile("dataset.txt");
    // }
    if ( fc ) mgr->StartAnalysis ("proof",fc);
    else {
      mgr->SetGridHandler(nullptr);
      mgr->StartAnalysis("proof","dataset.txt");
    }
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
void AliTaskSubmitter::WriteRunScript ( int runMode, const char* inputOptions, const char* analysisOptions, const char* taskOptions, bool isMuonAnalysis ) const
{
  // Write run script
  std::string outFilename = "runPod.sh";
  std::ofstream outFile(outFilename.c_str());
  std::string inputName = fIsInputFileCollection ? "dataset.root" : "dataset.txt";
  outFile << "#!/bin/bash" << std::endl;
  outFile << "nWorkers=${1-80}" << std::endl;
  outFile << "vafctl start" << std::endl;
  outFile << "vafreq $nWorkers" << std::endl;
  outFile << "vafwait $nWorkers" << std::endl;
  outFile << "cd " << fPodOutDir << std::endl;
  // if ( fProofSplitPerRun ) {
  //   outFile << "fileList=$(find . -maxdepth 1 -type f ! -name " << fDatasetName.Data() << " | xargs)" << endl;
  //   outFile << "while read line; do" << endl;
  //   outFile << "  runNum=$(echo \"$line\" | grep -oE '[0-9][0-9][0-9][1-9][0-9][0-9][0-9][0-9][0-9]' | xargs)" << endl;
  //   outFile << "  if [ -z \"$runNum\" ]; then" << endl;
  //   outFile << "    runNum=$(echo \"$line\" | grep -oE [1-9][0-9][0-9][0-9][0-9][0-9] | xargs)" << endl;
  //   outFile << "  fi" << endl;
  //   outFile << "  if [ -z \"$runNum\" ]; then" << endl;
  //   outFile << "    echo \"Cannot find run number in $line\"" << endl;
  //   outFile << "    continue" << endl;
  //   outFile << "   elif [ -e \"$runNum\" ]; then" << endl;
  //   outFile << "    echo \"Run number already processed: skip\"" << endl;
  //   outFile << "    continue" << endl;
  //   outFile << "  fi" << endl;
  //   outFile << "  echo \"\"" << endl;
  //   outFile << "  echo \"Analysing run $runNum\"" << endl;
  //   outFile << "  mkdir $runNum" << endl;
  //   outFile << "  cd $runNum" << endl;
  //   outFile << "  for ifile in $fileList; do ln -s ../$ifile; done" << endl;
  //   outFile << "  echo \"$line\" > " << fDatasetName.Data() << endl;
  // }
  // TString rootCmd = GetRunMacro();
  // rootCmd.Prepend("root -b -q '");
  // rootCmd.Append("'");
  // outFile << rootCmd.Data() << endl;
  outFile << "root -b << EOF" << endl;
  outFile << "gSystem->AddIncludePath(\"-I$ALICE_ROOT/include -I$ALICE_PHYSICS/include\");" << std::endl;
  outFile << ".L AliTaskSubmitter.cxx+" << endl;
  outFile << "AliTaskSubmitter sub;" << std::endl;
  outFile << "sub.SetIsPodMachine(true);" << std::endl;
  outFile << "sub.Run(" << runMode << ",\"" << inputName << "\",\"" << inputOptions << "\",\"" << analysisOptions << "\",\"" << taskOptions << "\",\"\"," << isMuonAnalysis << ")" << std::endl;
  outFile << ".q" << std::endl;
  outFile << "EOF" << std::endl;
  // if ( fProofSplitPerRun ) {
  //   outFile << "cd $TASKDIR" << endl;
  //   outFile << "done < " << fDatasetName.Data() << endl;
  //   outFile << "outNames=$(find $PWD/*/ -type f -name \"*.root\" -exec basename {} \\; | sort -u | xargs)" << endl;
  //   outFile << "for ifile in $outNames; do" << endl;
  //   TString mergeList = "mergeList.txt";
  //   outFile << "  find $PWD/*/ -name \"$ifile\" > " << mergeList.Data() << endl;
  //   outFile << "  root -b -q $ALICE_PHYSICS/PWGPP/MUON/lite/mergeGridFiles.C\\(\\\"$ifile\\\",\\\"" << mergeList.Data() << "\\\",\\\"\\\"\\)" << endl;
  //   outFile << "  rm " << mergeList.Data() << endl;
  //   outFile << "done" << endl;
  // }
  // outFile << "root -b <<EOF" << endl;
  // outFile << rootCmd.Data() << endl;
  // outFile << ".q" << endl;
  // outFile << "EOF" << endl;
  outFile << "vafctl stop" << std::endl;
  outFile << "exit" << std::endl;
  outFile.close();
  gSystem->Exec(Form("chmod u+x %s",outFilename.c_str()));
}
