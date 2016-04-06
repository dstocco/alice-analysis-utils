# alice-analysis-utils
The repository consists of a series of utilities with the aim of helping the user in running the analysis on AAFs, checking the logs, producing the QA output, etc.

## Setup repository
```bash
cd chosenDir
git clone https://github.com/dstocco/alice-analysis-utils
```
---
## Utilities to run user analyses
The macro **runTaskUtilities.C** contains a series of methods that can ease the way the user launch his/her analysis, allowing to transparently submit it locally, on AAF or on grid.

The utility provides two main methods:
```C++
TMap* SetupAnalysis ( TString runMode, TString analysisMode,
                      TString inputName,
                      TString inputOptions,
                      TString softVersions,
                      TString analysisOptions,
                      TString libraries,
                      TString includePaths,
                      TString workDir,
                      Bool_t isMuonAnalysis = kTRUE )
```
which takes care of setting up the analysis manager with the correct handlres, add some general tasks if required (see below), and load the required libraries locally. It returns a TMap containing some parsed parameters.

And
```C++
void StartAnalysis ( TString runMode, TString analysisMode,
                     TString inputName, TString inputOptions )
```
which should be called after you've added your analysis task to the manager and actually runs the analysis.

#### Arguments explained
The _runMode_ parameter strictly depends on the _analysisMode_. They will be discussed together in the following.
- **analysisMode** can be:
  - _local_
    - **runMode**:
      - _test_ or _full_ : execute full analysis
      - _terminate_ : launch only AnalysisTaskSE::Terminate
  - _grid_
    - **runMode**:
      - _test_ : use local pc as a "working node" of the grid
      - _full_ : launch jobs on grid
      - _merge_ : launch merging jobs on grid
      - _terminate_ : merge run-by-run output on grid, copy the result on the local pc and launch the Terminate of the task
  - _saf_ or _saf2_ or _vaf_ (proof mode):
    - **runMode**:
      - _test_ : launch analysis locally with prooflite
      - _full_ : launch analysis on proof
      - _terminate_ : launch only the task Terminate (in PoD it first get the output file from the proof machine)
- **inputName**:
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
- **softVersions**: in proof and grid mode, specifies the root/aliroot/aliphysics version to use. Syntax: _aliphysics=version,aliroot=version,root=version_. One can put only one of the three. The order does not matter.
- **analysisOptions** (optional): it is a space-separated list of keywords. The following are recognized:
  - _NOPHYSSEL_ : do not add the physics selection task in the list of tasks (it is added by default when running on ESDs)
  - _CENTR_: add the centrality tasks
  - _OLDCENTR_ : add the old centrality task (on ESDs)
  - _MIXED_ : use input handler for event mixing
  - _SPLIT_ : in proof mode, provides an output run-by-run
- **libraries**: it is space-separated list of libraries/par files/classes needed by your task. Notice that they should be provided in the correct order, following the dependencies. They can be:
  - library of aliphysics, e.g. libPWGmuon.so (notice that the .so is compulsory!)
  - par files, e.g. PWGmuon.par (if the par file is not found, it is automagically created)
  - your local class not in aliphysics, e.g. AliAnalysisTaskMyTask.cxx (no need to specify the corresponding .h, but it must be in the same directory)
  - your local AddMyTask not in aliphysics, e.g. AddMyTask.C: it will be copied on the output dir and loaded
- **includePaths**: the space-separated list of include paths needed by your analysis, e.g. ". $ALICE_ROOT/include $ALICE_PHYSICS/include"
- **workDir**: the directory in which all of the files needed to run the analysis will be copied. If one re-runs the command inside MyTask, the files are not overwritten. This allows to re-run the same analysis later on. Please notice that this is also the directory used in AliAnalysisAlien::SetGridWorkingDir in grid mode. This is compulsory in grid mode, so, if you decide to leave it blank, you need to directly act on the AliEn plugin:
```C++
    TMap* SetupAnalysis ([...]);
    AliAnalysisAlien* plugin = static_cast<AliAnalysisAlien*>(AliAnalysisManager::GetAnalysisManager()->GetGridHandler());
    plugin->SetGridWorkingDir("/AliEn/relative/path/to/the/chosen/folder");
```
- **isMuonAnalysis**: it is the default...just keep it ;)


### Example
Let us suppose that you have the task AliAnalysisTaskMyTask in PWG/muon.
You can easily run your code with the following macro:

```C++
void runTask ( TString runMode, TString analysisMode,
               TString inputName,
               TString inputOptions = "",
               TString softVersions = "",
               TString analysisOptions = "",
               TString taskOptions = "" )
{

  gROOT->LoadMacro(gSystem->ExpandPathName("$TASKDIR/runTaskUtilities.C"));

  SetupAnalysis(runMode,analysisMode,inputName,inputOptions,softVersions,analysisOptions, "libPWGmuon.so MyTaskNotInAliphysics.cxx AddMyTaskNotInAliphysics.C",". $ALICE_ROOT/include $ALICE_PHYSICS/include","MyTask");

// AliAnalysisAlien* plugin = static_cast<AliAnalysisAlien*>(AliAnalysisManager::GetAnalysisManager()->GetGridHandler()); // Uncomment it if you want to configure the plugin...

  Bool_t isMC = IsMC(inputOptions);

  gROOT->LoadMacro("$ALICE_ROOT/PWG/muon/AddMyCommittedTask.C");
  AliAnalysisTaskCommittedMine* task = AddMyCommittedTask(isMC);

  MyTaskNotInAliphysics* taskMy = AddMyTaskNotInAliphysics(isMC);

// AliMuonEventCuts* eventCuts = BuildMuonEventCuts(map); // Pre-configured AliMuonEventCuts
// SetupMuonBasedTask(task,eventCuts,taskOptions,map); // Automatically setup "task" if it derives from AliVAnalysisMuon

  StartAnalysis(runMode,analysisMode,inputName,inputOptions);
}
```
**CAVEAT:** remember to export the TASKDIR, so that it points to alice-analysis-utils

To run it on a local AliAOD.root, just run:
```bash
root -l
.x runTask.C("full","local","pathTo/AliAODs.root","","");
```
