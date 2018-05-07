//
//  SingleOscillatorVoice.cpp
//  modularSynth
//
//  Created by Ryan Challinor on 12/4/13.
//
//

#include "SingleOscillatorVoice.h"
#include "EnvOscillator.h"
#include "SynthGlobals.h"
#include "Scale.h"
#include "Profiler.h"

SingleOscillatorVoice::SingleOscillatorVoice(IDrawableModule* owner)
: mStartTime(-1)
, mUseFilter(false)
, mOwner(owner)
{
}

SingleOscillatorVoice::~SingleOscillatorVoice()
{
}

bool SingleOscillatorVoice::IsDone(double time)
{
   return mAdsr.IsDone(time);
}

bool SingleOscillatorVoice::Process(double time, float* out, int bufferSize)
{
   Profiler profiler("SingleOscillatorVoice");

   if (IsDone(time))
      return false;
   
   for (int u=0; u<mVoiceParams->mUnison && u<kMaxUnison; ++u)
      mOscData[u].mOsc.SetType(mVoiceParams->mOscType);
      
   float syncPhaseInc = GetPhaseInc(mVoiceParams->mSyncFreq);
   for (int pos=0; pos<bufferSize; ++pos)
   {
      if (mOwner)
         mOwner->ComputeSliders(pos);
      
      float adsrVal = mAdsr.Value(time);
      
      float summed = 0;
      for (int u=0; u<mVoiceParams->mUnison && u<kMaxUnison; ++u)
      {
         mOscData[u].mOsc.SetPulseWidth(mVoiceParams->mPulseWidth);
         mOscData[u].mOsc.SetShuffle(mVoiceParams->mShuffle);
         
         float pitch = GetPitch(pos);
         float freq = TheScale->PitchToFreq(pitch) * mVoiceParams->mMult;
         
         float detune = ((mVoiceParams->mDetune - 1) * mOscData[u].mDetuneFactor) + 1;
         float phaseInc = GetPhaseInc(freq * detune);
         
         mOscData[u].mPhase += phaseInc;
         if (mOscData[u].mPhase == INFINITY)
         {
            ofLog() << "Infinite phase. phaseInc:" + ofToString(phaseInc) + " detune:" + ofToString(mVoiceParams->mDetune) + " freq:" + ofToString(freq) + " pitch:" + ofToString(pitch) + " getpitch:" + ofToString(GetPitch(pos));
         }
         while (mOscData[u].mPhase > FTWO_PI*2)
         {
            mOscData[u].mPhase -= FTWO_PI*2;
            mOscData[u].mSyncPhase = 0;
         }
         mOscData[u].mSyncPhase += syncPhaseInc;
         
         float sample;
         float vol = mVoiceParams->mVol * .1f;
         
         if (mVoiceParams->mSync)
            sample = mOscData[u].mOsc.Value(mOscData[u].mSyncPhase) * adsrVal * vol;
         else
            sample = mOscData[u].mOsc.Value(mOscData[u].mPhase + mVoiceParams->mPhaseOffset) * adsrVal * vol;
         
         if (u >= 2)
            sample *= 1 - (mOscData[u].mDetuneFactor * .5f);
         
         summed += sample;
      }
      
      if (mUseFilter)
      {
         float f = mFilterAdsr.Value(time) * mVoiceParams->mFilterCutoff;
         float q = 1;
         mFilter.SetFilterParams(f, q);
         summed = mFilter.Filter(summed);
      }
      
      out[pos] += summed;
      time += gInvSampleRateMs;
   }
   
   return true;
}

void SingleOscillatorVoice::Start(double time, float target)
{
   mAdsr.Start(time, target, mVoiceParams->mAdsr);
   mStartTime = time;
   
   if (mVoiceParams->mFilterCutoff != SINGLEOSCILLATOR_NO_CUTOFF ||
       mVoiceParams->mFilterAdsr.GetA() > 1 ||
       mVoiceParams->mFilterAdsr.GetS() < 1 ||
       mVoiceParams->mFilterAdsr.GetR() > 30)
   {
      mUseFilter = true;
      mFilter.SetFilterType(kFilterType_Lowpass);
      mFilterAdsr = mVoiceParams->mFilterAdsr;
      mFilterAdsr.Start(time,1);
   }
   else
   {
      mUseFilter = false;
   }
   
   mOscData[0].mDetuneFactor = 1;
   mOscData[1].mDetuneFactor = 0;
   for (int u=2; u<kMaxUnison; ++u)
      mOscData[u].mDetuneFactor = ofRandom(-1,1);
}

void SingleOscillatorVoice::Stop(double time)
{
   mAdsr.Stop(time);
}

void SingleOscillatorVoice::ClearVoice()
{
   mAdsr.Clear();
   mFilterAdsr.Clear();
   for (int u=0; u<kMaxUnison; ++u)
   {
      mOscData[u].mPhase = 0;
      mOscData[u].mSyncPhase = 0;
   }
}

void SingleOscillatorVoice::SetVoiceParams(IVoiceParams* params)
{
   mVoiceParams = dynamic_cast<OscillatorVoiceParams*>(params);
}
