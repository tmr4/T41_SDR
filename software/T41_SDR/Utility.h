
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define DISPLAY_S_METER_DBM       0
#define DISPLAY_S_METER_DBMHZ     1
#define TABLE_SIZE_64             64
#define WIGGLE_ROOM               20     // This is the maximum value that can added to a BUSY_ANALOG_PIN pin read value of a push
                                         // button and still have the switch value be associated with the correct push button.

extern float32_t cosBuffer2[];
extern float32_t cosBuffer3[];

extern float32_t sinBuffer[];
extern float32_t sinBuffer2[];
extern float32_t sinBuffer3[];

extern double elapsed_micros_idx_t;
extern double elapsed_micros_sum;

extern const float32_t volumeLog[101];

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void sineTone(int numCycles);
void IQPhaseCorrection(float32_t *I_buffer, float32_t *Q_buffer, float32_t factor, uint32_t blocksize);
float MSinc(int m, float fc);
float32_t Izero(float32_t x);
float32_t log10f_fast(float32_t X);
float32_t AlphaBetaMag(float32_t  inphase, float32_t  quadrature);
float ApproxAtan(float z);
void SaveAnalogSwitchValues();
void DisplayClock();
void SetBand();
int SDPresentCheck();
void ShowTempAndLoad();

void initTempMon(uint16_t freq, uint32_t lowAlarmTemp, uint32_t highAlarmTemp, uint32_t panicAlarmTemp);

void FormatFrequency(long freq, char *freqBuffer);

float TGetTemp();

int  SetI2SFreq(int freq);
