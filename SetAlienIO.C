#if !defined(__CINT__) || defined(__MAKECINT__)
#include <Riostream.h>

// ROOT includes
#include "TString.h"
#include "TObjString.h"
#include "TObjArray.h"
#include "TSystem.h"
#include "TRegexp.h"

#include "AliAnalysisAlien.h"

#endif

//_______________________________________
Bool_t IsPathMatching ( TString path, TString matchList )
{
  Bool_t matchAll = kTRUE;
  TObjArray* arr = matchList.Tokenize(" ");
  for ( Int_t iarr=0; iarr<arr->GetEntries(); iarr++ ) {
    TString currStr = static_cast<TObjString*>(arr->At(iarr))->GetString();
//    printf("Path %s  matches %s => %i\n", path.Data(), currStr.Data(), path.Contains(TRegexp(currStr.Data()))); // REMEMBER TO CUT
    if ( ! path.Contains(TRegexp(currStr.Data())) ) {
      matchAll = kFALSE;
      break;
    }
  }
  delete arr;
  return matchAll;
}


//_______________________________________
Bool_t SetAlienIO ( TString& inputOptions, TString period, AliAnalysisAlien* plugin )
{
  TObjArray* optList = inputOptions.Tokenize(" ");
  TString boson = "";
  TString nucleons = "";
  TString alignment = "";
  for ( Int_t iopt=0; iopt<optList->GetEntries(); iopt++ ) {
    TString currStr = static_cast<TObjString*>(optList->At(iopt))->GetString();
    currStr.ToLower();
    if ( currStr.Contains("boson") || currStr.Contains("pwhg") || currStr.Contains("powheg") ) boson = currStr;
    else if ( currStr == "pp" || currStr == "pn" || currStr == "np" || currStr == "nn" ) nucleons = currStr;
    else if ( currStr.BeginsWith("align") ) {
      alignment = currStr;
      alignment.ReplaceAll("alignment","");
      alignment.ReplaceAll("align","");
    }
  }
  
  printf("Input options: %s\n",inputOptions.Data());
  if ( ! nucleons.IsNull() ) printf("Requested nucleons: %s\n", nucleons.Data());
  if ( ! alignment.IsNull() ) printf("Requested alignent: %s\n", alignment.Data());
  if ( ! boson.IsNull() ) printf("Requested boson: %s\n", boson.Data());
  
  delete optList;

  TString workDir = "";
  TString dataType = "";
  TString dataDir = "";
  TString dataPattern = "";
  
  if ( period == "LHC13d" || period == "LHC13e" || period == "LHC13f" ) {
    if ( ! boson.IsNull() ) {
      Bool_t isPowheg = boson.Contains("pwhg") || boson.Contains("powheg");
      Bool_t isNpdfOff = boson.Contains("npdfoff");
      Bool_t isFullAcceptance = boson.Contains("full");
      if ( isPowheg || isNpdfOff || isFullAcceptance ) {
        TString bosonDir = boson.BeginsWith("z") ? "zBoson" : "wBoson";
        if ( isPowheg ) {
          if ( boson.Contains("wplus") ) bosonDir = "wPlus";
          else if ( boson.Contains("wminus") ) bosonDir = "wMinus";
          bosonDir += "PWHG";
        }
        if ( isNpdfOff ) bosonDir += "_nPDFoff";
        if ( isFullAcceptance ) bosonDir += "_full";
        dataDir = Form("/alice/cern.ch/user/d/dstocco/sim/%s/%s/%s/align%s",period.Data(),bosonDir.Data(),nucleons.Data(),alignment.Data());
        workDir = Form("mcAna/%s/%s/%s/align%s",bosonDir.Data(),period.Data(),nucleons.Data(),alignment.Data());
        dataPattern = "AliAOD.Muons.root";
        dataType = "MC";
      }
      else {
        TString matchPattern = Form("/%s/ /%c /%s/ /align[a-z]*%s", period.Data(), boson[0], nucleons.Data(), alignment.Data());
        matchPattern.ToLower();
        TString matchPattern2 = matchPattern;
        if ( matchPattern.Contains("pn") ) matchPattern2.ReplaceAll("pn","np");
        else if ( matchPattern.Contains("np") ) matchPattern2.ReplaceAll("np","pn");
        else matchPattern2 = "";
    
        TString simuList = "$ALIDATA/runLists/pPb5020GeV13/vectorBoson_prod_LHC13def.txt";
        gSystem->ExpandPathName(simuList);
        ifstream inFile(simuList);
        if ( inFile.is_open() ) {
          TString currLine = "";
          while ( ! inFile.eof() ) {
            currLine.ReadLine(inFile);
            TString cutLine = currLine;
            for ( Int_t icut=0; icut<6; icut++ ) {
              cutLine.Remove(0,cutLine.Index("/")+1);
            }
            if ( ! cutLine.BeginsWith("/") ) cutLine.Prepend("/");
            if ( ! cutLine.EndsWith("/") ) cutLine.Append("/");
            if ( ! cutLine.Contains("align") ) cutLine.Append("align0/");
            cutLine.ToLower();
            if ( IsPathMatching(cutLine,matchPattern) ||
                ( ! matchPattern2.IsNull() && IsPathMatching(cutLine,matchPattern2) ) ) {
              dataDir = currLine;
              break;
            }
          }
          inFile.close();
        }
        workDir = Form("mcAna/%cSignal/%s/%s/align%s",boson[0],period.Data(),nucleons.Data(),alignment.Data());
        dataPattern = "AliAOD.Muons.root";
        dataType = "MC";
      }
    } // ! boson.IsNull()
    else if ( inputOptions.Contains("beauty",TString::kIgnoreCase) ) {
      TString matchPattern = "EffpPb2013woCuts/";
      if ( alignment.Atoi() != 0 ) matchPattern += Form("Eff%s/",alignment.Data());
      matchPattern += Form("output/%s",period.Data());
      
      TString simuList = "$ALIDATA/runLists/pPb5020GeV13/beauty_prod_LHC13def.txt";
      gSystem->ExpandPathName(simuList);
      ifstream inFile(simuList);
      if ( inFile.is_open() ) {
        TString currLine = "";
        while ( ! inFile.eof() ) {
          currLine.ReadLine(inFile);
          if ( IsPathMatching(currLine,matchPattern) ) {
            dataDir = currLine;
            break;
          }
        }
        inFile.close();
      }
      workDir = Form("mcAna/beauty/%s/align%s",period.Data(),alignment.Data());
      dataPattern = "AliESDs.root";
      dataType = "MC";
    }
    else if ( inputOptions.Contains("fonll",TString::kIgnoreCase) ) {
      TString matchPattern = Form("/%s/ /B/ /align[a-z]*%s", period.Data(), alignment.Data());
      matchPattern.ToLower();
      
      TString simuList = "$ALIDATA/runLists/pPb5020GeV13/fonll_prod_LHC13def.txt";
      gSystem->ExpandPathName(simuList);
      ifstream inFile(simuList);
      if ( inFile.is_open() ) {
        TString currLine = "";
        while ( ! inFile.eof() ) {
          currLine.ReadLine(inFile);
          TString cutLine = currLine;
          for ( Int_t icut=0; icut<6; icut++ ) {
            cutLine.Remove(0,cutLine.Index("/")+1);
          }
          if ( ! cutLine.BeginsWith("/") ) cutLine.Prepend("/");
          if ( ! cutLine.EndsWith("/") ) cutLine.Append("/");
          if ( ! cutLine.Contains("align") ) cutLine.Append("align0/");
          cutLine.ToLower();
          if ( IsPathMatching(cutLine,matchPattern) ) {
            dataDir = currLine;
            break;
          }
        }
        inFile.close();
      }
      workDir = Form("mcAna/fonll/%s/align%s",period.Data(),alignment.Data());
      dataPattern = "AliAOD.Muons.root";
      dataType = "MC";
    }
  }

  if ( ! workDir.IsNull() ) plugin->SetGridWorkingDir(workDir.Data());
  if ( ! dataPattern.IsNull() ) plugin->SetDataPattern(dataPattern.Data());
  inputOptions += Form(" %s",dataType.Data());

  return kTRUE;
}
