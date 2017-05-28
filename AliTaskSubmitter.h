#ifndef ALITASKSUBMITTER_H
#define ALITASKSUBMITTER_H

#include "TObject.h"
#include "TString.h"
#include "TMap.h"
#include <string>
#include <vector>

class AliAnalysisAlien;

class AliTaskSubmitter : public TObject {
public:
  AliTaskSubmitter();
  virtual ~AliTaskSubmitter();

  enum
  {
    kTestMode,       // Test mode
    kFullMode,       // Full mode
    kMergeMode,      // Merge mode
    kTerminateMode   // Terminate mode
  };

  enum
  {
    kLocalAnalysis,  // Local analysis
    kGridAnalysis,   // Grid analysis
    kAAFAnalysis,    // Proof analysis
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

  Bool_t Init ( const char* workDir );

  TMap* GetMap();

  TString GetPass ( TString checkString ) const;
  /// Get pass
  TString GetPass () const { return fPass; }
  TString GetPeriod ( TString checkString ) const;
  /// Get period
  TString GetPeriod () const { return fPeriod; }
  TString GetRunNumber ( TString checkString ) const;

  /// Has centrality
  Bool_t HasCentralityInfo () const { return fHasCentralityInfo; }

  /// Has physics selection
  Bool_t HasPhysSelInfo() const { return fHasPhysSelInfo; }

  /// Is MC
  Bool_t IsMC () const { return fIsMC; }

  /// Is embedded MC
  Bool_t IsEmbed () const { return fIsEmbed; }

  void SetAdditionalFiles ( const char* fileList );
  Bool_t SetAliPhysicsBuildDir ( const char* aliphysicsBuildDir );
  Bool_t SetInput ( const char* inputName, const char* inputOptions );
  void SetLibraries ( const char* libraries, const char* includePaths );
  Bool_t SetMode ( const char* runMode, const char* analysisMode );
  /// Enable event mixing
  void SetMixingEvent ( Bool_t mixingEvent ) { fEventMixing = mixingEvent; }
  void SetSoftVersion ( TString softVersion );

  /// Set number of workers for proof
  void SetProofNworkers ( Int_t nWorkers = 88 ) { fProofNworkers = nWorkers; }
  /// Analyse run by run on proof
  void SetProofSplitPerRun ( Bool_t splitPerRun ) { fProofSplitPerRun = splitPerRun; }
  /// Resume proof session (when analysis needs to be run several times, using the previous steps)
  void SetResumeProofSession ( Bool_t resumeProof = kTRUE ) { fProofResume = resumeProof; }
  Bool_t SetupAnalysis ( const char*  runMode, const char*  analysisMode,
                        const char*  inputName, const char* inputOptions,
                        const char*  softVersion = "",
                        const char*  analysisOptions = "",
                        const char*  libraries = "", const char*  includePaths = "",
                        const char*  workDir = "",
                        const char* additionalFiles = "",
                        Bool_t isMuonAnalysis = kTRUE );
  void StartAnalysis() const;

  void WriteTemplateRunTask ( const char* outputDir = "." ) const;

private:

  Bool_t AddPhysicsSelection ();
  Bool_t AddCentrality ( Bool_t oldFramework = kFALSE );

  Bool_t ConnectToPod () const;
  Bool_t CopyDatasetLocally ();
  Bool_t CopyFile ( const char* filename ) const;
  Bool_t CopyPodOutput () const;
  AliAnalysisAlien* CreateAlienHandler () const;
  TObject* CreateInputObject () const;
  TString GetAbsolutePath ( const char* path ) const;
  TString GetGridQueryVal ( TString queryString, TString keyword ) const;
  TString GetGridDataDir ( TString queryString ) const;
  TString GetGridDataPattern ( TString queryString ) const;
  TString GetRunMacro () const;

  /// Run only local terminare
  Bool_t IsTerminateOnly () const { return ( fRunMode == kTerminateMode ) && (fAnalysisMode != kGridAnalysis); }

  Bool_t LoadLibsLocally () const;
  Bool_t LoadLibsProof ( ) const;
  Bool_t LoadMacro ( const char* macroName ) const;

  Bool_t PerformAction ( TString command, Bool_t& yesToAll ) const;
  Bool_t SetupLocalWorkDir ( TString workDir );
  Bool_t SetupProof ( const char* analysisMode );

  void SetTaskDir ();

  void UnloadMacros() const;

  void WritePodExecutable () const;

  Bool_t fIsInitOk; ///< Initialization succesfull
  Int_t fRunMode; ///< Run mode
  TString fRunModeName; ///< Run mode name
  Int_t fAnalysisMode; ///< Analysis mode
  TString fAnalysisModeName; ///< Analysis mode name
  Int_t fAAF; ///< Analysis facility chosen
  Bool_t fIsPod; ///< AAF on POD
  Bool_t fIsPodMachine; ///< We are on pod machine
  Int_t fFileType; ///< AOD or ESD
  Bool_t fIsMC; ///< Is MC
  Bool_t fIsEmbed; ///< Is embedded MC
  Bool_t fHasCentralityInfo; /// Has centrality information
  Bool_t fHasPhysSelInfo; /// Has physics selection
  Bool_t fLoadAllBranches; ///< Load all branches in ESDs
  Bool_t fEventMixing; ///< Use event mixing
  TString fTaskDir; ///< Base directory of tasks
  TString fCurrentDir; ///< Current directory
  TString fInputName; ///< Input name
  TString fPeriod; ///< Period name
  TString fPass; ///< Pass name
  TString fSoftVersion; ///< Software version
  TString fGridDataDir; ///< Data dir for grid analysis
  TString fGridDataPattern; ///< Data pattern for grid analysis
  TString fWorkDir; ///< Working diretory
  TString fAliPhysicsBuildDir; ///< Aliphysics build dir
  TString fProofCluster; ///< Proof cluster
  TString fProofServer; ///< Proof server
  TString fProofCopyCommand; ///< Proof copy command
  TString fProofOpenCommand; ///< Proof open command
  TString fProofExecCommand; ///< Proof exec command
  TString fProofDatasetMode; ///< Proof dataset mode
  TString fDatasetName; ///< Local dataset name
  TString fPodOutDir; ///< Pod out dir
  Int_t fProofNworkers; ///< Proof N workers
  Bool_t fProofResume; ///< Resume proof session
  Bool_t fProofSplitPerRun; ///< Split analysis per run
  std::vector<std::string> fLibraries; ///< List of libraries and classes to load
  std::vector<std::string> fIncludePaths; ///< List of include paths
  std::vector<std::string> fSearchPaths; ///< List of include paths
  std::vector<std::string> fUtilityMacros; ///< List of utility macros
  std::vector<std::string> fAdditionalFiles; ///< List of additional files
  std::vector<Int_t> fRunList; ///< Run list
  TMap fMap; ///< Map of values to be passed to macros (for backward compatibility)

  ClassDef(AliTaskSubmitter, 1); // Task submitter
};
#endif
