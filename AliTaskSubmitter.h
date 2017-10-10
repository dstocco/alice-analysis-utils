#ifndef ALITASKSUBMITTER_H
#define ALITASKSUBMITTER_H

#include <string>
#include <vector>
#include <map>
#include "TMap.h"

class AliAnalysisAlien;
class AliAnalysisTaskCfg;
class TObjString;

class AliTaskSubmitter {
public:
  AliTaskSubmitter();
  virtual ~AliTaskSubmitter();

  enum {
    kLocalTerminate,
    kLocal,
    kGrid,
    kGridTest,
    kGridMerge,
    kGridTerminate,
    kGridLocalTerminate,
    kProofLite,
    kProofSaf,
    kProofSaf2,
    kProofVaf,
    kProofOnPod
  };

  enum {
    kCernVaf, ///< CERN Vaf
    kSAF,     ///< Subatech SAF (v3)
    kSAF2,    ///< Subatech SAF (v2)
  };

  enum {
    kAOD, ///< AOD input
    kESD  ///< ESD input
  };

  bool AddPhysicsSelection () const { return (fHasPhysSelInfo && fRunMode != kAOD); }
  bool AddCentrality () const { return fHasCentralityInfo; };

  TMap* GetMap();

  std::string GetPass ( const char* checkString ) const;
  /// Get pass
  std::string GetPass () const { return fPass; }

  std::string GetPeriod ( const char* checkString ) const;
  /// Get period
  std::string GetPeriod () const { return fPeriod; }

  /// Is MC
  bool IsMC() const { return fIsMC; }

  /// Is AOD
  bool IsAOD() const { return (fFileType == kAOD); }

  bool Run ( int runMode, const char* inputName, const char* inputOptions = "", const char* analysisOptions = "", const char* taskOptions = "", const char* softVersions = "", bool isMuonAnalysis = true );

  // void SetAdditionalFiles ( const char* fileList );
  // Bool_t SetAliPhysicsBuildDir ( const char* aliphysicsBuildDir );
  bool SetInput ( const char* inputName, const char* inputOptions );
  // void SetLibraries ( const char* libraries );
  // // void SetIncludePaths ( const char* includePaths );
  // void SetMacros ( const char* macros );
  // void SetSources ( const char* sources );
  //
  // /// Set local working directory
  // void SetWorkDir ( const char* workDir ) { fWorkDir = workDir; }

  // bool Submit ( int runMode ) const;

  // Bool_t SetMode ( const char* runMode, const char* analysisMode );
  // /// Enable event mixing
  // void SetMixingEvent ( bool mixingEvent ) { fEventMixing = mixingEvent; }
  void SetSoftVersion ( const char* softVersion = "" );

  bool SetupAndRun ( const char* workDir, const char* cfgList, int runMode, const char* inputName, const char* inputOptions = "", const char* analysisOptions = "", const char* taskOptions = "" );
  //
  // /// Set number of workers for proof
  // void SetProofNworkers ( int nWorkers = 88 ) { fProofNworkers = nWorkers; }
  // /// Analyse run by run on proof
  // void SetProofSplitPerRun ( bool splitPerRun ) { fProofSplitPerRun = splitPerRun; }
  // /// Resume proof session (when analysis needs to be run several times, using the previous steps)
  // void SetResumeProofSession ( bool resumeProof = kTRUE ) { fProofResume = resumeProof; }
  // Bool_t SetupAnalysis ( const char*  runMode, const char*  analysisMode,
  //                       const char*  inputName, const char* inputOptions,
  //                       const char*  softVersion = "",
  //                       const char*  analysisOptions = "",
  //                       const char*  libraries = "", const char*  includePaths = "",
  //                       const char*  workDir = "",
  //                       const char* additionalFiles = "",
  //                       bool isMuonAnalysis = kTRUE );
  //
  // void WriteTemplateRunTask ( const char* outputDir = "." ) const;

private:

  void AddObjects ( const char* objname, std::vector<std::string>& objlist );
  bool AddTask ( const char* configFilename );
  bool CopyFile ( const char* inFilename, const char* outFilename = nullptr ) const;

  void CreateAlienHandler();
  std::string GetAbsolutePath ( const char* path ) const;
  std::string GetGridQueryVal ( const char* queryString, const char* keyword ) const;
  std::string GetGridDataDir ( const char* queryString ) const;
  std::string GetGridDataPattern ( const char* queryString ) const;
  std::string GetRunNumber ( const char* checkString ) const;
  bool IsGrid() const { return (fRunMode == kGrid || fRunMode == kGridTest || fRunMode == kGridTerminate || fRunMode == kGridLocalTerminate ); }
  bool Load() const;
  int ReplaceKeywords ( std::string& input ) const;
  int ReplaceKeywords ( TObjString* input ) const;
  void SetKeywords ();
  void SetupHandlers ( const char* analysisOptions, bool isMuonAnalysis );
  bool SetupLocalWorkDir ( const char* cfgList );
  bool SetupProof ();
  bool SetupTasks ();
  void StartAnalysis() const;
  // void WriteAnalysisMacro() const;
  // void WriteLoadLibs() const;
  // void WriteRunScript() const;

  bool fHasCentralityInfo; /// Has centrality information
  bool fHasPhysSelInfo; /// Has physics selection
  bool fIsEmbed; //!<! Is embedded MC
  bool fIsInputFileCollection; //!<! File collection as input
    bool fIsMC; //!<! Is MC
  int fFileType; //!<! File type
  int fRunMode; //!<! Analysis mode
  std::string fGridDataDir; ///< Data dir for grid analysis
  std::string fGridDataPattern; ///< Data pattern for grid analysis
  std::string fPass; //!<! Pass name
  std::string fPeriod; //!<! Period name
  std::string fPodOutDir; //!<! Pod out dir
  std::string fProofCluster; //!<! Proof cluster
  std::string fProofDatasetMode; //!<! Proof dataset mode
  std::string fProofServer; //!<! Proof server
  std::string fProofCopyCommand; //!<! Proof copy command
  std::string fProofOpenCommand; //!<! Proof open command
  std::string fProofExecCommand; //!<! Proof exec command
  std::string fSoftVersion; //!<! Software version for analysis
  std::string fSubmitterDir; //!<! Submitter director
  std::string fTaskOptions; //!<! Task options
  std::string fWorkDir;     //!<! Local working directory
  std::vector<std::string> fAdditionalFiles; //!<! Additional files to be copied
  std::vector<std::string> fInputData; //!<! Input data list
  std::vector<std::string> fLibraries; //!<! Libraries
  std::vector<std::string> fMacros; //!<! Macros
  std::vector<std::string> fPackages; //!<! List of PAR files
  std::vector<std::string> fSources; //!<! Analysis sources (cxx)
  std::vector<std::string> fUtilityMacros; //!<! Utility macros
  std::vector<AliAnalysisTaskCfg> fTasks; //!<! Analysis tasks
  std::map<std::string,std::string> fKeywords; //!<! List of keywords
  TMap fMap; //!<! Map of values to be passed to macros (for backward compatibility)
  AliAnalysisAlien* fPlugin; //!<! Analysis plugin


  // Bool_t AddPhysicsSelection ();
  // Bool_t AddCentrality ( Bool_t oldFramework = kFALSE );
  //
  // Bool_t ConnectToPod () const;
  // Bool_t CopyDatasetLocally ();
  // Bool_t CopyFile ( const char* filename ) const;
  // Bool_t CopyPodOutput () const;
  // TObject* CreateInputObject () const;
  // TString GetAbsolutePath ( const char* path ) const;
  // TString GetRunMacro () const;
  //
  // /// Run only local terminare
  // Bool_t IsTerminateOnly () const { return ( fRunMode == kTerminateMode ) && (fAnalysisMode != kGridAnalysis); }
  //
  // Bool_t LoadLibsLocally () const;
  // Bool_t LoadLibsProof ( ) const;
  // Bool_t LoadMacro ( const char* macroName ) const;
  //
  // Bool_t PerformAction ( TString command, Bool_t& yesToAll ) const;
  // Bool_t SetupLocalWorkDir ( TString workDir );
  //
  // void SetTaskDir ();
  //
  // void UnloadMacros() const;
  //
  // void WritePodExecutable () const;

  // Bool_t fIsInitOk; ///< Initialization succesfull
  // Int_t fRunMode; ///< Run mode
  // TString fRunModeName; ///< Run mode name
  // Int_t fAnalysisMode; ///< Analysis mode
  // TString fAnalysisModeName; ///< Analysis mode name
  // Int_t fAAF; ///< Analysis facility chosen
  // Bool_t fIsPod; ///< AAF on POD
  // Bool_t fIsPodMachine; ///< We are on pod machine
  // Int_t fFileType; ///< AOD or ESD
  // Bool_t fLoadAllBranches; ///< Load all branches in ESDs
  // Bool_t fEventMixing; ///< Use event mixing
  // TString fTaskDir; ///< Base directory of tasks
  // TString fCurrentDir; ///< Current directory
  // TString fSoftVersion; ///< Software version
  // TString fWorkDir; ///< Working diretory
  // TString fAliPhysicsBuildDir; ///< Aliphysics build dir
  // TString fDatasetName; ///< Local dataset name
  // Int_t fProofNworkers; ///< Proof N workers
  // Bool_t fProofResume; ///< Resume proof session
  // Bool_t fProofSplitPerRun; ///< Split analysis per run
  // std::vector<std::string> fLibraries; ///< List of libraries and classes to load
  // std::vector<std::string> fIncludePaths; ///< List of include paths
  // std::vector<std::string> fSearchPaths; ///< List of include paths
  // std::vector<std::string> fUtilityMacros; ///< List of utility macros
  // std::vector<std::string> fAdditionalFiles; ///< List of additional files
  // std::vector<Int_t> fRunList; ///< Run list
  // TMap fMap; ///< Map of values to be passed to macros (for backward compatibility)
  //
  // ClassDef(AliTaskSubmitter, 1); // Task submitter
};
#endif
