
#include <Audio.h>
#include <OpenAudio_ArduinoLibrary.h>  // https://github.com/chipaudette/OpenAudio_ArduinoLibrary

extern AudioMixer4 modeSelectInR;
extern AudioMixer4 modeSelectInL;
extern AudioMixer4 modeSelectInExR;
extern AudioMixer4 modeSelectInExL;

extern AudioMixer4 modeSelectOutL;
extern AudioMixer4 modeSelectOutR;
extern AudioMixer4 modeSelectOutExL;
extern AudioMixer4 modeSelectOutExR;

extern AudioRecordQueue Q_in_L;
extern AudioRecordQueue Q_in_R;
extern AudioRecordQueue Q_in_L_Ex;
extern AudioRecordQueue Q_in_R_Ex;

extern AudioPlayQueue Q_out_L;
extern AudioPlayQueue Q_out_R;
extern AudioPlayQueue Q_out_L_Ex;
extern AudioPlayQueue Q_out_R_Ex;

void AudioSetup();
void AudioStart();
void ConfigAudioState();

#ifdef AUDIO_STATS
void StartAudioStats();
void EndAudioStats();
#endif
