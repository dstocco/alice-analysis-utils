#if !defined(__CINT__) || defined(__MAKECINT__)
#include <Riostream.h>

// ROOT includes
#include "TString.h"
#include "TObjString.h"
#include "TObjArray.h"
#include "TSystem.h"
#include "TArrayD.h"
#include "TMap.h"

#include "AliMuonEventCuts.h"

#endif

//_______________________________________
Bool_t SetTriggerInfo ( TString period, Bool_t isMC, AliMuonEventCuts* eventCuts )
{
  TString trigClasses = "kINT7,kMB,kCentral,kSemiCentral,kMUS7:Lpt,kMUSPB:Lpt,kMUSH7:Hpt,kMUU7:Lpt2,kINT8,kMuonSingleLowPt8:Lpt,kMuonSingleHighPt8:Hpt,kMuonUnlikeLowPt8:Lpt2,kMuonUnlikeLowPt0:Lpt2";
//  TString trigLevels = "";
  TString trigInputs = "";
  
  if ( isMC ) {
//    trigClasses = "ANY,CM*,MU*";
//    trigLevels = "CMSNGL:Lpt,MUHigh:Hpt,CMULLO:LptLpt,CMULHI:HptHpt,CMLKLO:LptLpt,CMLKHI:HptHpt,MULow:Lpt,MUHigh:Hpt,MULU:LptLpt,MULL:LptLpt,MUHU:HptHpt,MUHL:HptHpt";
    trigClasses = "ANY,CMSNGL:Lpt,MUHigh:Hpt,CMULLO:Lpt2,CMULHI:Hpt2,CMLKLO:Lpt2,CMLKHI:Hpt2,MULow:Lpt,MUHigh:Hpt,MULU:Lpt2,MULL:Lpt2,MUHU:Hpt2,MUHL:Hpt2";
    trigInputs = "0MSL:5,0MSH:6,0MUL:13,0MUH:14,0MLL:15,0MLH:16";
  }
  else {
//    trigLevels = "MSL:Lpt,MUSL:Lpt,MSH:Hpt,MUSH:Hpt,MUL:LptLpt,MUU:LptLpt,MLL:LptLpt";
    if ( period == "LHC10h" ) {
      trigClasses = "kMB";
      trigInputs = "0MUL:5,0MSL:6,0MLL:7";
    }
    else if ( period == "LHC11d" ) {
//      trigClasses = "CINT7-B-NOPF-ALLNOTRD,CINT7-B-NOPF-ALLNOTRD&0MSL,CINT7-B-NOPF-ALLNOTRD&0MSH,CMUS7-B-NOPF-MUON,CMUS7-B-NOPF-MUON&0MSH,CMUSH7-B-NOPF-MUON";
      trigClasses = "CINT7-B-NOPF-ALLNOTRD,CINT7-B-NOPF-ALLNOTRD&0MSL,CINT7-B-NOPF-ALLNOTRD&0MSH,CMUS7-B-NOPF-MUON:Lpt,CMUS7-B-NOPF-MUON&0MSH:Lpt,CMUSH7-B-NOPF-MUON:Hpt";
      trigInputs = "0MSL:6,0MSH:8";
    }
    else if ( period == "LHC11h" ) {
//      trigClasses = "CPBI1-B-NOPF-ALLNOTRD,CPBI1-B-NOPF-ALLNOTRD&0MSL,CPBI1-B-NOPF-ALLNOTRD&0MSH,CPBI2_B1-B-NOPF-ALLNOTRD,CPBI1MSL-B-NOPF-MUON,CPBI1MSL-B-NOPF-MUON&0MSH,CPBI1MSH-B-NOPF-MUON,CCENT_R2-B-NOPF-ALLNOTRD|CVHN_R2-B-NOPF-ALLNOTRD,CVLN_B2-B-NOPF-ALLNOTRD|CVLN_R1-B-NOPF-ALLNOTRD|CSEMI_R1-B-NOPF-ALLNOTRD";
      trigClasses = "CPBI1-B-NOPF-ALLNOTRD,CPBI1-B-NOPF-ALLNOTRD&0MSL,CPBI1-B-NOPF-ALLNOTRD&0MSH,CPBI2_B1-B-NOPF-ALLNOTRD,CPBI1MSL-B-NOPF-MUON:Lpt,CPBI1MSL-B-NOPF-MUON&0MSH:Lpt,CPBI1MSH-B-NOPF-MUON:Hpt,CCENT_R2-B-NOPF-ALLNOTRD|CVHN_R2-B-NOPF-ALLNOTRD,CVLN_B2-B-NOPF-ALLNOTRD|CVLN_R1-B-NOPF-ALLNOTRD|CSEMI_R1-B-NOPF-ALLNOTRD";
      trigInputs = "0MSL:6,0MSH:8";
    }
    else if ( period == "LHC13d" || period == "LHC13e" || period == "LHC13f" ) {
//      trigClasses = "CINT7-B-NOPF-ALLNOTRD,CINT7-B-NOPF-ALLNOTRD&0MSL,CINT7-B-NOPF-ALLNOTRD&0MSH,CMSL7-B-NOPF-MUON,CMSL7-B-NOPF-MUON&0MSH,CMSH7-B-NOPF-MUON";
      trigClasses = "CINT7-B-NOPF-ALLNOTRD,CINT7-B-NOPF-ALLNOTRD&0MSL,CINT7-B-NOPF-ALLNOTRD&0MSH,CMSL7-B-NOPF-MUON:Lpt,CMSL7-B-NOPF-MUON&0MSH:Lpt,CMSH7-B-NOPF-MUON:Hpt";
      trigInputs = "0MSL:12,0MSH:13,0MUL:14";
    }
    else if ( period == "LHC15o" ) {
      trigInputs = "0MSL:17,0MSH:18,0MLL:19,0MUL:20";
    }
  }
  
  if ( ! trigClasses.IsNull() ) eventCuts->SetTrigClassPatterns(trigClasses,trigInputs);
//  if ( ! trigLevels.IsNull() ) eventCuts->SetTrigClassLevels(trigLevels);

  printf("Trigger class pattern: %s\n", trigClasses.Data());
  printf("Trigger inputs: %s\n", trigInputs.Data());
//  printf("Trigger levels: %s\n", trigLevels.Data());

  if ( trigClasses.IsNull() ) return kFALSE;
    
  return kTRUE;
}


//_______________________________________
Bool_t SetCentralityBins ( Bool_t useCentr, TString period, AliMuonEventCuts* eventCuts )
{
  
  TString sCentrBins = "";
  if ( useCentr == kFALSE ) sCentrBins = "-5.,0.";
  else if ( period == "LHC13d" || period == "LHC13e" || period == "LHC13f" ) {
    sCentrBins = "-5.,0.,2.,5.,20.,40.,60.,80.,100.,105.";
  }
  else return kFALSE;
  
  TArrayD centrBins;
  TObjArray* centrBinsArr = sCentrBins.Tokenize(" ");
  for ( Int_t iarr=0; iarr<centrBinsArr->GetEntries(); iarr++ ) {
    TString currStr = (static_cast<TObjString*>(centrBinsArr->At(iarr)))->GetString();
    if ( currStr.Contains(",") ) {
      TString checkString = currStr;
      checkString.ReplaceAll(",","");
      checkString.ReplaceAll(".","");
      checkString.ReplaceAll("-","");
      if ( checkString.IsDigit() ) {
        TObjArray* arr = currStr.Tokenize(",");
        centrBins.Set(arr->GetEntries());
        for ( Int_t icentr=0; icentr<arr->GetEntries(); icentr++ ) {
          centrBins[icentr] = (static_cast<TObjString*>(arr->At(icentr)))->GetString().Atof();
        }
        delete arr;
        break;
      }
    }
  }
  delete centrBinsArr;
  eventCuts->SetCentralityClasses(centrBins.GetSize()-1,centrBins.GetArray());
  printf("Centrality bins: %s\n",sCentrBins.Data());
  return kTRUE;
}

//_______________________________________
AliMuonEventCuts* BuildMuonEventCuts ( TMap* map )
{
  TString period = map->GetValue("period")->GetName();
  TString dataType = map->GetValue("dataType")->GetName();
  Bool_t isMC = ( dataType == "MC" );
  TString mcDetails = map->GetValue("mcDetails")->GetName();

  AliMuonEventCuts* eventCuts = new AliMuonEventCuts("autoEvtCuts","autoEvtCuts");
  Bool_t mcTrigger = ( isMC && mcDetails != "EMBED" );
  SetTriggerInfo(period,mcTrigger,eventCuts);

  UInt_t filterMask = AliMuonEventCuts::kSelectedTrig;

  TString physSel = map->GetValue("physicsSelection")->GetName();
  if ( physSel == "YES" ) filterMask |= AliMuonEventCuts::kPhysicsSelected;

  TString centr = map->GetValue("centrality")->GetName();
  Bool_t useCentr = kTRUE;
  if ( centr != "NO" ) filterMask |= AliMuonEventCuts::kSelectedCentrality;
  else useCentr = kFALSE;
  SetCentralityBins(useCentr,period,eventCuts);

  eventCuts->SetFilterMask(filterMask);

//  if ( mcTrigger ) {
//    UInt_t filterMask = AliMuonEventCuts::kSelectedCentrality|AliMuonEventCuts::kSelectedTrig;
//    if ( ! mcDetails.Contains("NOVTX") ) filterMask |= AliMuonEventCuts::kGoodVertex;
//    eventCuts->SetFilterMask(filterMask);
//  }

  return eventCuts;
}

