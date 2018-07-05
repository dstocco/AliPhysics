#include "AliAnalysisTaskSigma0Run2.h"

#include "AliAnalysisManager.h"
#include "AliInputEventHandler.h"
#include "AliMCEvent.h"
#include "AliMultSelection.h"
#include "AliPIDResponse.h"

ClassImp(AliAnalysisTaskSigma0Run2)

    //____________________________________________________________________________________________________
    AliAnalysisTaskSigma0Run2::AliAnalysisTaskSigma0Run2()
    : AliAnalysisTaskSE("AliAnalysisTaskSigma0Run2"),
      fAliEventCuts(),
      fInputEvent(nullptr),
      fMCEvent(nullptr),
      fV0Reader(nullptr),
      fV0ReaderName("NoInit"),
      fV0Cuts(nullptr),
      fAntiV0Cuts(nullptr),
      fPhotonV0Cuts(nullptr),
      fSigmaCuts(nullptr),
      fAntiSigmaCuts(nullptr),
      fSigmaPhotonCuts(nullptr),
      fAntiSigmaPhotonCuts(nullptr),
      fIsMC(false),
      fIsHeavyIon(false),
      fIsLightweight(false),
      fV0PercentileMax(100.f),
      fTrigger(AliVEvent::kINT7),
      fLambdaContainer(),
      fAntiLambdaContainer(),
      fPhotonV0Container(),
      fGammaArray(nullptr),
      fOutputContainer(nullptr),
      fQA(nullptr),
      fHistCutQA(nullptr),
      fHistRunNumber(nullptr),
      fHistCutBooking(nullptr),
      fHistCentralityProfileBefore(nullptr),
      fHistCentralityProfileAfter(nullptr),
      fHistCentralityProfileCoarseAfter(nullptr),
      fHistTriggerBefore(nullptr),
      fHistTriggerAfter(nullptr),
      fOutputTree(nullptr) {}

//____________________________________________________________________________________________________
AliAnalysisTaskSigma0Run2::AliAnalysisTaskSigma0Run2(const char *name)
    : AliAnalysisTaskSE(name),
      fAliEventCuts(),
      fInputEvent(nullptr),
      fMCEvent(nullptr),
      fV0Reader(nullptr),
      fV0ReaderName("NoInit"),
      fV0Cuts(nullptr),
      fAntiV0Cuts(nullptr),
      fPhotonV0Cuts(nullptr),
      fSigmaCuts(nullptr),
      fAntiSigmaCuts(nullptr),
      fSigmaPhotonCuts(nullptr),
      fAntiSigmaPhotonCuts(nullptr),
      fIsMC(false),
      fIsHeavyIon(false),
      fIsLightweight(false),
      fV0PercentileMax(100.f),
      fTrigger(AliVEvent::kINT7),
      fLambdaContainer(),
      fAntiLambdaContainer(),
      fPhotonV0Container(),
      fGammaArray(nullptr),
      fOutputContainer(nullptr),
      fQA(nullptr),
      fHistCutQA(nullptr),
      fHistRunNumber(nullptr),
      fHistCutBooking(nullptr),
      fHistCentralityProfileBefore(nullptr),
      fHistCentralityProfileAfter(nullptr),
      fHistCentralityProfileCoarseAfter(nullptr),
      fHistTriggerBefore(nullptr),
      fHistTriggerAfter(nullptr),
      fOutputTree(nullptr) {
  DefineInput(0, TChain::Class());
  DefineOutput(1, TList::Class());
  DefineOutput(2, TList::Class());
}

//____________________________________________________________________________________________________
void AliAnalysisTaskSigma0Run2::UserExec(Option_t * /*option*/) {
  AliVEvent *fInputEvent = InputEvent();
  if (fIsMC) fMCEvent = MCEvent();

  // PREAMBLE - CHECK EVERYTHING IS THERE
  if (!fInputEvent) {
    AliError("No Input event");
    return;
  }

  if (fIsMC && !fMCEvent) {
    AliError("No MC event");
    return;
  }

  if (!fV0Cuts || !fAntiV0Cuts || !fPhotonV0Cuts) {
    AliError("V0 Cuts missing");
    return;
  }

  fV0Reader =
      (AliV0ReaderV1 *)AliAnalysisManager::GetAnalysisManager()->GetTask(
          fV0ReaderName.Data());
  if (!fV0Reader) {
    AliError("No V0 reader");
    return;
  }

  if (!fSigmaCuts || !fAntiSigmaCuts || !fSigmaPhotonCuts ||
      !fAntiSigmaPhotonCuts) {
    AliError("Sigma0 Cuts missing");
    return;
  }

  // EVENT SELECTION
  if (!AcceptEvent(fInputEvent)) return;

  // LAMBDA SELECTION
  fV0Cuts->SelectV0(fInputEvent, fMCEvent, fLambdaContainer);

  // LAMBDA SELECTION
  fAntiV0Cuts->SelectV0(fInputEvent, fMCEvent, fAntiLambdaContainer);

  // PHOTON V0 SELECTION
  fPhotonV0Cuts->SelectV0(fInputEvent, fMCEvent, fPhotonV0Container);

  // PHOTON SELECTION
  fGammaArray = fV0Reader->GetReconstructedGammas();  // Gammas from default Cut
  std::vector<AliSigma0ParticleV0> gammaConvContainer;
  CastToVector(gammaConvContainer, fInputEvent);

  // Sigma0 selection
  fSigmaCuts->SelectPhotonMother(fInputEvent, fMCEvent, gammaConvContainer,
                                 fLambdaContainer);

  // Sigma0 selection
  fAntiSigmaCuts->SelectPhotonMother(fInputEvent, fMCEvent, gammaConvContainer,
                                     fAntiLambdaContainer);

  // Sigma0 selection
  fSigmaPhotonCuts->SelectPhotonMother(fInputEvent, fMCEvent,
                                       fPhotonV0Container, fLambdaContainer);

  // Sigma0 selection
  fAntiSigmaPhotonCuts->SelectPhotonMother(
      fInputEvent, fMCEvent, fPhotonV0Container, fAntiLambdaContainer);
  // flush the data
  PostData(1, fOutputContainer);
}

//____________________________________________________________________________________________________
bool AliAnalysisTaskSigma0Run2::AcceptEvent(AliVEvent *event) {
  fHistRunNumber->Fill(0.f, event->GetRunNumber());
  if (!fIsLightweight) FillTriggerHisto(fHistTriggerBefore);

  fHistCutQA->Fill(0);

  // EVENT SELECTION
  if (!fAliEventCuts.AcceptEvent(event)) return false;
  fHistCutQA->Fill(1);

  if (!fIsLightweight) FillTriggerHisto(fHistTriggerAfter);

  // MULTIPLICITY SELECTION
  if (fTrigger == AliVEvent::kHighMultV0 && fV0PercentileMax < 100.f) {
    Float_t lPercentile = 300;
    AliMultSelection *MultSelection = 0x0;
    MultSelection = (AliMultSelection *)event->FindListObject("MultSelection");
    if (!MultSelection) {
      // If you get this warning (and lPercentiles 300) please check that the
      // AliMultSelectionTask actually ran (before your task)
      AliWarning("AliMultSelection object not found!");
    } else {
      lPercentile = MultSelection->GetMultiplicityPercentile("V0M");
    }
    if (!fIsLightweight) fHistCentralityProfileBefore->Fill(lPercentile);
    if (lPercentile > fV0PercentileMax) return false;
    if (!fIsLightweight) fHistCentralityProfileCoarseAfter->Fill(lPercentile);
    if (!fIsLightweight) fHistCentralityProfileAfter->Fill(lPercentile);
    fHistCutQA->Fill(2);
  }

  bool isConversionEventSelected =
      ((AliConvEventCuts *)fV0Reader->GetEventCuts())
          ->EventIsSelected(event, static_cast<AliMCEvent *>(fMCEvent));
  if (!isConversionEventSelected) return false;

  fHistCutQA->Fill(3);
  return true;
}

//____________________________________________________________________________________________________
void AliAnalysisTaskSigma0Run2::CastToVector(
    std::vector<AliSigma0ParticleV0> &container, const AliVEvent *inputEvent) {
  for (int iGamma = 0; iGamma < fGammaArray->GetEntriesFast(); ++iGamma) {
    auto photonCandidate =
        dynamic_cast<AliAODConversionPhoton *>(fGammaArray->At(iGamma));
    if (!photonCandidate) continue;
    AliSigma0ParticleV0 phot(photonCandidate, inputEvent);
    container.push_back(phot);
  }
}

//____________________________________________________________________________________________________
void AliAnalysisTaskSigma0Run2::UserCreateOutputObjects() {
  if (fOutputContainer != nullptr) {
    delete fOutputContainer;
    fOutputContainer = nullptr;
  }
  if (fOutputContainer == nullptr) {
    fOutputContainer = new TList();
    fOutputContainer->SetOwner(kTRUE);
  }

  fQA = new TList();
  fQA->SetName("EventCuts");
  fQA->SetOwner(true);

  if (fTrigger != AliVEvent::kINT7) {
    fAliEventCuts.SetManualMode();
    if (!fIsHeavyIon) fAliEventCuts.SetupRun2pp();
    fAliEventCuts.fTriggerMask = fTrigger;
    fAliEventCuts.fRequireExactTriggerMask = true;
  }

  fHistRunNumber = new TProfile("fHistRunNumber", ";;Run Number", 1, 0, 1);
  fQA->Add(fHistRunNumber);

  fHistCutQA = new TH1F("fHistCutQA", ";;Entries", 5, 0, 5);
  fHistCutQA->GetXaxis()->SetBinLabel(1, "Event");
  fHistCutQA->GetXaxis()->SetBinLabel(2, "AliEventCuts");
  fHistCutQA->GetXaxis()->SetBinLabel(3, "Multiplicity selection");
  fHistCutQA->GetXaxis()->SetBinLabel(4, "AliConversionCuts");
  fQA->Add(fHistCutQA);

  fHistCutBooking = new TProfile("fHistCutBooking", ";;Cut value", 1, 0, 1);
  fHistCutBooking->GetXaxis()->SetBinLabel(1, "V0 percentile");
  fQA->Add(fHistCutBooking);
  fHistCutBooking->Fill(0.f, fV0PercentileMax);

  if (!fIsLightweight) {
    fAliEventCuts.AddQAplotsToList(fQA);

    fV0Reader =
        (AliV0ReaderV1 *)AliAnalysisManager::GetAnalysisManager()->GetTask(
            fV0ReaderName.Data());
    if (!fV0Reader) {
      AliError("No V0 reader");
      return;
    }

    if (fV0Reader->GetEventCuts() &&
        fV0Reader->GetEventCuts()->GetCutHistograms()) {
      fOutputContainer->Add(fV0Reader->GetEventCuts()->GetCutHistograms());
    }
    if (fV0Reader->GetConversionCuts() &&
        fV0Reader->GetConversionCuts()->GetCutHistograms()) {
      fOutputContainer->Add(fV0Reader->GetConversionCuts()->GetCutHistograms());
    }
    if (fV0Reader->GetProduceV0FindingEfficiency() &&
        fV0Reader->GetV0FindingEfficiencyHistograms()) {
      fOutputContainer->Add(fV0Reader->GetV0FindingEfficiencyHistograms());
    }
    if (fV0Reader->GetProduceImpactParamHistograms()) {
      fOutputContainer->Add(fV0Reader->GetImpactParamHistograms());
    }

    fHistCentralityProfileBefore =
        new TH1F("fHistCentralityProfileBefore", "; V0 percentile (%); Entries",
                 1000, 0, 5);
    fHistCentralityProfileAfter =
        new TH1F("fHistCentralityProfileAfter", "; V0 percentile (%); Entries",
                 1000, 0, 5);
    fHistCentralityProfileCoarseAfter =
        new TH1F("fHistCentralityProfileCoarseAfter",
                 "; V0 percentile (%); Entries", 100, 0, 100);
    fQA->Add(fHistCentralityProfileBefore);
    fQA->Add(fHistCentralityProfileAfter);
    fQA->Add(fHistCentralityProfileCoarseAfter);

    fHistTriggerBefore = new TH1F("fHistTriggerBefore", ";;Entries", 50, 0, 50);
    fHistTriggerBefore->GetXaxis()->LabelsOption("u");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(1, "kMB");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(2, "kINT1");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(3, "kINT7");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(4, "kMUON");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(5, "kHighMult");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(6, "kHighMultSPD");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(7, "kEMC1");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(8, "kCINT5");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(9, "kINT5");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(10, "kCMUS5");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(11, "kMUSPB");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(12, "kINT7inMUON");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(13, "kMuonSingleHighPt7");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(14, "kMUSH7");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(15, "kMUSHPB");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(16, "kMuonLikeLowPt7");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(17, "kMUL7");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(18, "kMuonLikePB");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(19, "kMuonUnlikeLowPt7");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(20, "kMUU7");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(21, "kMuonUnlikePB");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(22, "kEMC7");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(23, "kEMC8");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(24, "kMUS7");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(25, "kMuonSingleLowPt7");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(26, "kPHI1");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(27, "kPHI7");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(28, "kPHI8");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(29, "kPHOSPb");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(30, "kEMCEJE");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(31, "kEMCEGA");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(32, "kHighMultV0");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(33, "kCentral");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(34, "kSemiCentral");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(35, "kDG");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(36, "kDG5");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(37, "kZED");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(38, "kSPI7");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(39, "kSPI");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(40, "kINT8");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(41, "kMuonSingleLowPt8");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(42, "kMuonSingleHighPt8");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(43, "kMuonLikeLowPt8");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(44, "kMuonUnlikeLowPt8");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(45, "kMuonUnlikeLowPt0");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(46, "kUserDefined");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(47, "kTRD");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(48, "kFastOnly");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(49, "kAny");
    fHistTriggerBefore->GetXaxis()->SetBinLabel(50, "kAnyINT");
    fQA->Add(fHistTriggerBefore);

    fHistTriggerAfter = new TH1F("fHistTriggerAfter", ";;Entries", 50, 0, 50);
    fHistTriggerAfter->GetXaxis()->LabelsOption("u");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(1, "kMB");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(2, "kINT1");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(3, "kINT7");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(4, "kMUON");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(5, "kHighMult");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(6, "kHighMultSPD");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(7, "kEMC1");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(8, "kCINT5");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(9, "kINT5");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(10, "kCMUS5");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(11, "kMUSPB");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(12, "kINT7inMUON");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(13, "kMuonSingleHighPt7");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(14, "kMUSH7");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(15, "kMUSHPB");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(16, "kMuonLikeLowPt7");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(17, "kMUL7");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(18, "kMuonLikePB");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(19, "kMuonUnlikeLowPt7");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(20, "kMUU7");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(21, "kMuonUnlikePB");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(22, "kEMC7");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(23, "kEMC8");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(24, "kMUS7");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(25, "kMuonSingleLowPt7");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(26, "kPHI1");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(27, "kPHI7");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(28, "kPHI8");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(29, "kPHOSPb");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(30, "kEMCEJE");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(31, "kEMCEGA");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(32, "kHighMultV0");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(33, "kCentral");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(34, "kSemiCentral");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(35, "kDG");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(36, "kDG5");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(37, "kZED");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(38, "kSPI7");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(39, "kSPI");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(40, "kINT8");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(41, "kMuonSingleLowPt8");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(42, "kMuonSingleHighPt8");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(43, "kMuonLikeLowPt8");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(44, "kMuonUnlikeLowPt8");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(45, "kMuonUnlikeLowPt0");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(46, "kUserDefined");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(47, "kTRD");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(48, "kFastOnly");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(49, "kAny");
    fHistTriggerAfter->GetXaxis()->SetBinLabel(50, "kAnyINT");
    fQA->Add(fHistTriggerAfter);
  }

  fOutputContainer->Add(fQA);

  if (fV0Cuts) fV0Cuts->InitCutHistograms(TString("Lambda"));
  if (fAntiV0Cuts) fAntiV0Cuts->InitCutHistograms(TString("AntiLambda"));
  if (fPhotonV0Cuts) fPhotonV0Cuts->InitCutHistograms(TString("Photon"));
  if (fSigmaCuts) fSigmaCuts->InitCutHistograms(TString("Sigma0"));
  if (fAntiSigmaCuts) fAntiSigmaCuts->InitCutHistograms(TString("AntiSigma0"));
  if (fSigmaPhotonCuts)
    fSigmaPhotonCuts->InitCutHistograms(TString("Sigma0Photon"));
  if (fAntiSigmaPhotonCuts)
    fAntiSigmaPhotonCuts->InitCutHistograms(TString("AntiSigma0Photon"));

  if (fV0Cuts && fV0Cuts->GetCutHistograms()) {
    fOutputContainer->Add(fV0Cuts->GetCutHistograms());
  }

  if (fAntiV0Cuts && fAntiV0Cuts->GetCutHistograms()) {
    fOutputContainer->Add(fAntiV0Cuts->GetCutHistograms());
  }

  if (fPhotonV0Cuts && fPhotonV0Cuts->GetCutHistograms()) {
    fOutputContainer->Add(fPhotonV0Cuts->GetCutHistograms());
  }

  if (fSigmaCuts && fSigmaCuts->GetCutHistograms()) {
    fOutputContainer->Add(fSigmaCuts->GetCutHistograms());
  }

  if (fAntiSigmaCuts && fAntiSigmaCuts->GetCutHistograms()) {
    fOutputContainer->Add(fAntiSigmaCuts->GetCutHistograms());
  }

  if (fSigmaPhotonCuts && fSigmaPhotonCuts->GetCutHistograms()) {
    fOutputContainer->Add(fSigmaPhotonCuts->GetCutHistograms());
  }

  if (fAntiSigmaPhotonCuts && fAntiSigmaPhotonCuts->GetCutHistograms()) {
    fOutputContainer->Add(fAntiSigmaPhotonCuts->GetCutHistograms());
  }

  if (fOutputTree != nullptr) {
    delete fOutputTree;
    fOutputTree = nullptr;
  }
  if (fOutputTree == nullptr) {
    fOutputTree = new TList();
    fOutputTree->SetOwner(kTRUE);
  }

  if (fSigmaCuts && fSigmaCuts->GetSigmaTree()) {
    fOutputTree->Add(fSigmaCuts->GetSigmaTree());
  }

  if (fAntiSigmaCuts && fAntiSigmaCuts->GetSigmaTree()) {
    fOutputTree->Add(fAntiSigmaCuts->GetSigmaTree());
  }

  if (fSigmaPhotonCuts && fSigmaPhotonCuts->GetSigmaTree()) {
    fOutputTree->Add(fSigmaPhotonCuts->GetSigmaTree());
  }

  if (fAntiSigmaPhotonCuts && fAntiSigmaPhotonCuts->GetSigmaTree()) {
    fOutputTree->Add(fAntiSigmaPhotonCuts->GetSigmaTree());
  }

  PostData(1, fOutputContainer);
  PostData(2, fOutputTree);
}

//____________________________________________________________________________________________________
void AliAnalysisTaskSigma0Run2::FillTriggerHisto(TH1F *histo) {
  if (fInputHandler->IsEventSelected() & AliVEvent::kMB) histo->Fill(0);
  if (fInputHandler->IsEventSelected() & AliVEvent::kINT1) histo->Fill(1);
  if (fInputHandler->IsEventSelected() & AliVEvent::kINT7) histo->Fill(2);
  if (fInputHandler->IsEventSelected() & AliVEvent::kMUON) histo->Fill(3);
  if (fInputHandler->IsEventSelected() & AliVEvent::kHighMult) histo->Fill(4);
  if (fInputHandler->IsEventSelected() & AliVEvent::kHighMultSPD)
    histo->Fill(5);
  if (fInputHandler->IsEventSelected() & AliVEvent::kEMC1) histo->Fill(6);
  if (fInputHandler->IsEventSelected() & AliVEvent::kCINT5) histo->Fill(7);
  if (fInputHandler->IsEventSelected() & AliVEvent::kINT5) histo->Fill(8);
  if (fInputHandler->IsEventSelected() & AliVEvent::kCMUS5) histo->Fill(9);
  if (fInputHandler->IsEventSelected() & AliVEvent::kMUSPB) histo->Fill(10);
  if (fInputHandler->IsEventSelected() & AliVEvent::kINT7inMUON)
    histo->Fill(11);
  if (fInputHandler->IsEventSelected() & AliVEvent::kMuonSingleHighPt7)
    histo->Fill(12);
  if (fInputHandler->IsEventSelected() & AliVEvent::kMUSH7) histo->Fill(13);
  if (fInputHandler->IsEventSelected() & AliVEvent::kMUSHPB) histo->Fill(14);
  if (fInputHandler->IsEventSelected() & AliVEvent::kMuonLikeLowPt7)
    histo->Fill(15);
  if (fInputHandler->IsEventSelected() & AliVEvent::kMUL7) histo->Fill(16);
  if (fInputHandler->IsEventSelected() & AliVEvent::kMuonLikePB)
    histo->Fill(17);
  if (fInputHandler->IsEventSelected() & AliVEvent::kMuonUnlikeLowPt7)
    histo->Fill(18);
  if (fInputHandler->IsEventSelected() & AliVEvent::kMUU7) histo->Fill(19);
  if (fInputHandler->IsEventSelected() & AliVEvent::kMuonUnlikePB)
    histo->Fill(20);
  if (fInputHandler->IsEventSelected() & AliVEvent::kEMC7) histo->Fill(21);
  if (fInputHandler->IsEventSelected() & AliVEvent::kEMC8) histo->Fill(22);
  if (fInputHandler->IsEventSelected() & AliVEvent::kMUS7) histo->Fill(23);
  if (fInputHandler->IsEventSelected() & AliVEvent::kMuonSingleLowPt7)
    histo->Fill(24);
  if (fInputHandler->IsEventSelected() & AliVEvent::kPHI1) histo->Fill(25);
  if (fInputHandler->IsEventSelected() & AliVEvent::kPHI7) histo->Fill(26);
  if (fInputHandler->IsEventSelected() & AliVEvent::kPHI8) histo->Fill(27);
  if (fInputHandler->IsEventSelected() & AliVEvent::kPHOSPb) histo->Fill(28);
  if (fInputHandler->IsEventSelected() & AliVEvent::kEMCEJE) histo->Fill(29);
  if (fInputHandler->IsEventSelected() & AliVEvent::kEMCEGA) histo->Fill(30);
  if (fInputHandler->IsEventSelected() & AliVEvent::kHighMultV0)
    histo->Fill(31);
  if (fInputHandler->IsEventSelected() & AliVEvent::kCentral) histo->Fill(32);
  if (fInputHandler->IsEventSelected() & AliVEvent::kSemiCentral)
    histo->Fill(33);
  if (fInputHandler->IsEventSelected() & AliVEvent::kDG) histo->Fill(34);
  if (fInputHandler->IsEventSelected() & AliVEvent::kDG5) histo->Fill(35);
  if (fInputHandler->IsEventSelected() & AliVEvent::kZED) histo->Fill(36);
  if (fInputHandler->IsEventSelected() & AliVEvent::kSPI7) histo->Fill(37);
  if (fInputHandler->IsEventSelected() & AliVEvent::kSPI) histo->Fill(38);
  if (fInputHandler->IsEventSelected() & AliVEvent::kINT8) histo->Fill(39);
  if (fInputHandler->IsEventSelected() & AliVEvent::kMuonSingleLowPt8)
    histo->Fill(40);
  if (fInputHandler->IsEventSelected() & AliVEvent::kMuonSingleHighPt8)
    histo->Fill(41);
  if (fInputHandler->IsEventSelected() & AliVEvent::kMuonLikeLowPt8)
    histo->Fill(42);
  if (fInputHandler->IsEventSelected() & AliVEvent::kMuonUnlikeLowPt8)
    histo->Fill(43);
  if (fInputHandler->IsEventSelected() & AliVEvent::kMuonUnlikeLowPt0)
    histo->Fill(44);
  if (fInputHandler->IsEventSelected() & AliVEvent::kUserDefined)
    histo->Fill(45);
  if (fInputHandler->IsEventSelected() & AliVEvent::kTRD) histo->Fill(46);
  if (fInputHandler->IsEventSelected() & AliVEvent::kFastOnly) histo->Fill(47);
  if (fInputHandler->IsEventSelected() & AliVEvent::kAny) histo->Fill(48);
  if (fInputHandler->IsEventSelected() & AliVEvent::kAnyINT) histo->Fill(49);
}
