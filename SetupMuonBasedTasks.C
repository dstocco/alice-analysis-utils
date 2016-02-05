#if !defined(__CINT__) || defined(__MAKECINT__)
#include <Riostream.h>

// ROOT includes
#include "TString.h"
#include "TObjString.h"
#include "TObjArray.h"
#include "TSystem.h"
#include "TMap.h"

// ANALYSIS includes
#include "AliAnalysisManager.h"

// PWG includes
#include "AliVAnalysisMuon.h"
#include "AliMuonEventCuts.h"
#include "AliMuonTrackCuts.h"

#endif

//_______________________________________
void PrintNames ( TString header, TObjArray* array )
{
  printf("%s:",header.Data());
  if ( ! array ) {
    printf(" NOT SET!!\n");
    return;
  }
  else printf("\n");
  for ( Int_t iarr=0; iarr<array->GetEntries(); iarr++ ) {
    printf(" %i => \"%s\"\n", iarr, array->At(iarr)->GetName());
  }
}

//_______________________________________
TObjArray* GetTerminateOptions ( TString taskOpt, Bool_t isMC )
{
  TObjArray* terminateList = 0x0;
  if ( ! taskOpt.IsNull() ) {
    TObjArray* optList = taskOpt.Tokenize(";");
    for ( Int_t iopt=0; iopt<optList->GetEntries(); iopt++ ) {
      TString currOpt = (static_cast<TObjString*>(optList->At(iopt)))->GetString();
      if ( ! currOpt.Contains("@") ) continue;
      TObjArray* tmpList = currOpt.Tokenize("@");
      if ( tmpList->GetEntries() == 4 ) {
        terminateList = tmpList;
        break;
      }
      delete tmpList;
    }
    delete optList;
  }
  else {
    terminateList = new TObjArray(4);
    terminateList->SetOwner();
    TString physSel = "", trigClasses = "", centr = "", furtherOpt = "";
    if ( isMC ) {
      physSel = "PhysSelPass,PhysSelReject";
      trigClasses = "ANY";
      centr = "-5_105";
      furtherOpt = "MC verbose";
    }
    //TString GetPeriod(opt);
    terminateList->AddAt(new TObjString(physSel),0);
    terminateList->AddAt(new TObjString(trigClasses),1);
    terminateList->AddAt(new TObjString(centr),2);
    terminateList->AddAt(new TObjString(furtherOpt),3);
  }
  
//  if ( terminateList ) {
//    printf("Printing terminate list\n"); // REMEMBER TO CUT
//    PrintNames("Terminate options", terminateList);
//    printf(" ***************:\n");
//  }
  
  return terminateList;
}

//_______________________________________
Bool_t SetMuonEventCuts ( AliMuonEventCuts* to, AliMuonEventCuts* from )
{
  if ( ! from ) return kFALSE;
  (*to) = (*from);
  return kTRUE;
}

//_______________________________________
Bool_t SetMuonTrackCuts ( AliMuonTrackCuts* trackCuts )
{
  if ( ! trackCuts ) return kFALSE;

  trackCuts->SetAllowDefaultParams();
  printf("Allowing default parameters\n");

  return kTRUE;
}

//_______________________________________
Bool_t SetMuonTerminate ( AliVAnalysisMuon* muonTask, Bool_t isMC, TString taskOptions )
{
  TObjArray* terminateList = GetTerminateOptions(taskOptions,isMC);
  
  if ( !  terminateList ) return kFALSE;
  
  muonTask->SetTerminateOptions(terminateList->At(0)->GetName(),terminateList->At(1)->GetName(),terminateList->At(2)->GetName(),terminateList->At(3)->GetName());
  
  return kTRUE;
}

//_______________________________________
Bool_t SetupMuonBasedTask ( AliAnalysisTask* task, AliMuonEventCuts* eventCuts, TString taskOptions, TMap* map )
{
  if ( ! task->IsA()->InheritsFrom(AliVAnalysisMuon::Class()) ) return kFALSE;
  AliVAnalysisMuon* muonTask = static_cast<AliVAnalysisMuon*>(task);

  TString dataType = map->GetValue("dataType")->GetName();
  Bool_t isMC = ( dataType == "MC" );
  
  SetMuonEventCuts(muonTask->GetMuonEventCuts(),eventCuts);
  SetMuonTrackCuts(muonTask->GetMuonTrackCuts());
  SetMuonTerminate(muonTask,isMC,taskOptions);
  
  return kTRUE;
}

