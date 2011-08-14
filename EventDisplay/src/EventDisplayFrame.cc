#include <TApplication.h>
#include <TAxis3D.h>
#include <TBox.h>
#include <TCanvas.h>
#include <TColor.h>
#include <TGButton.h>
#include <TGComboBox.h>
#include <TGFileDialog.h>
#include <TGIcon.h>
#include <TGLabel.h>
#include <TGListBox.h>
#include <TGTextBuffer.h>
#include <TGTextEntry.h>
#include <TPad.h>
#include <TPolyLine.h>
#include <TROOT.h>
#include <TRootEmbeddedCanvas.h>
#include <TSystem.h>
#include <TText.h>
#include <TTimer.h>
#include <TView3D.h>

#include "TrackColorSelector.h"
#include "ContentSelector.h"
#include "DataInterface.h"
#include "EventDisplayFrame.h"
#include "VirtualShape.h"
#include "dict_classes/EventDisplayPad.h"

#include "TClass.h"
#include "TClassMenuItem.h"

#include <iostream>

namespace mu2e_eventdisplay
{

EventDisplayFrame::EventDisplayFrame(const TGWindow* p, UInt_t w, UInt_t h, fhicl::ParameterSet const &pset) : 
  TGMainFrame(p, w, h),
  _g4ModuleLabel(pset.get<std::string>("g4ModuleLabel","g4run"))
{
  int x,y;
  unsigned int width,height;
  gVirtualX->GetWindowSize(gClient->GetRoot()->GetId(),x,y,width,height);
  MoveResize(20,20,width-30,height-70);

  _timer=new TTimer();
  _timer->SetObject(this);
  _timeCurrent=NAN;
  _clock=NULL;
  _isClosed=false;
  _saveAnim=false;
  for(int i=0; i<30; i++)
  {
    _legendText[i]=NULL;
    _legendBox[i]=NULL;
  }
  for(int i=0; i<30; i++)
  {
    _legendParticleGroup[i]=NULL;
    _legendParticleText[i]=NULL;
    _legendParticleLine[i]=NULL;
  }

  //bare pointers needed since ROOT manages the following object
  TGHorizontalFrame *mainFrame = new TGHorizontalFrame(this,800,400);
  _mainCanvas = new TRootEmbeddedCanvas("EventDisplayCanvas",mainFrame,GetWidth()-270,GetHeight()-170);
  TGVerticalFrame *subFrame = new TGVerticalFrame(mainFrame,300,400);

  mainFrame->AddFrame(_mainCanvas, new TGLayoutHints(kLHintsTop));
  mainFrame->AddFrame(subFrame, new TGLayoutHints(kLHintsTop));
  AddFrame(mainFrame, new TGLayoutHints(kLHintsTop, 2,2,2,2));

  TGLayoutHints *lh0 = new TGLayoutHints(kLHintsTop,0,0,0,0);
  TGLayoutHints *lh1 = new TGLayoutHints(kLHintsTop,2,1,2,2);

  TGLabel *hitLabel  = new TGLabel(subFrame, "Tracker Hits");
  TGComboBox *hitBox = new TGComboBox(subFrame,10);
  hitBox->Associate(this);
  hitBox->Resize(250,20);
  subFrame->AddFrame(hitLabel, lh1);
  subFrame->AddFrame(hitBox, lh1);

  TGLabel *caloHitLabel  = new TGLabel(subFrame, "Calo Hits");
  TGComboBox *caloHitBox = new TGComboBox(subFrame,11);
  caloHitBox->Associate(this);
  caloHitBox->Resize(250,20);
  subFrame->AddFrame(caloHitLabel, lh1);
  subFrame->AddFrame(caloHitBox, lh1);

  TGLabel *trackLabel  = new TGLabel(subFrame, "Tracks");
  TGListBox *trackBox = new TGListBox(subFrame,12);
  trackBox->Associate(this);
  trackBox->Resize(250,60);
  trackBox->SetMultipleSelections(true);
  subFrame->AddFrame(trackLabel, lh1);
  subFrame->AddFrame(trackBox, lh1);

  _contentSelector=new ContentSelector(hitBox, caloHitBox, trackBox, _g4ModuleLabel);

  _unhitButton = new TGCheckButton(subFrame,"Show Unhit Straws",31);
  subFrame->AddFrame(_unhitButton, lh1);
  _unhitButton->Associate(this);

  _unhitCrystalsButton = new TGCheckButton(subFrame,"Show Unhit Crystals",36);
  subFrame->AddFrame(_unhitCrystalsButton, lh1);
  _unhitCrystalsButton->Associate(this);

  _supportStructuresButton = new TGCheckButton(subFrame,"Show Tracker Supports, Calo Vanes, Target",32);
  _supportStructuresButton->SetState(kButtonDown);
  subFrame->AddFrame(_supportStructuresButton, lh1);
  _supportStructuresButton->Associate(this);

  _otherStructuresButton = new TGCheckButton(subFrame,"Show Toy DS, CR Steel Shield",37);
  _otherStructuresButton->SetState(kButtonDown);
  subFrame->AddFrame(_otherStructuresButton, lh1);
  _otherStructuresButton->Associate(this);

  _outsideTracksButton = new TGCheckButton(subFrame,"Adjust View to show all Tracks",33);
  subFrame->AddFrame(_outsideTracksButton, lh1);
  _outsideTracksButton->Associate(this);

  _calorimeterViewButton = new TGCheckButton(subFrame,"Adjust View to show Calorimeter",34);
  _calorimeterViewButton->SetState(kButtonDown);
  subFrame->AddFrame(_calorimeterViewButton, lh1);
  _calorimeterViewButton->Associate(this);

  _targetViewButton = new TGCheckButton(subFrame,"Adjust View to show Target",35);
  _targetViewButton->SetState(kButtonDown);
  subFrame->AddFrame(_targetViewButton, lh1);
  _targetViewButton->Associate(this);

  TGHorizontalFrame *subFrameAnim = new TGHorizontalFrame(subFrame,300,15);
  TGTextButton *animButtonStart   = new TGTextButton(subFrameAnim, "Start Animation", 40);
  TGTextButton *animButtonStop    = new TGTextButton(subFrameAnim, "Stop Animation",41);
  TGTextButton *animButtonReset   = new TGTextButton(subFrameAnim, "Reset",42);
  subFrameAnim->AddFrame(animButtonStart, lh1);
  subFrameAnim->AddFrame(animButtonStop, lh1);
  subFrameAnim->AddFrame(animButtonReset, lh1);
  subFrame->AddFrame(subFrameAnim, lh0);
  animButtonStart->Associate(this);
  animButtonStop->Associate(this);
  animButtonReset->Associate(this);

  TGHorizontalFrame *subFrameAnimTime = new TGHorizontalFrame(subFrame,300,15);
  TGLabel *timeIntervalLabel1  = new TGLabel(subFrameAnimTime, "Time Interval from");
  TGLabel *timeIntervalLabel2  = new TGLabel(subFrameAnimTime, "ns to");
  TGLabel *timeIntervalLabel3  = new TGLabel(subFrameAnimTime, "ns");
  _timeIntervalField1 = new TGTextEntry(subFrameAnimTime, new TGTextBuffer, 45);
  _timeIntervalField2 = new TGTextEntry(subFrameAnimTime, new TGTextBuffer, 46);
  subFrameAnimTime->AddFrame(timeIntervalLabel1, lh1);
  subFrameAnimTime->AddFrame(_timeIntervalField1, lh1);
  subFrameAnimTime->AddFrame(timeIntervalLabel2, lh1);
  subFrameAnimTime->AddFrame(_timeIntervalField2, lh1);
  subFrameAnimTime->AddFrame(timeIntervalLabel3, lh1);
  subFrame->AddFrame(subFrameAnimTime, lh0);
  _timeIntervalField1->Associate(this);
  _timeIntervalField2->Associate(this);
  _timeIntervalField1->SetWidth(50);
  _timeIntervalField2->SetWidth(50);

  _repeatAnimationButton = new TGCheckButton(subFrame,"Repeat Animation",43);
  subFrame->AddFrame(_repeatAnimationButton, lh1);
  _repeatAnimationButton->Associate(this);

  TGHorizontalFrame *subFrameSave = new TGHorizontalFrame(subFrame,300,15);
  TGTextButton *saveButton        = new TGTextButton(subFrameSave, "Save", 50);
  TGTextButton *saveAnimButton    = new TGTextButton(subFrameSave, "Save Animation", 51);
  subFrameSave->AddFrame(saveButton, lh1);
  subFrameSave->AddFrame(saveAnimButton, lh1);
  subFrame->AddFrame(subFrameSave, lh0);
  saveButton->Associate(this);
  saveAnimButton->Associate(this);

  _hitColorButton = new TGCheckButton(subFrame,"Use Hit Colors",60);
  _trackColorButton = new TGCheckButton(subFrame,"Use Track Colors",61);
  _backgroundButton = new TGCheckButton(subFrame,"White Background",62);
  subFrame->AddFrame(_hitColorButton, lh1);
  subFrame->AddFrame(_trackColorButton, lh1);
  subFrame->AddFrame(_backgroundButton, lh1);
  _hitColorButton->Associate(this);
  _trackColorButton->Associate(this);
  _backgroundButton->Associate(this);
  _hitColorButton->SetState(kButtonDown);
  _trackColorButton->SetState(kButtonDown);
  _backgroundButton->SetState(kButtonUp);

  _eventInfo = new TGLabel*[4];
  for(int i=0; i<4; i++)
  {
    _eventInfo[i] = new TGLabel(subFrame, "Place Holder for Event Info");
    _eventInfo[i]->SetTextJustify(kTextLeft);
    subFrame->AddFrame(_eventInfo[i], new TGLayoutHints(kLHintsLeft,2,0,2,1));
  }

  TGHorizontalFrame *footLine   = new TGHorizontalFrame(this,800,100);

//The following lines are a variation of something I found in CaloCellTimeMonitoring.C by Christophe Le Maner.
//Its purpose is to create a TRootEmbeddedCanvas with scrollbars.
  _infoCanvas = new TGCanvas(footLine, GetWidth()-600, GetHeight()-700);
  footLine->AddFrame(_infoCanvas, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY,2,2,0,2));
  TGCompositeFrame *container = new TGCompositeFrame(_infoCanvas->GetViewPort());
  _infoCanvas->SetContainer(container);
  _infoEmbeddedCanvas = new TRootEmbeddedCanvas(0, container, 100, 100);//default size - exact size will be set later
  container->AddFrame(_infoEmbeddedCanvas, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

  TGGroupFrame *zoomangleFrame  = new TGGroupFrame(footLine,"Zoom & Angle");
  TGHorizontalFrame *zoomFrame1 = new TGHorizontalFrame(zoomangleFrame,500,50);
  TGHorizontalFrame *zoomFrame2 = new TGHorizontalFrame(zoomangleFrame,500,50);
  TGHorizontalFrame *angleFrame = new TGHorizontalFrame(zoomangleFrame,500,50);
  TGHorizontalFrame *perspectiveFrame = new TGHorizontalFrame(zoomangleFrame,500,50);
  TGLabel *zoomLabel1  = new TGLabel(zoomFrame1, "minx");
  TGLabel *zoomLabel2  = new TGLabel(zoomFrame1, "mm  miny");
  TGLabel *zoomLabel3  = new TGLabel(zoomFrame1, "mm  minz");
  TGLabel *zoomLabel4  = new TGLabel(zoomFrame1, "mm");
  TGLabel *zoomLabel5  = new TGLabel(zoomFrame2, "maxx");
  TGLabel *zoomLabel6  = new TGLabel(zoomFrame2, "mm  maxy");
  TGLabel *zoomLabel7  = new TGLabel(zoomFrame2, "mm  maxz");
  TGLabel *zoomLabel8  = new TGLabel(zoomFrame2, "mm");
  TGLabel *angleLabel1 = new TGLabel(angleFrame, "phi");
  TGLabel *angleLabel2 = new TGLabel(angleFrame, "°  theta");
  TGLabel *angleLabel3 = new TGLabel(angleFrame, "°  psi");
  TGLabel *angleLabel4 = new TGLabel(angleFrame, "°");
  _minXField = new TGTextEntry(zoomFrame1, new TGTextBuffer, 1501);
  _minYField = new TGTextEntry(zoomFrame1, new TGTextBuffer, 1502);
  _minZField = new TGTextEntry(zoomFrame1, new TGTextBuffer, 1503);
  _maxXField = new TGTextEntry(zoomFrame2, new TGTextBuffer, 1504);
  _maxYField = new TGTextEntry(zoomFrame2, new TGTextBuffer, 1505);
  _maxZField = new TGTextEntry(zoomFrame2, new TGTextBuffer, 1506);
  _phiField   = new TGTextEntry(angleFrame, new TGTextBuffer, 1601);
  _thetaField = new TGTextEntry(angleFrame, new TGTextBuffer, 1602);
  _psiField   = new TGTextEntry(angleFrame, new TGTextBuffer, 1603);
  _perspectiveButton = new TGRadioButton(perspectiveFrame, "perspective", 1700);
  _parallelButton    = new TGRadioButton(perspectiveFrame, "parallel", 1701);
  _minXField->SetWidth(50);
  _minYField->SetWidth(50);
  _minZField->SetWidth(50);
  _maxXField->SetWidth(50);
  _maxYField->SetWidth(50);
  _maxZField->SetWidth(50);
  _phiField->SetWidth(50);
  _thetaField->SetWidth(50);
  _psiField->SetWidth(50);
  TGTextButton *setRangeButton = new TGTextButton(zoomangleFrame, "Set &Range", 1500);
  TGTextButton *setAngleButton = new TGTextButton(zoomangleFrame, "Set &Angle", 1600);
  zoomFrame1->AddFrame(zoomLabel1, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  zoomFrame1->AddFrame(_minXField, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  zoomFrame1->AddFrame(zoomLabel2, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  zoomFrame1->AddFrame(_minYField, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  zoomFrame1->AddFrame(zoomLabel3, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  zoomFrame1->AddFrame(_minZField, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  zoomFrame1->AddFrame(zoomLabel4, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  zoomFrame2->AddFrame(zoomLabel5, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  zoomFrame2->AddFrame(_maxXField, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  zoomFrame2->AddFrame(zoomLabel6, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  zoomFrame2->AddFrame(_maxYField, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  zoomFrame2->AddFrame(zoomLabel7, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  zoomFrame2->AddFrame(_maxZField, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  zoomFrame2->AddFrame(zoomLabel8, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  angleFrame->AddFrame(angleLabel1, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  angleFrame->AddFrame(_phiField, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  angleFrame->AddFrame(angleLabel2, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  angleFrame->AddFrame(_thetaField, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  angleFrame->AddFrame(angleLabel3, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  angleFrame->AddFrame(_psiField, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  angleFrame->AddFrame(angleLabel4, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,1,0,1,0));
  perspectiveFrame->AddFrame(_perspectiveButton, new TGLayoutHints(kLHintsLeft,0,0,0,0));
  perspectiveFrame->AddFrame(_parallelButton, new TGLayoutHints(kLHintsLeft,0,0,0,0));
  zoomangleFrame->AddFrame(zoomFrame1, new TGLayoutHints(kLHintsLeft,0,0,0,0));
  zoomangleFrame->AddFrame(zoomFrame2, new TGLayoutHints(kLHintsLeft,0,0,0,0));
  zoomangleFrame->AddFrame(setRangeButton, new TGLayoutHints(kLHintsLeft,0,0,0,0));
  zoomangleFrame->AddFrame(angleFrame, new TGLayoutHints(kLHintsLeft,0,0,0,0));
  zoomangleFrame->AddFrame(setAngleButton, new TGLayoutHints(kLHintsLeft,0,0,0,0));
  zoomangleFrame->AddFrame(perspectiveFrame, new TGLayoutHints(kLHintsLeft,0,0,0,0));
  footLine->AddFrame(zoomangleFrame, new TGLayoutHints(kLHintsLeft,0,0,0,0));

  _minXField->Associate(this);
  _minYField->Associate(this);
  _minZField->Associate(this);
  _maxXField->Associate(this);
  _maxYField->Associate(this);
  _maxZField->Associate(this);
  _phiField->Associate(this);
  _thetaField->Associate(this);
  _psiField->Associate(this);
  setRangeButton->Associate(this);
  setAngleButton->Associate(this);
  _perspectiveButton->Associate(this);
  _parallelButton->Associate(this);
  _perspectiveButton->SetState(kButtonDown);
  _parallelButton->SetState(kButtonUp);

  TGVerticalFrame *innerFrame1   = new TGVerticalFrame(footLine,100,400);

  TGGroupFrame *optionsFrame     = new TGGroupFrame(innerFrame1,"Options");
  TGHorizontalFrame *filterFrame = new TGHorizontalFrame(optionsFrame,500,50);
  TGHorizontalFrame *jumpFrame   = new TGHorizontalFrame(optionsFrame,500,50);
  TGLabel *minHitLabel      = new TGLabel(filterFrame, "minimum hits");
  TGLabel *eventToFindLabel = new TGLabel(jumpFrame, "jump to event number");
  _minHitField = new TGTextEntry(filterFrame, new TGTextBuffer, 1101);
  _eventToFindField = new TGTextEntry(jumpFrame, new TGTextBuffer, 1103);
  _minHits=0;
  _minHitField->SetWidth(50);
  _minHitField->SetText("0");
  _findEvent=false;
  _eventToFind=0;
  _eventToFindField->SetWidth(50);
  _eventToFindField->SetText("");
  TGTextButton *applyButton = new TGTextButton(filterFrame, "&Apply", 1100);
  TGTextButton *goButton    = new TGTextButton(jumpFrame, "&Go", 1102);
  filterFrame->AddFrame(minHitLabel, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,3,0,3,0));
  filterFrame->AddFrame(_minHitField, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,3,0,3,0));
  filterFrame->AddFrame(applyButton, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,3,0,3,0));
  jumpFrame->AddFrame(eventToFindLabel, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,3,0,3,0));
  jumpFrame->AddFrame(_eventToFindField, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,3,0,3,0));
  jumpFrame->AddFrame(goButton, new TGLayoutHints(kLHintsLeft|kLHintsCenterY,3,0,3,0));
  optionsFrame->AddFrame(filterFrame, new TGLayoutHints(kLHintsLeft,3,0,0,0));
  optionsFrame->AddFrame(jumpFrame, new TGLayoutHints(kLHintsLeft,3,0,0,0));
  innerFrame1->AddFrame(optionsFrame, new TGLayoutHints(kLHintsLeft,0,0,0,0));

  TGHorizontalFrame *navigationFrame = new TGHorizontalFrame(innerFrame1,100,200);
  TGTextButton *quitButton         = new TGTextButton(navigationFrame, "&Quit", 1001);
  TGTextButton *nextButton         = new TGTextButton(navigationFrame, "&Next", 1111);
  navigationFrame->AddFrame(quitButton, new TGLayoutHints(kLHintsLeft,10,0,10,0));
  navigationFrame->AddFrame(nextButton, new TGLayoutHints(kLHintsLeft,10,0,10,0));
  innerFrame1->AddFrame(navigationFrame, new TGLayoutHints(kLHintsLeft,10,0,10,0));

  quitButton->Associate(this);
  nextButton->Associate(this);
  _minHitField->Associate(this);
  _eventToFindField->Associate(this);
  applyButton->Associate(this);
  goButton->Associate(this);

  std::string logoFileName=getenv("MU2E_BASE_RELEASE");
  logoFileName.append("/EventDisplay/src/logo_small.png");
  const TGPicture *logo = gClient->GetPicture(logoFileName.c_str());
  TGIcon *icon = new TGIcon(navigationFrame, logo, 50, 50);
  navigationFrame->AddFrame(icon, new TGLayoutHints(kLHintsLeft,20,0,0,0));

  footLine->AddFrame(innerFrame1, new TGLayoutHints(kLHintsLeft,0,0,0,0));
  AddFrame(footLine, new TGLayoutHints(kLHintsLeft,0,0,0,0));

  MapSubwindows();
  SetWindowName("Mu2e Event Display");
  MapWindow();

  _mainCanvas->GetCanvas()->cd();
  _mainPad = new EventDisplayPad("mainPad","Detector", 0, 0, 1, 1, 5,1,1);
  _mainPad->setEventDisplayFrame(this);
  _mainPad->SetFillColor(1);
  _mainPad->Draw();

  _infoEmbeddedCanvas->GetCanvas()->cd();
  _infoPad = new TPad("infoPad","InfoField", 0, 0, 1, 1, 5,1,1);
  _infoPad->SetFillColor(0);
  _infoPad->Draw();

  for(int i=0; i<20; i++)
  {
    float r,g,b;
    TColor *c;
    TColor::HLS2RGB(i*360/20,.5,.5,r,g,b);
    if(!gROOT->GetColor(i+2000)) c = new TColor(i+2000,r,g,b);
  }

  _mainPad->cd();
  _dataInterface = boost::shared_ptr<DataInterface>(new DataInterface(this));
}

EventDisplayFrame::~EventDisplayFrame()
{
  // TODO
  // delete timer;
  // Cleanup();
}

Bool_t EventDisplayFrame::HandleConfigureNotify(Event_t *event)
{
// This is a modified version of the function from TGFrame.cxx
   if ((event->fWidth != fWidth) || (event->fHeight != fHeight))
   {
      fWidth  = event->fWidth;
      fHeight = event->fHeight;
      _mainCanvas->SetWidth(fWidth-270);
      _mainCanvas->SetHeight(fHeight-170);
      _infoCanvas->SetWidth(fWidth-600);
      _infoCanvas->SetHeight(fHeight-700);
      Layout();
   }
   return kTRUE;
}

void EventDisplayFrame::fillZoomAngleFields()
{
  if(_mainPad->GetView()==NULL) return;
  char c[100];
  double min[3], max[3];
  _mainPad->GetView()->GetRange(min,max);
  sprintf(c,"%.0f",min[0]); _minXField->SetText(c);
  sprintf(c,"%.0f",min[1]); _minYField->SetText(c);
  sprintf(c,"%.0f",min[2]); _minZField->SetText(c);
  sprintf(c,"%.0f",max[0]); _maxXField->SetText(c);
  sprintf(c,"%.0f",max[1]); _maxYField->SetText(c);
  sprintf(c,"%.0f",max[2]); _maxZField->SetText(c);
  sprintf(c,"%.0f",_mainPad->GetView()->GetLongitude()); _phiField->SetText(c);
  sprintf(c,"%.0f",_mainPad->GetView()->GetLatitude()); _thetaField->SetText(c);
  sprintf(c,"%.0f",_mainPad->GetView()->GetPsi()); _psiField->SetText(c);
  if(_mainPad->GetView()->IsPerspective())
  {
    _perspectiveButton->SetState(kButtonDown);
    _parallelButton->SetState(kButtonUp);
  }
  else
  {
    _perspectiveButton->SetState(kButtonUp);
    _parallelButton->SetState(kButtonDown);
  }
}

void EventDisplayFrame::fillGeometry()
{
  _mainPad->cd();
  _dataInterface->fillGeometry();
  DataInterface::spaceminmax m=_dataInterface->getSpaceBoundary(false, true, false);
  _mainPad->GetView()->SetRange(m.minx,m.miny,m.minz,m.maxx,m.maxy,m.maxz);
  _mainPad->GetView()->AdjustScales();
  _mainPad->Modified();
  _mainPad->Update();
}

void EventDisplayFrame::setEvent(const art::Event& event, bool firstLoop)
{
  char eventInfoText[50];
  sprintf(eventInfoText,"Event #: %i",event.id().event());
  _eventInfo[0]->SetText(eventInfoText);
  sprintf(eventInfoText,"Run #: %i",event.id().run());
  _eventInfo[1]->SetText(eventInfoText);
  this->Layout();

  _contentSelector->setAvailableCollections(event);
  if(firstLoop) _contentSelector->firstLoop();
  fillEvent(firstLoop);

  gApplication->Run(true);
}

void EventDisplayFrame::fillEvent(bool firstLoop)
{
  _findEvent=false;
  _mainPad->cd();
  _dataInterface->fillEvent(_contentSelector);
  _dataInterface->useHitColors(_hitColorButton->GetState()==kButtonDown,
                               _backgroundButton->GetState()==kButtonDown);
  _dataInterface->useTrackColors(_contentSelector,
                                 _trackColorButton->GetState()==kButtonDown,
                                 _backgroundButton->GetState()==kButtonDown);
  updateTimeIntervalFields();
  updateHitLegend(_hitColorButton->GetState()==kButtonDown);
  updateTrackLegend(_trackColorButton->GetState()==kButtonDown);

  //set zoom only if "all tracks" option is on, or if it is the first event
  if(_outsideTracksButton->GetState()==kButtonDown || firstLoop)
  {
    DataInterface::spaceminmax m=_dataInterface->getSpaceBoundary(_targetViewButton->GetState()==kButtonDown,
                                                   _calorimeterViewButton->GetState()==kButtonDown,
                                                   _outsideTracksButton->GetState()==kButtonDown);
    _mainPad->GetView()->SetRange(m.minx,m.miny,m.minz,m.maxx,m.maxy,m.maxz);
    _mainPad->GetView()->AdjustScales();
  }

  char eventInfoText[50];
  sprintf(eventInfoText,"Number of tracker hits: %i",_dataInterface->getNumberHits());
  _eventInfo[2]->SetText(eventInfoText);
  sprintf(eventInfoText,"Number of calorimeter hits: %i",_dataInterface->getNumberCrystalHits());
  _eventInfo[3]->SetText(eventInfoText);
  this->Layout();

  drawEverything();
}

void EventDisplayFrame::updateTimeIntervalFields()
{
  double mint, maxt;
  if(_outsideTracksButton->GetState()==kButtonDown)
  {
    mint=_dataInterface->getTracksTimeBoundary().mint;
    maxt=_dataInterface->getTracksTimeBoundary().maxt;
  }
  else
  {
    mint=_dataInterface->getHitsTimeBoundary().mint;
    maxt=_dataInterface->getHitsTimeBoundary().maxt;
  }

  char c[50];
  sprintf(c,"%.0f",mint); _timeIntervalField1->SetText(c);
  sprintf(c,"%.0f",maxt); _timeIntervalField2->SetText(c);
}

void EventDisplayFrame::updateHitLegend(bool draw)
{
  for(int i=0; i<21; i++)
  {
    delete _legendBox[i];
    delete _legendText[i];
    _legendBox[i]=NULL;
    _legendText[i]=NULL;
  }

  if(draw)
  {
    for(int i=0; i<21; i++)
    {
      if(i<20)
      {
        _legendBox[i]=new TBox(0.6,0.557+i*0.02,0.7,0.577+i*0.02);
        _legendBox[i]->SetFillColor(i+2000);
        _legendBox[i]->Draw();
      }
      if(i%4==0)
      {
        double mint=_dataInterface->getHitsTimeBoundary().mint;
        double maxt=_dataInterface->getHitsTimeBoundary().maxt;
        double t=i*(maxt-mint)/20.0+mint;
        char s[50];
        sprintf(s,"%+.3e ns",t);
        _legendText[i]=new TText(0.72,0.54+i*0.02,s);
        _legendText[i]->SetTextColor(kGray);
        _legendText[i]->SetTextSize(0.025);
        _legendText[i]->Draw();
      }
    }
  }
}

void EventDisplayFrame::updateTrackLegend(bool draw)
{
  for(int i=0; i<30; i++)
  {
    delete _legendParticleGroup[i];
    delete _legendParticleLine[i];
    delete _legendParticleText[i];
    _legendParticleGroup[i]=NULL;
    _legendParticleLine[i]=NULL;
    _legendParticleText[i]=NULL;
  }

  if(draw)
  {
    std::vector<ContentSelector::trackInfoStruct> selectedTracks=_contentSelector->getSelectedTrackNames();
    TrackColorSelector colorSelector(&selectedTracks);
    colorSelector.drawTrackLegend(_legendParticleGroup, _legendParticleText, _legendParticleLine);
  }
}

bool EventDisplayFrame::isClosed() const
{
  return _isClosed;
}

bool EventDisplayFrame::getSelectedHitsName(std::string &className,
                                            std::string &moduleLabel,
                                            std::string &productInstanceName) const
{
  return _contentSelector->getSelectedHitsName(className, moduleLabel, productInstanceName);
}

int EventDisplayFrame::getMinimumHits() const
{
  return _minHits;
}

int EventDisplayFrame::getEventToFind(bool &findEvent) const
{
  findEvent=_findEvent;
  return _eventToFind;
}

void EventDisplayFrame::CloseWindow()
{
  _isClosed=true;
  _timer->Stop();
  _timeCurrent=NAN;
  TGMainFrame::CloseWindow();
  gApplication->Terminate();
}

Bool_t EventDisplayFrame::ProcessMessage(Long_t msg, Long_t param1, Long_t param2)
{
  switch (GET_MSG(msg))
  {
    case kC_COMMAND:
      switch (GET_SUBMSG(msg))
      {
        case kCM_BUTTON: if(param1==1001) CloseWindow();
                         if(param1==40) prepareAnimation();
                         if(param1==41)
                         {
                           _timer->Stop();
                           _timeCurrent=NAN;
                           if(_saveAnim) combineAnimFiles();
                         }
                         if(param1==42)
                         {
                           _timer->Stop();
                           _timeCurrent=NAN;
                           drawEverything();
                         }
                         if(param1==1111)
                         {
                           _timer->Stop();
                           _timeCurrent=NAN;
                           gApplication->Terminate();
                         }
                         if(param1==1100)
                         {
                           _minHits=atoi(_minHitField->GetText());
                         }
                         if(param1==1102)
                         {
                           _eventToFind=atoi(_eventToFindField->GetText());
                           _findEvent=true;
                           _timer->Stop();
                           _timeCurrent=NAN;
                           gApplication->Terminate();
                         }
                         if(param1==50 || param1==51)
                         {
                           const char *fileType[]={"Gif files","*.gif",0,0};
                           TGFileInfo fileInfo;
                           fileInfo.fFileTypes = fileType;
                           TGFileDialog *fileDialog;
                           fileDialog=new TGFileDialog(gClient->GetRoot(), gClient->GetRoot(), kFDSave, &fileInfo); //ROOT takes care of deleting this
                           if(!fileInfo.fFilename) break;
                           char f[strlen(fileInfo.fFilename)+5];
                           strcpy(f,fileInfo.fFilename);
                           if(strcmp(f+strlen(f)-4,".gif")!=0) strcat(f,".gif");
                           if(param1==50) _mainPad->SaveAs(f);
                           if(param1==51)
                           {
                             _saveAnim=true;
                             _saveAnimFile.assign(f,strlen(f)-4);
                             prepareAnimation();
                           }
                         }
                         if(param1==1500)
                         {
                           double min[3],max[3];
                           min[0]=atof(_minXField->GetText());
                           min[1]=atof(_minYField->GetText());
                           min[2]=atof(_minZField->GetText());
                           max[0]=atof(_maxXField->GetText());
                           max[1]=atof(_maxYField->GetText());
                           max[2]=atof(_maxZField->GetText());
                           if(min[0]<max[0] && min[1]<max[1] && min[2]<max[2])
                             _mainPad->GetView()->SetRange(min,max);
                           _mainPad->Modified();
                           _mainPad->Update();
                         }
                         if(param1==1600)
                         {
                           double phi=atof(_phiField->GetText());
                           double theta=atof(_thetaField->GetText());
                           double psi=atof(_psiField->GetText());
                           int irep=0;
                           _mainPad->GetView()->SetView(phi,theta,psi,irep);
                           _mainPad->SetPhi(-90-phi);
                           _mainPad->SetTheta(90-theta);
                           _mainPad->Modified();
                           _mainPad->Update();
                         }
                         break;
   case kCM_RADIOBUTTON: if(param1==1700)
                         {
                           _parallelButton->SetState(kButtonUp);
                           _perspectiveButton->SetState(kButtonDown);
                           _mainPad->GetView()->SetPerspective();
                           _mainPad->Modified();
                           _mainPad->Update();
                         }
                         if(param1==1701)
                         {
                           _perspectiveButton->SetState(kButtonUp);
                           _parallelButton->SetState(kButtonDown);
                           _mainPad->GetView()->SetParallel();
                           _mainPad->Modified();
                           _mainPad->Update();
                         }
                         break;
   case kCM_CHECKBUTTON: if(param1==31)
                         {
                           _mainPad->cd();
                           if(_unhitButton->GetState()==kButtonDown)
                           {
                             _dataInterface->makeStrawsVisibleBeforeStart(true);
                           }
                           else
                           {
                             _dataInterface->makeStrawsVisibleBeforeStart(false);
                           }
                           drawEverything();
                         }
                         if(param1==32)
                         {
                           _mainPad->cd();
                           if(_supportStructuresButton->GetState()==kButtonDown)
                           {
                             _dataInterface->makeSupportStructuresVisible(true);
                           }
                           else
                           {
                             _dataInterface->makeSupportStructuresVisible(false);
                           }
                           drawEverything();
                         }
                         if(param1==37)
                         {
                           _mainPad->cd();
                           if(_otherStructuresButton->GetState()==kButtonDown)
                           {
                             _dataInterface->makeOtherStructuresVisible(true);
                           }
                           else
                           {
                             _dataInterface->makeOtherStructuresVisible(false);
                           }
                           drawEverything();
                         }
                         if(param1==36)
                         {
                           _mainPad->cd();
                           if(_unhitCrystalsButton->GetState()==kButtonDown)
                           {
                             _dataInterface->makeCrystalsVisibleBeforeStart(true);
                           }
                           else
                           {
                             _dataInterface->makeCrystalsVisibleBeforeStart(false);
                           }
                           drawEverything();
                         }
                         if(param1==33 || param1==34 || param1==35)
                         {
                           _mainPad->cd();
                           DataInterface::spaceminmax m=_dataInterface->getSpaceBoundary(
                                                 _targetViewButton->GetState()==kButtonDown,
                                                 _calorimeterViewButton->GetState()==kButtonDown,
                                                 _outsideTracksButton->GetState()==kButtonDown);
                           _mainPad->GetView()->SetRange(m.minx,m.miny,m.minz,m.maxx,m.maxy,m.maxz);
                           _mainPad->GetView()->AdjustScales();
                           _mainPad->Modified();
                           _mainPad->Update();
                           updateTimeIntervalFields();
                         }
                         if(param1==60)
                         {
                           _mainPad->cd();
                           _dataInterface->useHitColors(_hitColorButton->GetState()==kButtonDown,
                                                        _backgroundButton->GetState()==kButtonDown);
                           updateHitLegend(_hitColorButton->GetState()==kButtonDown);
                           if(isnan(_timeCurrent)) drawEverything();
                           else drawSituation();
                         }
                         if(param1==61)
                         {
                           _mainPad->cd();
                           _dataInterface->useTrackColors(_contentSelector,
                                                          _trackColorButton->GetState()==kButtonDown,
                                                          _backgroundButton->GetState()==kButtonDown);
                           updateTrackLegend(_trackColorButton->GetState()==kButtonDown);
                           if(isnan(_timeCurrent)) drawEverything();
                           else drawSituation();
                         }
                         if(param1==62)
                         {
                           _mainPad->cd();
                           if(_backgroundButton->GetState()==kButtonDown) _mainPad->SetFillColor(0);
                           else _mainPad->SetFillColor(1);
                           _dataInterface->useHitColors(_hitColorButton->GetState()==kButtonDown,
                                                        _backgroundButton->GetState()==kButtonDown);
                           _dataInterface->useTrackColors(_contentSelector,
                                                          _trackColorButton->GetState()==kButtonDown,
                                                          _backgroundButton->GetState()==kButtonDown);
                           if(isnan(_timeCurrent)) drawEverything();
                           else drawSituation();
                         }
                         break;
  case kCM_COMBOBOX : if(param1==10) fillEvent();
                      if(param1==11) fillEvent();
                      break;
  case kCM_LISTBOX : if(param1==12) fillEvent();
                     break;
      }
      break;
  }
  return kTRUE;
}

void EventDisplayFrame::prepareAnimation()
{
  _timer->Stop();           //needed if an animation is already running
  delete _clock; _clock=NULL;
  _mainPad->cd();
  _dataInterface->startComponents();
  _mainPad->Modified();
  _mainPad->Update();
  _timeStart=atof(_timeIntervalField1->GetText());
  _timeStop=atof(_timeIntervalField2->GetText());

  if(isnan(_timeStart) || isnan(_timeStop) || (_timeStop-_timeStart)<=0.0) return;
  double diff=_timeStop-_timeStart;
  _timeStart-=diff*0.01;
  _timeStop+=diff*0.01;
  _timeCurrent=_timeStart;
  _saveAnimCounter=0;
  _timer->Start(100,kFALSE);
}


Bool_t EventDisplayFrame::HandleTimer(TTimer *)
{
  _timer->Reset();
  _timeCurrent+=(_timeStop-_timeStart)/50;
  drawSituation();
  if(_saveAnim)
  {
    _saveAnimCounter++;
    if(_saveAnimCounter%3==0) //save only every 3rd gif to make final file smaller
    {
      char c[_saveAnimFile.length()+15];
      sprintf(c,"%s_tmp_%04i.gif",_saveAnimFile.c_str(),_saveAnimCounter);
      _mainPad->SaveAs(c);
    }
  }
  if(_timeCurrent>=_timeStop)
  {
    _timer->Stop();
    _timeCurrent=NAN;
    if(_saveAnim) combineAnimFiles();
    if(_repeatAnimationButton->GetState()==kButtonDown) prepareAnimation();
  }
  return kTRUE;
}

void EventDisplayFrame::combineAnimFiles()
{
  _saveAnim=false;
  char c[2*_saveAnimFile.length()+100];
  sprintf(c,"convert -delay 50 -loop 0 %s_tmp_*.gif %s.gif",_saveAnimFile.c_str(),_saveAnimFile.c_str());
  gSystem->Exec(c);
  sprintf(c,"rm -f %s_tmp_*.gif",_saveAnimFile.c_str());
  gSystem->Exec(c);
}

void EventDisplayFrame::drawSituation()
{
  _mainPad->cd();
  _dataInterface->updateComponents(_timeCurrent);
  if(!_clock)
  {
    char timeText[50];
    sprintf(timeText,"%+.4e ns",_timeCurrent);
    _clock = new TText(0.52,-0.9,timeText);
    _clock->SetTextColor(5);
    _clock->Draw("same");
  }
  else
  {
    char timeText[50];
    sprintf(timeText,"%+.4e ns",_timeCurrent);
    _clock->SetTitle(timeText);
  }

  _mainPad->Modified();
  _mainPad->Update();
}

void EventDisplayFrame::drawEverything()
{
  _mainPad->cd();
  delete _clock; _clock=NULL;
  if(TAxis3D::GetPadAxis(_mainPad)==NULL) _mainPad->GetView()->ShowAxis();
  TAxis3D::GetPadAxis(_mainPad)->SetLabelSize(0.025);
  _mainPad->Modified();
  _mainPad->Update();
  _dataInterface->updateComponents(NAN);
  _mainPad->Modified();
  _mainPad->Update();
}

void EventDisplayFrame::showInfo(TObject *o)  //ROOT accepts only bare pointers here
{
  _infoPad->cd();
  _infoPad->Clear();
  unsigned int w,h;
  (dynamic_cast<ComponentInfo*>(o))->getExpectedSize(w,h);
  if(w<_infoCanvas->GetWidth()-20) w=_infoCanvas->GetWidth()-20;
  if(h<_infoCanvas->GetHeight()-20) h=_infoCanvas->GetHeight()-20;
  _infoEmbeddedCanvas->SetWidth(w);
  _infoEmbeddedCanvas->SetHeight(h);
  _infoCanvas->Layout();
  _infoPad->cd();
  _infoPad->Modified();
  _infoPad->Update();
  (dynamic_cast<ComponentInfo*>(o))->showInfo(w,h);
  _infoPad->Modified();
  _infoPad->Update();
  _mainPad->cd();
}

}
