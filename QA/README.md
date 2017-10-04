# QA utilities
This folder contains a series of utilities with the aim of helping the user in producing the QA output.

---
## Utilities to make the QA presentation
The file **getFilesAndMakeSlides.sh** is created in order to further simplify the creation of the QA presentation.
It collects the QA files from the output QA repositories and run the MakeSlides.C.

The script recreate locally the directory structure of the QA website, so it is advised to chose a directory (which we will call _yourChosenDir_ in the following) and always run the commands inside it.

An example of usage is:
```bash
cd yourChosenDir
/pathTo/alice-analysis-utils/QA/getFilesAndMakeSlides.sh data/2017/LHC17l/muon_calo_pass1
```

The code will download the QA files for _data/2017/LHC17l/muon_calo_pass1_ and make the slides.
The operation has a certain degree of interactivity. In particular the script:
- it prompts for a comma-separated list of triggers to be displayed
- opens the e-logbook with the proper runs selection in the browser as well as an editor to create and edit a _runListLogbook.txt_ file. The user should export the list of runs in the logbook and copy it in the editor. Once this is done, one should type "y" in the terminal so that the execution continues.
- opens a latex editor so that the user can modify the muonQA.tex file writing the observations.

#### Important
If you re-run the script, the muonQA.tex is **not** deleted. Instead, a backup copy is created: muonQA.tex.backup.
The summary part of this backup copy (which is typically the one modified by the user) is then re-copied back into the new muonQA.tex so that you do not lose your modifications. If something goes wrong in the process, you can still recover your modifications from the muonQA.tex.backup

### At the end of the process
When you have performed the QA and correctly updated the run-by-run summary table in the muonQA.tex file, you can easily retreive the list of good runs with the script **getListOfGoodRuns.sh**.
This is an example:
```bash
/pathTo/alice-analysis-utils/QA/getListOfGoodRuns.sh data/2017/LHC17l/muon_calo_pass1/muonQA.tex
```
The script will parse the muonQA.tex in order to build the list of selected runs.
To do so, it removes all runs declared as bad (using the errorColor or badForPassColor).
If the run has some other "warningColor", the script asks if you want to select the run or not.
At the end, it opens a browser with the list of selected runs, so that one can easily check the statistics.
