#include "SDT.h"
#include "AudioConfig.h"
#include "DSP_Fn.h"

AudioControlSGTL5000_Extended sgtl5000_1;      // controller for the Teensy Audio Board microphone
AudioConvert_I16toF32 int2Float1, int2Float2;  //Converts Int16 to Float.  See class in AudioStream_F32.h
AudioEffectGain_F32 gain1, gain2;              //Applies digital gain to audio data.  Expected Float data.
AudioConvert_F32toI16 float2Int1, float2Int2;  //Converts Float to Int16.  See class in AudioStream_F32.h

AudioInputI2SQuad i2s_quadIn;
AudioOutputI2SQuad i2s_quadOut;

#ifdef AUDIO_STATS
elapsedMicros usecAudio;
#endif


// for testing processor loading and memory usage of mixers
// each connected mixer adds between 50-250 usec to processor load
// each connected mixer consumes about 77 bytes

// w/ mixers and connections
// Memory Usage on Teensy 4.1:
//   FLASH: code:233396, data:192036, headers:8736   free for files:7692296
//    RAM1: variables:264416, code:184264, padding:12344   free for local variables:63264
//    RAM2: variables:377024  free for malloc/new:147264

// w/o mixers and connections
// Memory Usage on Teensy 4.1:
//   FLASH: code:232548, data:192036, headers:8560   free for files:7693320
//    RAM1: variables:262880, code:183416, padding:13192   free for local variables:64800
//    RAM2: variables:377024  free for malloc/new:147264

/*
AudioMixer4 mixer0;
AudioMixer4 mixer1;
AudioMixer4 mixer2;
AudioMixer4 mixer3;
AudioMixer4 mixer4;
AudioMixer4 mixer5;
AudioMixer4 mixer6;
AudioMixer4 mixer7;
AudioMixer4 mixer8;
AudioMixer4 mixer9;

AudioConnection connect0(i2s_quadIn, 2, mixer0, 0);
AudioConnection connect1(i2s_quadIn, 2, mixer1, 0);
AudioConnection connect2(i2s_quadIn, 2, mixer2, 0);
AudioConnection connect3(i2s_quadIn, 2, mixer3, 0);
AudioConnection connect4(i2s_quadIn, 2, mixer4, 0);
AudioConnection connect5(i2s_quadIn, 2, mixer5, 0);
AudioConnection connect6(i2s_quadIn, 2, mixer6, 0);
AudioConnection connect7(i2s_quadIn, 2, mixer7, 0);
AudioConnection connect8(i2s_quadIn, 2, mixer8, 0);
AudioConnection connect9(i2s_quadIn, 2, mixer9, 0);

AudioMixer4 mixer10;
AudioMixer4 mixer11;
AudioMixer4 mixer12;
AudioMixer4 mixer13;
AudioMixer4 mixer14;
AudioMixer4 mixer15;
AudioMixer4 mixer16;
AudioMixer4 mixer17;
AudioMixer4 mixer18;
AudioMixer4 mixer19;

AudioConnection connect10(i2s_quadIn, 2, mixer10, 0);
AudioConnection connect11(i2s_quadIn, 2, mixer11, 0);
AudioConnection connect12(i2s_quadIn, 2, mixer12, 0);
AudioConnection connect13(i2s_quadIn, 2, mixer13, 0);
AudioConnection connect14(i2s_quadIn, 2, mixer14, 0);
AudioConnection connect15(i2s_quadIn, 2, mixer15, 0);
AudioConnection connect16(i2s_quadIn, 2, mixer16, 0);
AudioConnection connect17(i2s_quadIn, 2, mixer17, 0);
AudioConnection connect18(i2s_quadIn, 2, mixer18, 0);
AudioConnection connect19(i2s_quadIn, 2, mixer19, 0);
*/

#ifdef USE_MIXERS
AudioMixer4 modeSelectInR;
AudioMixer4 modeSelectInL;
#endif

AudioMixer4 modeSelectInExR;
AudioMixer4 modeSelectInExL;

AudioMixer4 modeSelectOutL;
AudioMixer4 modeSelectOutR;
AudioMixer4 modeSelectOutExL;
AudioMixer4 modeSelectOutExR;

AudioRecordQueue Q_in_L;
AudioRecordQueue Q_in_R;
AudioRecordQueue Q_in_L_Ex;
AudioRecordQueue Q_in_R_Ex;

AudioPlayQueue Q_out_L;
AudioPlayQueue Q_out_R;
AudioPlayQueue Q_out_L_Ex;
AudioPlayQueue Q_out_R_Ex;

#ifdef T41_USB_AUDIO
AudioOutputUSB usb1;
AudioAmplifier amp1, amp2;
AudioFilterBiquad biquad1;
#endif

AudioConnection patchCord1(i2s_quadIn, 0, int2Float1, 0);  //connect the Left input to the Left Int->Float converter
AudioConnection patchCord2(i2s_quadIn, 1, int2Float2, 0);  //connect the Right input to the Right Int->Float converter

AudioConnection_F32 patchCord3(int2Float1, 0, comp1, 0);  //Left.  makes Float connections between objects
AudioConnection_F32 patchCord4(int2Float2, 0, comp2, 0);  //Right.  makes Float connections between objects
AudioConnection_F32 patchCord5(comp1, 0, float2Int1, 0);  //Left.  makes Float connections between objects
AudioConnection_F32 patchCord6(comp2, 0, float2Int2, 0);  //Right.  makes Float connections between objects
//AudioConnection_F32     patchCord3(int2Float1, 0, float2Int1, 0); //Left.  makes Float connections between objects
//AudioConnection_F32     patchCord4(int2Float2, 0, float2Int2, 0); //Right.  makes Float connections between objects

AudioConnection patchCord7(float2Int1, 0, modeSelectInExL, 0);  //Input Ex
AudioConnection patchCord8(float2Int2, 0, modeSelectInExR, 0);

#ifdef USE_MIXERS
AudioConnection patchCord9(i2s_quadIn, 2, modeSelectInL, 0);  // Input Rec
AudioConnection patchCord10(i2s_quadIn, 3, modeSelectInR, 0);

AudioConnection patchCord13(modeSelectInR, 0, Q_in_R, 0);  // Rec in Queue
AudioConnection patchCord14(modeSelectInL, 0, Q_in_L, 0);
#else
AudioConnection patchCord9(i2s_quadIn, 2, Q_in_L, 0);  // Input Rec
AudioConnection patchCord10(i2s_quadIn, 3, Q_in_R, 0);
#endif

AudioConnection patchCord11(modeSelectInExR, 0, Q_in_R_Ex, 0);  // Ex in Queue
AudioConnection patchCord12(modeSelectInExL, 0, Q_in_L_Ex, 0);

AudioConnection patchCord15(Q_out_L_Ex, 0, modeSelectOutExL, 0);  //Ex out Queue
AudioConnection patchCord16(Q_out_R_Ex, 0, modeSelectOutExR, 0);

AudioConnection patchCord17(Q_out_L, 0, modeSelectOutL, 0);  //Rec out Queue
AudioConnection patchCord18(Q_out_R, 0, modeSelectOutR, 0);

AudioConnection patchCord19(modeSelectOutExL, 0, i2s_quadOut, 0);  //Ex out
AudioConnection patchCord20(modeSelectOutExR, 0, i2s_quadOut, 1);
AudioConnection patchCord21(modeSelectOutL, 0, i2s_quadOut, 2);  //Rec out
AudioConnection patchCord22(modeSelectOutR, 0, i2s_quadOut, 3);

AudioConnection patchCord23(Q_out_L_Ex, 0, modeSelectOutL, 1);  //Rec out Queue for sidetone
AudioConnection patchCord24(Q_out_R_Ex, 0, modeSelectOutR, 1);

#ifdef T41_USB_AUDIO
AudioConnection patchCord25(Q_out_L, biquad1);
//AudioConnection patchCord25(Q_out_L, amp1);
AudioConnection patchCord26(biquad1, amp1);
AudioConnection patchCord27(amp1, 0, usb1, 0);
AudioConnection patchCord28(Q_out_L, 0, amp2, 0);
AudioConnection patchCord29(amp2, 0, usb1, 1);
#endif

AudioControlSGTL5000 sgtl5000_2;

void AudioSetup() {
  // configure an SGTL5000 control object for input from the audio adapter microphone
  // (this is on Teensy pin 8)
  sgtl5000_1.setAddress(LOW);
  sgtl5000_1.enable();
  AudioMemory(500);
  AudioMemory_F32(10);
  sgtl5000_1.inputSelect(AUDIO_INPUT_MIC);
  sgtl5000_1.micGain(20);
  sgtl5000_1.lineInLevel(0);
  sgtl5000_1.lineOutLevel(20);
  sgtl5000_1.adcHighPassFilterDisable();  //reduces noise.  https://forum.pjrc.com/threads/27215-24-bit-audio-boards?p=78831&viewfull=1#post78831

  // configure a second SGTL5000 control object for input from the Main board ADC
  // this is a PCM1808 not an SGTL5000 so any I2C related configuration functions aren't usable
  // (this is on Teensy pin 6)
  sgtl5000_2.setAddress(HIGH);
  sgtl5000_2.enable();
  sgtl5000_2.inputSelect(AUDIO_INPUT_LINEIN);
  sgtl5000_2.volume(0.5);
}

void AudioStart() {
  Q_in_L.begin();  //Initialize receive input buffers
  Q_in_R.begin();
  Q_out_L.setBehaviour(AudioPlayQueue::NON_STALLING); // NON_STALLING

#ifdef T41_USB_AUDIO
  amp1.gain(100);
  amp2.gain(200);
  //amp2.gain(100);
  //amp2.gain(1);
  //biquad1.setBandpass(0, 1000, 0.5);
  biquad1.setLowpass(0, 3000, 0.5);
#endif

}

void ConfigAudioState() {
  switch(radioState) {
    // *** the end and begin methods are fast, but leave the background interrupt process running ***
    // *** TODO: compare effect of using end/begin and disconnect/connect on CW signal timing ***
    case SSB_RECEIVE_STATE:
      // set up input queues for receive
      Q_in_L_Ex.end();
      Q_in_R_Ex.end();
      Q_in_L.begin();
      Q_in_R.begin();

#ifdef USE_MIXERS
      modeSelectInL.gain(0, 1);
      modeSelectInR.gain(0, 1);
#else
      patchCord9.connect();
      patchCord10.connect();
#endif

      modeSelectInExR.gain(0, 0);
      modeSelectInExL.gain(0, 0);

      modeSelectOutL.gain(0, 1);
      modeSelectOutR.gain(0, 1);
      modeSelectOutL.gain(1, 0);
      modeSelectOutR.gain(1, 0);
      modeSelectOutExL.gain(0, 0);
      modeSelectOutExR.gain(0, 0);
      break;

    case SSB_TRANSMIT_STATE:
      // set up input queues for transmit
      Q_in_L.end();
      Q_in_R.end();
      Q_in_L_Ex.begin();
      Q_in_R_Ex.begin();

      comp1.setPreGain_dB(currentMicGain);
      comp2.setPreGain_dB(currentMicGain);

#ifdef USE_MIXERS
      modeSelectInR.gain(0, 0);
      modeSelectInL.gain(0, 0);
#else
      patchCord9.disconnect();
      patchCord10.disconnect();
#endif

      modeSelectInExR.gain(0, 1);
      modeSelectInExL.gain(0, 1);

      modeSelectOutL.gain(0, 0);
      modeSelectOutR.gain(0, 0);
      modeSelectOutExL.gain(0, powerOutSSB[currentBand]);
      modeSelectOutExR.gain(0, powerOutSSB[currentBand]);
      break;

    case CW_RECEIVE_STATE:
      Q_in_L_Ex.end();
      Q_in_R_Ex.end();

      Q_in_L.begin();
      Q_in_R.begin();

#ifdef USE_MIXERS
      modeSelectInR.gain(0, 1);
      modeSelectInL.gain(0, 1);
#else
      patchCord9.connect();
      patchCord10.connect();
#endif

      modeSelectInExR.gain(0, 0);
      modeSelectInExL.gain(0, 0);

      modeSelectOutL.gain(0, 1);
      modeSelectOutR.gain(0, 1);
      modeSelectOutL.gain(1, 0);
      modeSelectOutR.gain(1, 0);
      modeSelectOutExL.gain(0, 0);
      modeSelectOutExR.gain(0, 0);
      break;

    case CW_TRANSMIT_STRAIGHT_STATE:
      // stop collecting input I/Q data
      Q_in_L.end();
      Q_in_R.end();

#ifdef USE_MIXERS
      modeSelectInR.gain(0, 0);
      modeSelectInL.gain(0, 0);
#else
      patchCord9.disconnect();
      patchCord10.disconnect();
#endif

      modeSelectInExR.gain(0, 0);

      modeSelectInExL.gain(0, 0); // ??? missing in original

      modeSelectOutL.gain(0, 0);
      modeSelectOutR.gain(0, 0);
      modeSelectOutExL.gain(0, 0);
      modeSelectOutExR.gain(0, 0);
      break;

    case CW_TRANSMIT_KEYER_STATE:
      // stop collecting input I/Q data
      Q_in_L.end();
      Q_in_R.end();

#ifdef USE_MIXERS
      modeSelectInR.gain(0, 0);
      modeSelectInL.gain(0, 0);
#else
      patchCord9.disconnect();
      patchCord10.disconnect();
#endif

      modeSelectInExR.gain(0, 0);
      modeSelectInExL.gain(0, 0);
      modeSelectOutL.gain(0, 0);
      modeSelectOutR.gain(0, 0);
      modeSelectOutExL.gain(0, 0);
      modeSelectOutExR.gain(0, 0);
      break;

    case CALIBRATE_STATE:
      modeSelectOutExL.gain(0, powerOutCW[currentBand]);
      modeSelectOutExR.gain(0, powerOutCW[currentBand]);

#ifdef USE_MIXERS
      modeSelectInL.gain(0, 1);
      modeSelectInR.gain(0, 1);
#else
      patchCord9.connect();
      patchCord10.connect();
#endif

      modeSelectInExR.gain(0, 0);
      modeSelectInExL.gain(0, 0);

      modeSelectOutL.gain(0, 1);
      modeSelectOutR.gain(0, 1);
      modeSelectOutL.gain(1, 0);
      modeSelectOutR.gain(1, 0);
      modeSelectOutExL.gain(0, 1);
      modeSelectOutExR.gain(0, 1);
      break;

    default:
      break;
  }

}

#ifdef AUDIO_STATS

void StartAudioStats() {
  usecAudio = 0;
}

void EndAudioStats() {
  //int tmp = usecAudio;
  //Serial.print("Audio stat: ");
  //Serial.println(usecAudio);
  //Serial.println(tmp);
  //Serial.println(modeSelectInL.processorUsage());
  Serial.println(AudioMemoryUsageMax());
  AudioMemoryUsageMaxReset();
}

#endif
