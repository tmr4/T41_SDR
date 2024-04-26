
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//this constant ensures proper scaling for qa_fmemod testcases for SNR calculation and more.
#define fmdemod_quadri_K 0.340447550238101026565118445432744920253753662109375

extern float32_t qlast, ilast;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void AMDecodeSAM();
float ApproxAtan2(float y, float x);

//float32_t fmdemod_quadri_novect_cf(float32_t  inow, float32_t  qnow);
//void deemphasis_nfm_ff(float* input, float* output, int input_size, int sample_rate);
void nfmdemod(float32_t* input, float32_t* output, int input_size);
//void fmdemod_atan_cf(float32_t* input, float32_t* output, int input_size);
