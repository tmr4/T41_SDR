
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern arm_fir_instance_f32 FIR_Hilbert_L;
extern arm_fir_instance_f32 FIR_Hilbert_R;

extern arm_fir_decimate_instance_f32 FIR_dec1_EX_I;
extern arm_fir_decimate_instance_f32 FIR_dec1_EX_Q;
extern arm_fir_decimate_instance_f32 FIR_dec2_EX_I;
extern arm_fir_decimate_instance_f32 FIR_dec2_EX_Q;

extern int16_t *sp_L2;
extern int16_t *sp_R2;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void ExciterIQData();
void SetBandRelay(int state);
