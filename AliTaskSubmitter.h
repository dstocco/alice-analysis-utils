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
    kProofLite,
    kProofSaf,
    kProofSaf2,
    kProofVaf
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

  /// Set Alien username (needed to connect to some proof clusters)
  bool SetAlienUsername ( const char* username = nullptr );
  /// Set the grid working directory
  void SetGridWorkingDir ( const char* gridWorkingDir ) { fGridWorkingDir = gridWorkingDir; }
  // void SetAdditionalFiles ( const char* fileList );
  void SetGridNtestFiles( int nTestFiles ) { fGridTestFiles = nTestFiles; }
  bool SetAliPhysicsBuildDir ( const char* aliphysicsBuildDir = nullptr );
  bool SetInput ( const char* inputName, const char* inputOptions );
  void SetIsPodMachine ( bool isPodMachine = true ) { fIsPodMachine = isPodMachine; }

  /// Set number of workers for proof
  void SetProofNworkers ( int nWorkers ) { fProofNworkers = nWorkers; }
  /// Analyse run by run on proof
  void SetProofSplitPerRun ( bool splitPerRun ) { fProofSplitPerRun = splitPerRun; }
  /// Resume proof session (when analysis needs to be run several times, using the previous steps)
  void SetResumeProofSession ( bool resumeProof = true ) { fProofResume = resumeProof; }

  // /// Enable event mixing
  // void SetMixingEvent ( bool mixingEvent ) { fEventMixing = mixingEvent; }
  void SetSoftVersion ( const char* softVersion = "" );

  bool SetupAndRun ( const char* workDir, const char* cfgList, int runMode, const char* inputName, const char* inputOptions = "", const char* analysisOptions = "", const char* taskOptions = "" );

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
  bool IsGrid() const { return (fRunMode == kGrid || fRunMode == kGridTest || fRunMode == kGridMerge || fRunMode == kGridTerminate ); }
  bool IsPod() const { return ( ! fProofCopyCommand.empty() ); }
  bool Load() const;
  bool LoadProof() const;
  int ReplaceKeywords ( std::string& input ) const;
  int ReplaceKeywords ( TObjString* input ) const;
  bool RunPod() const;
  void SetKeywords ();
  void SetupHandlers ( const char* analysisOptions, bool isMuonAnalysis );
  bool SetupLocalWorkDir ( const char* cfgList );
  bool SetupProof ( const char* analysisOptions );
  bool SetupTasks ();
  void StartAnalysis() const;
  // void WriteAnalysisMacro() const;
  // void WriteLoadLibs() const;
  void WriteRunScript ( int runMode, const char* inputOptions, const char* analysisOptions, const char* taskOptions, bool isMuonAnalysis ) const;

  bool fHasCentralityInfo; //!<! Has centrality information
  bool fHasPhysSelInfo; //!<! Has physics selection
  bool fIsEmbed; //!<! Is embedded MC
  bool fIsInputFileCollection; //!<! File collection as input
  bool fIsMC; //!<! Is MC
  bool fIsPodMachine; //!<! We are on pod machine
  bool fProofResume; //!<! Resume proof session
  bool fProofSplitPerRun; //!<! Split analysis per run
  int fFileType; //!<! File type
  int fProofNworkers; //!<! Proof N workers
  int fRunMode; //!<! Analysis mode
  int fGridTestFiles; //!<! Number of test files for grid
  std::string fAlienUsername; //!<! Alien username
  std::string fAliPhysicsBuildDir; //!<! Aliphysics build dir
  std::string fGridDataDir; //!<! Data dir for grid analysis
  std::string fGridDataPattern; //!<! Data pattern for grid analysis
  std::string fGridWorkingDir; //!<! Grid working directory
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
  mutable std::vector<AliAnalysisTaskCfg> fTasks; //!<! Analysis tasks
  std::map<std::string,std::string> fKeywords; //!<! List of keywords
  std::map<std::string,int> fUtilityMacros; //!<! Utility macros
  TMap fMap; //!<! Map of values to be passed to macros (for backward compatibility)
  AliAnalysisAlien* fPlugin; //!<! Analysis plugin

  // ClassDef(AliTaskSubmitter, 1); // Task submitter
};
#endif
