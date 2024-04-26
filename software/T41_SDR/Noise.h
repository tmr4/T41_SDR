//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define ANR_DLINE_SIZE              512
#define MAX_LMS_TAPS                96
#define MAX_LMS_DELAY               256
#define NR_FFT_L                    256
#define NR_OPTIONS                  3   // { "Off", "Kim", "Spectral", "LMS" }

extern float32_t LMS_NormCoeff_f32[MAX_LMS_TAPS + MAX_LMS_DELAY];
extern float32_t LMS_nr_delay[512 + MAX_LMS_DELAY];
extern float32_t LMS_StateF32[MAX_LMS_TAPS + MAX_LMS_DELAY];
extern uint8_t NR_first_time;

extern float32_t /*DMAMEM*/ NR_FFT_buffer[512];
extern float32_t NR_output_audio_buffer[NR_FFT_L];
extern float32_t NR_last_iFFT_result[NR_FFT_L / 2];
extern float32_t NR_last_sample_buffer_L[NR_FFT_L / 2];
extern float32_t NR_last_sample_buffer_R[NR_FFT_L / 2];
extern float32_t NR_X[NR_FFT_L / 2][3];
extern float32_t NR_E[NR_FFT_L / 2][15];
extern float32_t NR_M[NR_FFT_L / 2];
extern float32_t NR_Nest[NR_FFT_L / 2][2]; //
extern float32_t NR_lambda[NR_FFT_L / 2];
extern float32_t NR_Gts[NR_FFT_L / 2][2];
extern float32_t NR_G[NR_FFT_L / 2];
extern float32_t NR_SNR_prio[NR_FFT_L / 2];
extern float32_t NR_SNR_post[NR_FFT_L / 2];
extern float32_t ANR_d[ANR_DLINE_SIZE];
extern float32_t ANR_w[ANR_DLINE_SIZE];

extern float32_t NR_Hk_old[NR_FFT_L / 2];

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void Kim1_NR();
void Xanr();
void SpectralNoiseReduction();
void InitLMSNoiseReduction();
void SpectralNoiseReductionInit();
