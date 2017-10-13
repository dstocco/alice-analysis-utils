# alice-analysis-utils
The repository consists of a series of utilities with the aim of helping the user in running the analysis on AAFs, checking the logs, producing the QA output, etc.

## Setup repository
```bash
cd chosenDir
git clone https://github.com/dstocco/alice-analysis-utils
```
---
## Utilities to run user analyses
The task **AliTaskSubmitter** contains a series of methods that can ease the way the user launch his/her analysis, allowing to transparently submit it locally, on AAF or on grid.

The main method of the utility is:
```C++
SetupAndRun ( const char* workDir, const char* cfgList, int runMode,
              const char* inputName, const char* inputOptions,
              const char* analysisOptions, const char* taskOptions )
```
which takes care of setting up the analysis manager with the correct handlers, add some general tasks if required (see below), and load the required libraries locally.


#### Arguments explained
- **workDir** : the local directory in which all files needed for analysis will be saved.
Please notice that this is also the directory used in AliAnalysisAlien::SetGridWorkingDir in grid mode. This is compulsory in grid mode, so, if you decide to leave it blank, you need to directly act on the AliEn plugin inside your task.cfg:
```C++
#Module.StartConfig
    AliAnalysisAlien* plugin = static_cast<AliAnalysisAlien*>(AliAnalysisManager::GetAnalysisManager()->GetGridHandler());
    plugin->SetGridWorkingDir("/AliEn/relative/path/to/the/chosen/folder");
    ...
#Module.EndConfig
```
- **cfgList** : comma separated list of configuration files to setup a train. The format is the one of _AliAnalysisTaskCfg_.
- **runMode** can be:
  - _kLocalTerminate_ : execute only the Terminate function
  - _kLocal_ : launch jobs locally
  - _kGrid_ : launch jobs on grid
  - _kGridTest_ : use local pc as a "working node" of the grid
  - _kGridMerge_ : launch merging jobs on grid
  - _kGridTerminate_ : merge run-by-run output on grid, copy the result on the local pc and launch the Terminate of the task
  - _kProofLite_ : run proof locally (proof lite)
  - _kProofSaf_ : run on SAF AAF (only for registered users)
  - _kProofSaf2_ : run on SAF2 AAF (only for registered users)
  - _kProofVaf_ : run on CERN VAF
- **inputName**: (CAVEAT: when local filenames are provided, the absolute path must be used)
  - ESD or AOD filename (in local mode)
  - dataset-like search string, e.g. Find;BasePath=/alice/data/2015/LHC15o/000244918/muon_calo_pass1/AOD/;FileName=AliAOD.Muons.root; (in proof and grid mode)
  - txt filename containing:
     - list of local ESD or AOD files (in local mode)
     - list of dataset-like search strings (in proof and grid mode)
     - root file with a file collection (in proof mode)
- **inputOptions** (optional): it is a space-separated list of keywords. The following are recognized:
  - _MC_ : load MC handler if needed
  - _EMBED_ : embedding production (needs MC handler, but use non MC option for other things, e.g. Physics Selection)
  - _AOD_ or _ESD_ : the runTaskUtilities tries to guess if you're running on ESDs or AODs from the inputs specified in _inputName_, but you can write it explicitly in case it fails
- **softVersion**: in proof and grid mode, specifies the AliPhysics version to use. If it is not specified, the latest available AN tag will be used
- **analysisOptions** (optional): it is a space-separated list of keywords. The following are recognized:
  - _NOPHYSSEL_ : do not add the physics selection task in the list of tasks (it is added by default when running on ESDs)
  - _CENTR_: add the centrality tasks
  - _OLDCENTR_ : add the old centrality task (on ESDs)
  - _MIXED_ : use input handler for event mixing
  - _SPLIT_ : in proof mode, provides an output run-by-run (CAVEAT: not working!)
- **isMuonAnalysis**: it is the default...just keep it ;)

### Example
Let us suppose that you have the task AliAnalysisTaskMyTask in PWG/muon.
You need to add the proper configuration file. Let us call it _singleMu.cfg_ which lookslike this:

```
#Module.Begin         SingleMu
#Module.Libs          PWGmuon
#Module.Deps
#Module.DataTypes     ESD, AOD, MC
#Module.MacroName     $ALICE_PHYSICS/PWG/muon/AddTaskSingleMuonAnalysis.C
# Not used when giving full macro
#Module.MacroArgs     __VAR_ISMC__, ""
#Module.OutputFile    AnalysisResults.root
#Module.TerminateFile
#Module.StartConfig
  // __R_ADDTASK__->GetMuonTrackCuts()->SetAllowDefaultParams(true);
#Module.EndConfig
#
# EOF
#
```
You can then run your code locally with:
```C++
root -l
gSystem->AddIncludePath("-I$ALICE_ROOT/include -I$ALICE_PHYSICS/include"); // This is needed only if you use ROOT5
.L path_to/AliTaskSubmitter.cxx+
AliTaskSubmitter sub;
sub.SetupAndRun(AliTaskSubmitter::kLocal,"testDir","singleMu.cfg","/path_to_local/AliAOD.Muons.root");
```


If you re-run the code and the directory _testDir_ exists, you will be prompted if you want to keep it or to overwrite it.

On the other hand, if you want to re-run the analysis, you can also enter the working directory and run:
```C++
.L path_to/AliTaskSubmitter.cxx+
AliTaskSubmitter sub;
sub.Run(AliTaskSubmitter::kLocal,"/path_to_local/AliAOD.Muons.root");
```
