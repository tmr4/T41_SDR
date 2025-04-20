#include "SDT.h"
#include "AudioConfig.h"
#include "DSP_Fn.h"

#ifdef AUDIO_STATS
elapsedMicros usecAudio;
#endif

/**************************************************************
T41 audio chain
  Receive path:
    PCM1808 ADC -> i2s_quadIn (ch 3&4) -> Q_in_L/R -> DSP - > Q_out_L/R -> i2s_quadOut -> PCM5102 DAC

  Transmit path:
    Audio adapter mic -> i2s_quadIn (ch 1&2) -> Q_in_L/R_Ex -> DSP - > Q_out_L/R_Ex -> i2s_quadOut (ch 1&2) -> Audio adapter line out -> Exciter
    Sidetone: Q_out_L -> i2s_quadOut (ch 3) -> PCM5102 DAC
***************************************************************/

/*
See https://www.reddit.com/r/T41_EP/comments/1jnkhud/restructuring_the_t41_audio_chain/
for a discussion on reconfiguring the T41 audio chain.abort.

The use of mixers to control audio chain flow is inefficient:

  Mixer processor loading and memory usage:
    * each connected mixer adds between 50-250 usec to processor load each loop
    * each connected mixer consumes about 77 bytes
    * removing original v49.2k mixers and their patchcords speeds up main loop by about 2ms

  Memory Usage on Teensy 4.1:
    w/ 20 additional mixers and connections
    FLASH: code:233396, data:192036, headers:8736   free for files:7692296
    RAM1: variables:264416, code:184264, padding:12344   free for local variables:63264
    RAM2: variables:377024  free for malloc/new:147264

    w/o 20 additional mixers and connections
    FLASH: code:232548, data:192036, headers:8560   free for files:7693320
    RAM1: variables:262880, code:183416, padding:13192   free for local variables:64800
    RAM2: variables:377024  free for malloc/new:147264

    w/o all T41 mixers and their patchcords
    FLASH: code:227884, data:191632, headers:8508   free for files:7698440
    RAM1: variables:261184, code:179000, padding:17608   free for local variables:66496
    RAM2: variables:377024  free for malloc/new:147264
*/

AudioControlSGTL5000_Extended sgtl5000_1; // controller for the Teensy Audio Board microphone https://www.janbob.com/electron/OpenAudio_Design_Tool/index.html?info=AudioControlSGTL5000
AudioControlSGTL5000 sgtl5000_2;          // control object PCM1808 ADC (doesn't actually control ADC) https://www.pjrc.com/teensy/gui/?info=AudioControlSGTL5000

// Audio inputs
AudioInputI2SQuad i2s_quadIn; // Microphone on ch 1&2 and ADC on ch 3&4.  See https://www.pjrc.com/teensy/gui/?info=AudioOutputI2SQuad

// Microphone input (pin 8)
// *** TODO: in orginal software compressor objects are always active
//           restructure to add to chain only when active or examine
//           using the SGTL5000 to preprocess the microphone output ***
#ifdef USE_MIC_COMPRESSION
AudioConvert_I16toF32 int2Float1, int2Float2;  // https://www.janbob.com/electron/OpenAudio_Design_Tool/index.html?info=AudioConvert_I16toF32
AudioEffectCompressor_F32 comp1, comp2;        // https://www.janbob.com/electron/OpenAudio_Design_Tool/index.html?info=AudioEffectCompressor_F32
AudioConvert_F32toI16 float2Int1, float2Int2;  // https://www.janbob.com/electron/OpenAudio_Design_Tool/index.html?info=AudioConvert_F32toI16
#endif

AudioRecordQueue Q_in_L_Ex;
AudioRecordQueue Q_in_R_Ex;

#ifdef USE_MIC_COMPRESSION
AudioConnection patchCord1(i2s_quadIn, 0, int2Float1, 0);
AudioConnection patchCord2(i2s_quadIn, 1, int2Float2, 0);
AudioConnection_F32 patchCord3(int2Float1, 0, comp1, 0);
AudioConnection_F32 patchCord4(int2Float2, 0, comp2, 0);
AudioConnection_F32 patchCord5(comp1, 0, float2Int1, 0);
AudioConnection_F32 patchCord6(comp2, 0, float2Int2, 0);
AudioConnection patchCord7(float2Int1, 0, Q_in_L_Ex, 0);
AudioConnection patchCord8(float2Int2, 0, Q_in_R_Ex, 0);
#else
AudioConnection pc_Q_in_L_Ex(i2s_quadIn, 0, Q_in_L_Ex, 0);
AudioConnection pc_Q_in_R_Ex(i2s_quadIn, 1, Q_in_R_Ex, 0);
#endif

// Receive I/Q input (pin 6)
AudioRecordQueue Q_in_L; // https://www.pjrc.com/teensy/gui/?info=AudioRecordQueue
AudioRecordQueue Q_in_R;
AudioConnection pc_Q_in_L(i2s_quadIn, 2, Q_in_L, 0);
AudioConnection pc_Q_in_R(i2s_quadIn, 3, Q_in_R, 0);

// Audio outputs
AudioOutputI2SQuad i2s_quadOut; // configures pins 7 (Audio adapter line out on ch 1&2) and 32 (DAC on ch 3&4) as output. https://www.pjrc.com/teensy/gui/?info=AudioOutputI2SQuad

// Exciter I/Q (pin 7)
AudioPlayQueue Q_out_L_Ex; // https://www.pjrc.com/teensy/gui/?info=AudioPlayQueue
AudioPlayQueue Q_out_R_Ex;

AudioConnection pc_Q_out_L_Ex(Q_out_L_Ex, 0, i2s_quadOut, 0);
AudioConnection pc_Q_out_R_Ex(Q_out_R_Ex, 0, i2s_quadOut, 1);

// Receiver audio and Sidetone (pin 32)
AudioPlayQueue Q_out_L;

// *** TODO: consider adding a volume control ***
/*
AudioAmplifier outputAmp; // gain of 0 or 1 handled efficiently. https://www.pjrc.com/teensy/gui/?info=AudioAmplifier
AudioConnection pc_Q_out_L(Q_out_L, 0, outputAmp, 0);
AudioConnection pc_OutputAmp(outputAmp, 0, i2s_quadOut, 2);
*/

AudioConnection pc_Q_out_L(Q_out_L, 0, i2s_quadOut, 2);

#ifdef T41_USB_AUDIO
AudioOutputUSB usb1;
AudioAmplifier amp1, amp2;
AudioFilterBiquad biquad1;
AudioConnection patchCord25(Q_out_L, biquad1);
//AudioConnection patchCord25(Q_out_L, amp1);
AudioConnection patchCord26(biquad1, amp1);
AudioConnection patchCord27(amp1, 0, usb1, 0);
AudioConnection patchCord28(Q_out_L, 0, amp2, 0);
AudioConnection patchCord29(amp2, 0, usb1, 1);
#endif


void AudioSetup() {
  // configure an SGTL5000 control object for input from the audio adapter microphone
  sgtl5000_1.setAddress(LOW); // Teensy pin 8
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
  sgtl5000_2.setAddress(HIGH); // Teensy pin 6
  sgtl5000_2.enable();
  sgtl5000_2.inputSelect(AUDIO_INPUT_LINEIN);
  sgtl5000_2.volume(0.5);

  // *** TODO: examine need for these with regards to audio memory ***
  // enabling these causes unstable cw behavior
  //Q_out_L.setBehaviour(AudioPlayQueue::NON_STALLING);
  //Q_out_L_Ex.setBehaviour(AudioPlayQueue::NON_STALLING);
  //Q_out_R_Ex.setBehaviour(AudioPlayQueue::NON_STALLING);

#ifdef T41_USB_AUDIO
  amp1.gain(100);
  amp2.gain(200);
  //amp2.gain(100);
  //amp2.gain(1);
  //biquad1.setBandpass(0, 1000, 0.5);
  biquad1.setLowpass(0, 3000, 0.5);
#endif
}

inline void Q_in_Ex_Stop() {
  Q_in_L_Ex.end();
  Q_in_R_Ex.end();
  pc_Q_in_L_Ex.disconnect();
  pc_Q_in_R_Ex.disconnect();
  Q_in_L_Ex.clear();
  Q_in_R_Ex.clear();
}

inline void Q_in_Stop() {
  Q_in_L.end();
  Q_in_R.end();
  pc_Q_in_L.disconnect();
  pc_Q_in_R.disconnect();
  Q_in_L.clear();
  Q_in_R.clear();
}

inline void Q_out_Ex_Stop() {
  pc_Q_out_L_Ex.disconnect();
  pc_Q_out_R_Ex.disconnect();
}

inline void Q_out_Stop() {
  pc_Q_out_L.disconnect();
}

inline void Q_in_Ex_Start() {
  pc_Q_in_L_Ex.connect();
  pc_Q_in_R_Ex.connect();
  Q_in_L_Ex.begin();
  Q_in_R_Ex.begin();
}

inline void Q_in_Start() {
  pc_Q_in_L.connect();
  pc_Q_in_R.connect();
  Q_in_L.begin();
  Q_in_R.begin();
}

inline void Q_out_Ex_Start() {
  pc_Q_out_L_Ex.connect();
  pc_Q_out_R_Ex.connect();
}

inline void Q_out_Start() {
  pc_Q_out_L.connect();
}

void ConfigAudioState() {
  switch(radioState) {
    case SSB_RECEIVE_STATE:
    case CW_RECEIVE_STATE:
      digitalWrite(MUTE, LOW);      // unmute audio

      // stop and clear receive and transmit queues
      Q_in_Stop();
      Q_in_Ex_Stop();
      Q_out_Stop();
      Q_out_Ex_Stop();

      // start receive audio chain
      Q_in_Start();
      Q_out_Start();
      break;

    case SSB_TRANSMIT_STATE:
      digitalWrite(MUTE, HIGH);  // mute audio

      // stop and clear receive queue
      Q_in_Stop();
      Q_out_Stop();

      // start transmit audio chain
      Q_in_Ex_Start();
      Q_out_Ex_Start();
      break;

    case CW_TRANSMIT_STRAIGHT_STATE:
    case CW_TRANSMIT_KEYER_STATE:
      // stop and clear receive queues
      Q_in_Stop();
      Q_out_Stop();

      // start transmit audio chain and sidetone
      Q_out_Ex_Start();
      Q_out_Start(); // sidetone
      break;

    case CALIBRATE_STATE:
      //modeSelectOutExL.gain(0, powerOutCW[currentBand]);
      //modeSelectOutExR.gain(0, powerOutCW[currentBand]);
      //
      //patchCord9.connect();
      //patchCord10.connect();
      //
      //modeSelectInExR.gain(0, 0);
      //modeSelectInExL.gain(0, 0);
      //
      //modeSelectOutL.gain(0, 1);
      //modeSelectOutR.gain(0, 1);
      //modeSelectOutL.gain(1, 0);
      //modeSelectOutR.gain(1, 0);
      //modeSelectOutExL.gain(0, 1);
      //modeSelectOutExR.gain(0, 1);
      break;

    default:
      break;
  }

}


#ifdef USE_MIC_COMPRESSION
/*****
  Purpose: Setup Teensy Mic Compressor
  Parameter list:
    void
  Return value:
    void
*****/
FLASHMEM void SetupMicCompressors(boolean use_HP_filter1, float knee_dBFS1, float comp_ratio1, float attack_sec1, float release_sec1) {
  comp1.enableHPFilter(use_HP_filter1);
  comp2.enableHPFilter(use_HP_filter1);
  comp1.setThresh_dBFS(knee_dBFS1);
  comp2.setThresh_dBFS(knee_dBFS1);
  comp1.setCompressionRatio(comp_ratio1);
  comp2.setCompressionRatio(comp_ratio1);

  float fs_Hz = AUDIO_SAMPLE_RATE;
  comp1.setAttack_sec(attack_sec1, fs_Hz);
  comp2.setAttack_sec(attack_sec1, fs_Hz);
  comp1.setRelease_sec(release_sec1, fs_Hz);
  comp2.setRelease_sec(release_sec1, fs_Hz);
}
#endif

#ifdef AUDIO_STATS

void StartAudioStats() {
  usecAudio = 0;
  AudioMemoryUsageMaxReset();
}

void EndAudioStats() {
  //int tmp = usecAudio;
  //Serial.print("Audio stat: ");
  //Serial.println(usecAudio);
  //Serial.println(tmp);
  //Serial.println(modeSelectInL.processorUsage());
  Serial.println(AudioMemoryUsageMax());
}

#endif
