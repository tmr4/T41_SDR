// modified from: https://github.com/DD4WH/Pocket_FT8
// this is a combination of Pocket_FT8.ino, Process_DSP.cpp and decode_ft8.cpp and their header files

#include <stdint.h>
#include <math.h>

#include "SDT.h"
#include "Display.h"

#ifdef FT8

#include "ft8.h"
#include "ft8_constants.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define FFT_SIZE  2048
#define ft8_buffer 368  //arbitrary for 3 kc
#define ft8_min_bin 48

#define ft8_msg_samples 92

//q15_t DMAMEM ft8_dsp_buffer[3*input_gulp_size] __attribute__ ((aligned (4)));
//q15_t DMAMEM dsp_output[FFT_SIZE *2] __attribute__ ((aligned (4)));
//q15_t DMAMEM window_ft8_dsp_buffer[FFT_SIZE] __attribute__ ((aligned (4)));
//float DMAMEM window[FFT_SIZE];

q15_t *ft8_dsp_buffer, *dsp_output, *window_ft8_dsp_buffer;
float *window;

uint32_t current_time, start_time, ft8_time;

int DSP_Flag;
int ft8_flag, FT_8_counter, ft8_decode_flag;
int num_decoded_msg;

int master_offset, offset_step;

bool ft8Init = false;
bool syncFlag = false;

//q15_t DMAMEM FFT_Scale[FFT_SIZE * 2]; 
q15_t DMAMEM FFT_Magnitude[FFT_SIZE]; 
int32_t DMAMEM FFT_Mag_10[FFT_SIZE/2];
float  DMAMEM mag_db[FFT_SIZE/2 + 1];

q15_t *FFT_Scale; //, *FFT_Magnitude;

arm_rfft_instance_q15 fft_inst;
//uint8_t DMAMEM export_fft_power[ft8_msg_samples*ft8_buffer*4] ;
uint8_t *export_fft_power;

typedef struct Candidate {
    int16_t      score;
    int16_t      time_offset;
    int16_t      freq_offset;
    uint8_t      time_sub;
    uint8_t      freq_sub;
} Candidate;

int max_score;

typedef struct
{
    char field1[14];
    char field2[14];
    char field3[7];
    int  freq_hz;
    char decode_time[10];
    int  sync_score;
    int  snr;
    int  distance;

} Decode;

Decode new_decoded[20];
//Decode *new_decoded;

// from: https://github.com/kgoba/ft8_lib/issues/3
// decode time can be adjusted by changing the number of candidates (kMax_candidates) and LDPC iterations (kLDPC_iterations).
const int kLDPC_iterations = 10;
const int kMax_candidates = 20;

const int kMax_decoded_messages = 10;  //chhh 27 feb
//const int kMax_message_length = 20;
const int kMax_message_length = 35; // from ft8_lib < max message length = callsign[13] + space + callsign[13] + space + report[6] + terminator
                                    // difference is included detail and spacing (Pocket_FT8 excludes some spacing I included)

const int kMin_score = 40;		// Minimum sync score threshold for candidates
//const int kMin_score = 30;
//const int kMin_score = 20;
//const int kMin_score = 10;		// try a lower score to see if we get anything
//const int kMin_score = 5;

//-------------------------------------------------------------------------------------------------------------
// forwards
//-------------------------------------------------------------------------------------------------------------

int load_wav(uint32_t num_samples);
int unpack_text(const uint8_t *a71, char *text);
int unpack_telemetry(const uint8_t *a71, char *telemetry);
int unpack_nonstandard(const uint8_t *a77, char *field1, char *field2, char *field3);
int unpack_type1(const uint8_t *a77, uint8_t i3, char *field1, char *field2, char *field3);

// Utility functions for characters and strings
const char * trim_front(const char *str);
char * trim(char *str);

// Convert a 2 digit integer to string
void int_to_dd(char *str, int value, int width, bool full_sign);

char charn(int c, int table_idx);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void displaySync(const char *message, int color) {
  // clear old line
  tft.fillRect(WATERFALL_L + 400, YPIXELS - 35, WATERFALL_W - 400, 35, RA8875_BLACK);
  tft.setFontScale(0,1);
  tft.setTextColor(color);

  tft.setCursor(WATERFALL_L + 425, YPIXELS - 35);
  tft.print(message);
}

void auto_sync_FT8() {
  // allow process to loop until we're within 1 second
  if((second())%15 <= 1) {
    // now we can sync up without causing a long delay
    while ((second())%15 != 0){
    }

    start_time =millis();
    ft8_flag = 1;
    FT_8_counter = 0;
    syncFlag = true;
    displaySync("sync'd", RA8875_GREEN);
  }
  else {
    displaySync("not sync'd", RA8875_RED);
  }
}

//void sync_FT8() {
//  start_time =millis();
//  ft8_flag = 1;
//  FT_8_counter = 0;
//}

// called when ft8_flag = 0
void update_synchronization() {
  current_time = millis();
  ft8_time = current_time  - start_time;

  // we're missing every other interval, try to relax this a bit
  // are we within 3 sec of 15 sec interval
  if(ft8_time % 15000 <= 200) 
  //if(ft8_flag == 0 && ft8_time % 15000 <= 266) { // within 4 sec of 15 sec interval
  {
    ft8_flag = 1;
    FT_8_counter = 0;
  }
}

float ft_blackman_i(int i, int N) {
  const float alpha = 0.16f; // or 2860/18608
  const float a0 = (1 - alpha) / 2;
  const float a1 = 1.0f / 2;
  const float a2 = alpha / 2;

  float x1 = cosf(2 * (float)M_PI * i / (N - 1));
  //float x2 = cosf(4 * (float)M_PI * i / (N - 1));
  float x2 = 2*x1*x1 - 1; // Use double angle formula
  return a0 - a1*x1 + a2*x2;
}

FLASHMEM bool init_DSP(void) {
  //Serial.println(sizeof(float32_t));
  
  export_fft_power = new uint8_t[ft8_msg_samples*ft8_buffer*4];

  if(export_fft_power == NULL) {
    Serial.println("Insufficient memory to activate FT8");
    return false;
  }

  int offset = 0;

  ft8_dsp_buffer = (q15_t *)&sharedRAM2[offset]; // 3 * input_gulp_size * 2 bytes = 3 * FFT_SIZE / 2 * 2 bytes
  offset += 3 * input_gulp_size * 2;
  dsp_output = (q15_t *)&sharedRAM2[offset]; // FFT_SIZE * 2 * 2 bytes
  offset += FFT_SIZE * 2 * 2;
  window_ft8_dsp_buffer = (q15_t *)&sharedRAM2[offset]; // FFT_SIZE * 2 bytes
  offset += FFT_SIZE * 2;
  window = (float *)&sharedRAM2[offset]; // FFT_SIZE * 4 bytes
  offset += FFT_SIZE * 4;

  //Serial.println(offset);

  offset = 0;
  //new_decoded = (Decode *)&sharedRAM1[offset]; // using 1280
  //offset += 1280;
  FFT_Scale = (q15_t *)&sharedRAM1[offset]; // FFT_SIZE * 2 * 2
  offset += FFT_SIZE * 2 * 2;
  //FFT_Magnitude = (q15_t *)&sharedRAM1[offset]; // FFT_SIZE * 2
  //offset += FFT_SIZE * 2;

  //Serial.println(offset);

  arm_rfft_init_q15(&fft_inst, FFT_SIZE, 0, 1);
  for (int i = 0; i < FFT_SIZE; ++i) {
    window[i] = ft_blackman_i(i, FFT_SIZE);
  }
  offset_step = (int) ft8_buffer*4;

  return true;
}

// Compute FFT magnitudes (log power) for each timeslot in the signal
void extract_power(int offset) {
  // Loop over two possible time offsets (0 and block_size/2)
  for (int time_sub = 0; time_sub <= input_gulp_size/2; time_sub += input_gulp_size/2) {

    for (int i = 0; i <  FFT_SIZE ; i++) {
      window_ft8_dsp_buffer[i] = (q15_t) ( (float) ft8_dsp_buffer[i + time_sub] * window[i] );
      //if(window_ft8_dsp_buffer[i] != 0) Serial.printf("%d: %d\n", i, window_ft8_dsp_buffer[i]);
    }

    arm_rfft_q15(&fft_inst, window_ft8_dsp_buffer,dsp_output );
    arm_shift_q15(&dsp_output[0], 5, &FFT_Scale[0], FFT_SIZE * 2 );
    //arm_shift_q15(&dsp_output[0], 6, &FFT_Scale[0], FFT_SIZE * 2 );
    arm_cmplx_mag_squared_q15(&FFT_Scale[0], &FFT_Magnitude[0], FFT_SIZE);

    for (int j = 0; j< FFT_SIZE/2; j++) {
      FFT_Mag_10[j] = 10 * (int32_t)FFT_Magnitude[j];
      //mag_db[j] =  5.0 * log((float)FFT_Mag_10[j] + 0.1);
      mag_db[j] =  10.0 * log((float)FFT_Mag_10[j] + 0.1);
    }

    // Loop over two possible frequency bin offsets (for averaging)
    for (int freq_sub = 0; freq_sub < 2; ++freq_sub) {
      for (int j = 0; j < ft8_buffer; ++j) {
        float db1 = mag_db[j * 2 + freq_sub];
        float db2 = mag_db[j * 2 + freq_sub + 1];
        float db = (db1 + db2) / 2;
        
        int scaled = (int) (db);
        export_fft_power[offset] = (scaled < 0) ? 0 : ((scaled > 255) ? 255 : scaled);
        ++offset;
      }
    }
  }
}

// called when ft8_flag = 1
void process_FT8_FFT(void) {
  master_offset = offset_step * FT_8_counter;
  extract_power(master_offset);

  FT_8_counter++;
  if (FT_8_counter == ft8_msg_samples) {
    ft8_flag = 0;
    ft8_decode_flag = 1;
  }
}

static float max2(float a, float b) {
  return (a >= b) ? a : b;
}

static float max4(float a, float b, float c, float d) {
  return max2(max2(a, b), max2(c, d));
}

static void heapify_down(Candidate *heap, int heap_size) {
  // heapify from the root down
  int current = 0;
  while (true) {
    int largest = current;
    int left = 2 * current + 1;
    int right = left + 1;

    if (left < heap_size && heap[left].score < heap[largest].score) {
      largest = left;
    }
    if (right < heap_size && heap[right].score < heap[largest].score) {
      largest = right;
    }
    if (largest == current) {
      break;
    }

    Candidate tmp = heap[largest];
    heap[largest] = heap[current];
    heap[current] = tmp;
    current = largest;
  }
}

static void heapify_up(Candidate *heap, int heap_size) {
  // heapify from the last node up
  int current = heap_size - 1;
  while (current > 0) {
    int parent = (current - 1) / 2;
    if (heap[current].score >= heap[parent].score) {
      break;
    }

    Candidate tmp = heap[parent];
    heap[parent] = heap[current];
    heap[current] = tmp;
    current = parent;
  }
}

// Compute unnormalized log likelihood log(p(1) / p(0)) of 3 message bits (1 FSK symbol)
static void decode_symbol(const uint8_t *power, const uint8_t *code_map, int bit_idx, float *log174) {
  // Cleaned up code for the simple case of n_syms==1
  float s2[8];

  for (int j = 0; j < 8; ++j) {
    s2[j] = (float)power[code_map[j]];
    //s2[j] = (float)work_fft_power[offset+code_map[j]];
  }

  log174[bit_idx + 0] = max4(s2[4], s2[5], s2[6], s2[7]) - max4(s2[0], s2[1], s2[2], s2[3]);
  log174[bit_idx + 1] = max4(s2[2], s2[3], s2[6], s2[7]) - max4(s2[0], s2[1], s2[4], s2[5]);
  log174[bit_idx + 2] = max4(s2[1], s2[3], s2[5], s2[7]) - max4(s2[0], s2[2], s2[4], s2[6]);
}

// find_sync(export_fft_power, ft8_msg_samples, ft8_buffer, kCostas_map, kMax_candidates, candidate_list, kMin_score);
// Localize top N candidates in frequency and time according to their sync strength (looking at Costas symbols)
// We treat and organize the candidate list as a min-heap (empty initially).
int find_sync(const uint8_t *power, int num_blocks, int num_bins, const uint8_t *sync_map, int num_candidates, Candidate *heap, int min_score) {
  int heap_size = 0;
  max_score = 0;
  // int x = 500;
  // Here we allow time offsets that exceed signal boundaries, as long as we still have all data bits.
  // I.e. we can afford to skip the first 7 or the last 7 Costas symbols, as long as we track how many
  // sync symbols we included in the score, so the score is averaged.
  for (int alt = 0; alt < 4; ++alt) {
    for (int time_offset = -7; time_offset < num_blocks - NN + 7; ++time_offset) {  // NN=79
      for (int freq_offset = ft8_min_bin; freq_offset < num_bins - 8; ++freq_offset) {
        int score = 0;

        // Compute average score over sync symbols (m+k = 0-7, 36-43, 72-79)
        int num_symbols = 0;
        for (int m = 0; m <= 72; m += 36) {
          for (int k = 0; k < 7; ++k) {
            // Check for time boundaries
            if (time_offset + k + m < 0) continue;
            if (time_offset + k + m >= num_blocks) break;

            int offset = ((time_offset + k + m) * 4 + alt) * num_bins + freq_offset;

            const uint8_t *p8 = power + offset;

            // note from ft8_lib decode.c ft8_sync_score()
            // Weighted difference between the expected and all other symbols
            // Does not work as well as the alternative score below
            score += 8 * p8[sync_map[k]] -
                          p8[0] - p8[1] - p8[2] - p8[3] -
                          p8[4] - p8[5] - p8[6] - p8[7];
/*
            // Check only the neighbors of the expected symbol frequency- and time-wise
            int sm = sync_map[k];   // Index of the expected bin
            if (sm > 0) {
                // look at one frequency bin lower
                score += p8[sm] - p8[sm - 1];
            }
            if (sm < 7) {
                // look at one frequency bin higher
                score += p8[sm] - p8[sm + 1];
            }
            if (k > 0) {
                // look one symbol back in time
                score += p8[sm] - p8[sm - 4 * num_bins];
            }
            if (k < 6) {
                // look one symbol forward in time
                score += p8[sm] - p8[sm + 4 * num_bins];
            }
*/
            ++num_symbols;
          }
        }
        score /= num_symbols;

        // if(score > max_score) max_score = score;
        if (score < min_score) continue;

        // If the heap is full AND the current candidate is better than
        // the worst in the heap, we remove the worst and make space
        if (heap_size == num_candidates && score > heap[0].score) {
            heap[0] = heap[heap_size - 1];
            --heap_size;

            heapify_down(heap, heap_size);
        }

        // If there's free space in the heap, we add the current candidate
        if (heap_size < num_candidates) {
            heap[heap_size].score = score;
            heap[heap_size].time_offset = time_offset;
            heap[heap_size].freq_offset = freq_offset;
            heap[heap_size].time_sub = alt / 2;
            heap[heap_size].freq_sub = alt % 2;
            ++heap_size;

            heapify_up(heap, heap_size);
        }
      }
    }
  } //end of alt

  return heap_size;
}

// Compute log likelihood log(p(1) / p(0)) of 174 message bits
// for later use in soft-decision LDPC decoding
void extract_likelihood(const uint8_t *power, int num_bins, Candidate cand, const uint8_t *code_map, float *log174) {
  int ft8_offset = (cand.time_offset * 4 + cand.time_sub * 2 + cand.freq_sub) * num_bins + cand.freq_offset;

  //tft.graphicsMode();
  //tft.drawLine(cand.freq_offset, 479,cand.freq_offset,479-100 , RA8875_YELLOW);
  //tft.drawLine(0, 0,1,1 , RA8875_YELLOW);

  //display_value(500,20,ft8_offset);

  //show_variable(600,200,offset);
  // Go over FSK tones and skip Costas sync symbols
  const int n_syms = 1;

  for (int k = 0; k < ND; k += n_syms) {
    int sym_idx = (k < ND / 2) ? (k + 7) : (k + 14);
    int bit_idx = 3 * k;

    // Pointer to 8 bins of the current symbol
    const uint8_t *ps = power + (ft8_offset + sym_idx * 4 * num_bins);

    decode_symbol(ps, code_map, bit_idx, log174);
  }

  // Compute the variance of log174
  float sum   = 0;
  float sum2  = 0;
  float inv_n = 1.0f / N;
  for (int i = 0; i < N; ++i) {
    sum  += log174[i];
    sum2 += log174[i] * log174[i];
  }
  float variance = (sum2 - sum * sum * inv_n) * inv_n;
  // Normalize log174 such that sigma = 2.83 (Why? It's in WSJT-X, ft8b.f90)
  // Seems to be 2.83 = sqrt(8). Experimentally sqrt(16) works better.
  float norm_factor = sqrtf(16.0f / variance);
  for (int i = 0; i < N; ++i) {
    log174[i] *= norm_factor;
  }
}

//
// does a 174-bit codeword pass the FT8's LDPC parity checks?
// returns the number of parity errors.
// 0 means total success.
//
static int ldpc_check(uint8_t codeword[]) {
  int errors = 0;

  for (int j = 0; j < M; ++j) {
    uint8_t x = 0;
    for (int i = 0; i < kNrw[j]; ++i) {
      x ^= codeword[kNm[j][i] - 1];
    }
    if (x != 0) {
      ++errors;
    }
  }
  return errors;
}

// https://varietyofsound.wordpress.com/2011/02/14/efficient-tanh-computation-using-lamberts-continued-fraction/
// http://functions.wolfram.com/ElementaryFunctions/ArcTanh/10/0001/
// https://mathr.co.uk/blog/2017-09-06_approximating_hyperbolic_tangent.html

// thank you Douglas Bagnall
// https://math.stackexchange.com/a/446411
static float fast_tanh(float x) {
  if (x < -4.97f) {
    return -1.0f;
  }
  if (x > 4.97f) {
    return 1.0f;
  }
  float x2 = x * x;
  //float a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
  //float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
  //float a = x * (10395.0f + x2 * (1260.0f + x2 * 21.0f));
  //float b = 10395.0f + x2 * (4725.0f + x2 * (210.0f + x2));
  float a = x * (945.0f + x2 * (105.0f + x2));
  float b = 945.0f + x2 * (420.0f + x2 * 15.0f);
  return a / b;
}

static float fast_atanh(float x) {
  float x2 = x * x;
  //float a = x * (-15015.0f + x2 * (19250.0f + x2 * (-5943.0f + x2 * 256.0f)));
  //float b = (-15015.0f + x2 * (24255.0f + x2 * (-11025.0f + x2 * 1225.0f)));
  //float a = x * (-1155.0f + x2 * (1190.0f + x2 * -231.0f));
  //float b = (-1155.0f + x2 * (1575.0f + x2 * (-525.0f + x2 * 25.0f)));
  float a = x * (945.0f + x2 * (-735.0f + x2 * 64.0f));
  float b = (945.0f + x2 * (-1050.0f + x2 * 225.0f));
  return a / b;
}

void bp_decode(float codeword[], int max_iters, uint8_t plain[], int *ok) {
  float tov[N][3];
  float toc[M][7];

  int min_errors = M;

  // initialize messages to checks
  for (int i = 0; i < M; ++i) {
    for (int j = 0; j < kNrw[i]; ++j) {
      toc[i][j] = codeword[kNm[i][j] - 1];
    }
  }

  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < 3; ++j) {
      tov[i][j] = 0;
    }
  }

  for (int iter = 0; iter < max_iters; ++iter) {
    float   zn[N];

    // Update bit log likelihood ratios (tov=0 in iter 0)
    for (int i = 0; i < N; ++i) {
      zn[i] = codeword[i] + tov[i][0] + tov[i][1] + tov[i][2];
      plain[i] = (zn[i] > 0) ? 1 : 0;
    }

    // Check to see if we have a codeword (check before we do any iter)
    int errors = ldpc_check(plain);

    if (errors < min_errors) {
      // we have a better guess - update the result
      min_errors = errors;

      if (errors == 0) {
        break;  // Found a perfect answer
      }
    }

    // Send messages from bits to check nodes
    for (int i = 0; i < M; ++i) {
      for (int j = 0; j < kNrw[i]; ++j) {
        int ibj = kNm[i][j] - 1;
        toc[i][j] = zn[ibj];
        for (int kk = 0; kk < 3; ++kk) {
          // subtract off what the bit had received from the check
          if (kMn[ibj][kk] - 1 == i) {
            toc[i][j] -= tov[ibj][kk];
          }
        }
      }
    }

    // send messages from check nodes to variable nodes
    for (int i = 0; i < M; ++i) {
      for (int j = 0; j < kNrw[i]; ++j) {
        toc[i][j] = fast_tanh(-toc[i][j] / 2);
      }
    }

    for (int i = 0; i < N; ++i) {
      for (int j = 0; j < 3; ++j) {
        int ichk = kMn[i][j] - 1; // kMn(:,j) are the checks that include bit j
        float Tmn = 1.0f;
        for (int k = 0; k < kNrw[ichk]; ++k) {
          if (kNm[ichk][k] - 1 != i) {
            Tmn *= toc[ichk][k];
          }
        }
        tov[i][j] = 2 * fast_atanh(-Tmn);
      }
    }
  }

  *ok = min_errors;
}

// Packs a string of bits each represented as a zero/non-zero byte in plain[],
// as a string of packed bits starting from the MSB of the first byte of packed[]
void pack_bits(const uint8_t plain[], int num_bits, uint8_t packed[]) {
  int num_bytes = (num_bits + 7) / 8;
  for (int i = 0; i < num_bytes; ++i) {
    packed[i] = 0;
  }

  uint8_t mask = 0x80;
  int     byte_idx = 0;
  for (int i = 0; i < num_bits; ++i) {
    if (plain[i]) {
      packed[byte_idx] |= mask;
    }
    mask >>= 1;
    if (!mask) {
      mask = 0x80;
      ++byte_idx;
    }
  }
}

// field1 - at least 14 bytes
// field2 - at least 14 bytes
// field3 - at least 7 bytes
int unpack77_fields(const uint8_t *a77, char *field1, char *field2, char *field3) {
  uint8_t  n3, i3;

  // Extract n3 (bits 71..73) and i3 (bits 74..76)
  n3 = ((a77[8] << 2) & 0x04) | ((a77[9] >> 6) & 0x03);
  i3 = (a77[9] >> 3) & 0x07;

  field1[0] = field2[0] = field3[0] = '\0';

  if (i3 == 0 && n3 == 0) {
    // 0.0  Free text
    return unpack_text(a77, field1);
  }
  // else if (i3 == 0 && n3 == 1) {
  //     // 0.1  K1ABC RR73; W9XYZ <KH1/KH7Z> -11   28 28 10 5       71   DXpedition Mode
  // }
  // else if (i3 == 0 && n3 == 2) {
  //     // 0.2  PA3XYZ/P R 590003 IO91NP           28 1 1 3 12 25   70   EU VHF contest
  // }
  // else if (i3 == 0 && (n3 == 3 || n3 == 4)) {
  //     // 0.3   WA9XYZ KA1ABC R 16A EMA            28 28 1 4 3 7    71   ARRL Field Day
  //     // 0.4   WA9XYZ KA1ABC R 32A EMA            28 28 1 4 3 7    71   ARRL Field Day
  // }
  else if (i3 == 0 && n3 == 5) {
    // 0.5   0123456789abcdef01                 71               71   Telemetry (18 hex)
    return unpack_telemetry(a77, field1);
  }
  else if (i3 == 1 || i3 == 2) {
    // Type 1 (standard message) or Type 2 ("/P" form for EU VHF contest)
    return unpack_type1(a77, i3, field1, field2, field3);
  }
  // else if (i3 == 3) {
  //     // Type 3: ARRL RTTY Contest
  // }
  else if (i3 == 4) {
  //     // Type 4: Nonstandard calls, e.g. <WA9XYZ> PJ4/KA1ABC RR73
  //     // One hashed call or "CQ"; one compound or nonstandard call with up
  //     // to 11 characters; and (if not "CQ") an optional RRR, RR73, or 73.
    return unpack_nonstandard(a77, field1, field2, field3);
  }
  // else if (i3 == 5) {
  //     // Type 5: TU; W9XYZ K1ABC R-09 FN             1 28 28 1 7 9       74   WWROF contest
  // }

  // unknown type, should never get here
  return -1;
}

// Compute 14-bit CRC for a sequence of given number of bits
// [IN] message  - byte sequence (MSB first)
// [IN] num_bits - number of bits in the sequence
uint16_t crc(uint8_t *message, int num_bits) {
  // Adapted from https://barrgroup.com/Embedded-Systems/How-To/CRC-Calculation-C-Code
  //constexpr uint16_t  TOPBIT = (1 << (CRC_WIDTH - 1));
  uint16_t  TOPBIT = (1 << (CRC_WIDTH - 1));
  // printf("CRC, %d bits: ", num_bits);
  // for (int i = 0; i < (num_bits + 7) / 8; ++i) {
  //     printf("%02x ", message[i]);
  // }
  // printf("\n");

  uint16_t remainder = 0;
  int idx_byte = 0;

  // Perform modulo-2 division, a bit at a time.
  for (int idx_bit = 0; idx_bit < num_bits; ++idx_bit) {
    if (idx_bit % 8 == 0) {
      // Bring the next byte into the remainder.
      remainder ^= (message[idx_byte] << (CRC_WIDTH - 8));
      ++idx_byte;
    }

    // Try to divide the current data bit.
    if (remainder & TOPBIT) {
      remainder = (remainder << 1) ^ CRC_POLYNOMIAL;
    }
    else {
      remainder = (remainder << 1);
    }
  }
  // printf("CRC = %04xh\n", remainder & ((1 << CRC_WIDTH) - 1));
  return remainder & ((1 << CRC_WIDTH) - 1);
}

int validate_locator(char locator[]) {
  uint8_t A1, A2, N1, N2;
  uint8_t test = 0;

  A1 = locator[0] - 65;
  A2 = locator[1] - 65;
  N1 = locator[2] - 48;
  N2= locator [3] - 48;

  if (A1 >= 0 && A1 <= 17) test++;
  if (A2 > 0 && A2 < 17) test++; //block RR73 Artic and Anartica
  if (N1 >= 0 && N1 <= 9) test++;
  if (N2 >= 0 && N2 <= 9) test++;

  if (test == 4) return 1;
  else
  return 0;
}

int ft8_decode(void) {
  char rtc_string[10];   // print format stuff
  Candidate candidate_list[kMax_candidates];
  Candidate cand;
  char decoded[kMax_decoded_messages][kMax_message_length];
  float freq_hz;
  int num_candidates;
  //char message[kMax_message_length];
  char message[48];
  uint16_t chksum, chksum2;
  int raw_RSL;
  int display_RSL;
  float distance;
  int num_decoded = 0;
  float   log174[N];
  uint8_t plain[N];
  int n_errors;
  uint8_t a91[K_BYTES];
  char field1[14];
  char field2[14];
  char field3[7];
  int rc;
  const float fsk_dev = 6.25f;    // tone deviation in Hz and symbol rate
  bool found;

  // Find top candidates by Costas sync score and localize them in time and frequency
  num_candidates = find_sync(export_fft_power, ft8_msg_samples, ft8_buffer, kCostas_map, kMax_candidates, candidate_list, kMin_score);

  // Go over candidates and attempt to decode messages
  for(int idx = 0; idx < num_candidates; ++idx) {
    cand = candidate_list[idx];
    freq_hz  = (cand.freq_offset + cand.freq_sub / 2.0f) * fsk_dev;

    extract_likelihood(export_fft_power, ft8_buffer, cand, kGray_map, log174);

    // bp_decode() produces better decodes, uses way less memory
    n_errors = 0;
    bp_decode(log174, kLDPC_iterations, plain, &n_errors);
    
    if (n_errors > 0)    continue;
    //if (n_errors > 0)    {
    //  Serial.print("errors = "); Serial.println(n_errors);
    //}

    // Extract payload + CRC (first K bits)
    pack_bits(plain, K, a91);
    
    // Extract CRC and check it
    chksum = ((a91[9] & 0x07) << 11) | (a91[10] << 3) | (a91[11] >> 5);
    a91[9] &= 0xF8;
    a91[10] = 0;
    a91[11] = 0;
    chksum2 = crc(a91, 96 - 14);
    if (chksum != chksum2)   continue;
    //if (chksum != chksum2) {
    //  Serial.print("chksum = "); Serial.print(chksum); Serial.print(" chksum2 = "); Serial.println(chksum2);
    //}
    
    rc = unpack77_fields(a91, field1, field2, field3);
    if (rc < 0) continue;
    //if (rc < 0) {
    //  Serial.print("rc = "); Serial.println(rc);
    //  }
    
    sprintf(message,"%7s %7s %4s ",field1, field2, field3);
    //if(strlen(message) > 10) Serial.println(message);

    // Check for duplicate messages (TODO: use hashing)
    found = false;
    for (int i = 0; i < num_decoded; ++i) {
      if (0 == strcmp(decoded[i], message)) {
        found = true;
        break;
      }
    }

    getTeensy3Time();
    sprintf(rtc_string,"%2i:%2i:%2i",hour(),minute(),second());

    if (!found && num_decoded < kMax_decoded_messages) {
      if(strlen(message) < kMax_message_length) {
        //Serial.println("creating message");
        //Serial.println(message);
        strcpy(decoded[num_decoded], message);

        new_decoded[num_decoded].sync_score = cand.score;
        new_decoded[num_decoded].freq_hz = (int)freq_hz;
        strcpy(new_decoded[num_decoded].field1, field1);
        strcpy(new_decoded[num_decoded].field2, field2);
        strcpy(new_decoded[num_decoded].field3, field3);
        strcpy(new_decoded[num_decoded].decode_time, rtc_string);
          
        raw_RSL = new_decoded[num_decoded].sync_score;
        if (raw_RSL > 160) {
          raw_RSL = 160;
        }
        display_RSL = (raw_RSL - 160 ) / 6;
        new_decoded[num_decoded].snr = display_RSL;

        char Target_Locator[] = "    ";

        strcpy(Target_Locator, new_decoded[num_decoded].field3);

        if (validate_locator(Target_Locator)  == 1) {
          distance = 0;
          new_decoded[num_decoded].distance = (int)distance;
        }
        else {
          new_decoded[num_decoded].distance = 0;
        }
        ++num_decoded;
      }

    }
  }  //End of big decode loop

  return num_decoded;
}

void display_messages(int decoded_messages, int message_limit){
/*
  char message[kMax_message_length];
  char big_gulp[60];

  tft.fillRect(0, 120, 320, 195, HX8357_BLACK);

  for (int i = 0; i<decoded_messages && i < message_limit; i++ ) {
    sprintf(message,"%s %s %s",new_decoded[i].field1, new_decoded[i].field2, new_decoded[i].field3);

    sprintf(big_gulp,"%s %s", new_decoded[i].decode_time, message);
    tft.setTextColor(HX8357_YELLOW , HX8357_BLACK);
    tft.setTextSize(2);
    tft.setCursor(0, 120 + i *25 );
    tft.print(message);
  }
*/
}

void display_details(int decoded_messages, int message_limit) {
  char message[48];
  int rowCount = 5;
  int columnOffset = 0;

  //tft.setFontScale((enum RA8875tsize)0);
  //tft.setFontScale(3,2);
  //tft.setFontScale(1,2);
  tft.setFontScale(0,1);
  tft.setTextColor(RA8875_WHITE);

  // print messages in 2 columns
  for (int i = 0; i < decoded_messages && i < message_limit; i++){
    sprintf(message,"%.13s %.13s %.6s",new_decoded[i].field1, new_decoded[i].field2, new_decoded[i].field3);
    tft.setCursor(WATERFALL_L + columnOffset, YPIXELS - 25 * rowCount);
    tft.print(message);

    if(--rowCount == 0) {
      // start in column 2
      rowCount = 5;
      columnOffset = 256;
    }
  }
}

//-------------------------------------------------------------------------------------------------------------
// unpack.cpp
//-------------------------------------------------------------------------------------------------------------

const uint32_t MAX22    = 4194304L;
const uint32_t NTOKENS  = 2063592L;
const uint16_t MAXGRID4 = 32400L;


// n28 is a 28-bit integer, e.g. n28a or n28b, containing all the
// call sign bits from a packed message.
int unpack28(uint32_t n28, uint8_t ip, uint8_t i3, char *result) {
  // Check for special tokens DE, QRZ, CQ, CQ_nnn, CQ_aaaa
  if (n28 < NTOKENS) {
    if (n28 <= 2) {
      if (n28 == 0) strcpy(result, "DE");
      if (n28 == 1) strcpy(result, "QRZ");
      if (n28 == 2) strcpy(result, "CQ");
      return 0;   // Success
    }
    if (n28 <= 1002) {
      // CQ_nnn with 3 digits
      strcpy(result, "CQ ");
      int_to_dd(result + 3, n28 - 3, 3, true);
      return 0;   // Success
    }
    if (n28 <= 532443L) {
      // CQ_aaaa with 4 alphanumeric symbols
      uint32_t n = n28 - 1003;
      char aaaa[5];

      aaaa[4] = '\0';
      for (int i = 3; /* */; --i) {
        aaaa[i] = charn(n % 27, 4);
        if (i == 0) break;
        n /= 27;
      }

      strcpy(result, "CQ ");
      strcat(result, trim_front(aaaa));
      return 0;   // Success
    }
    // ? TODO: unspecified in the WSJT-X code
    return -1;
  }

  n28 = n28 - NTOKENS;
  if (n28 < MAX22) {
    // This is a 22-bit hash of a result
    //call hash22(n22,c13)     !Retrieve result from hash table
    // TODO: implement
    // strcpy(result, "<...>");
    result[0] = '<';
    int_to_dd(result + 1, n28, 7, true);
    result[8] = '>';
    result[9] = '\0';
    return 0;
  }

  // Standard callsign
  uint32_t n = n28 - MAX22;

  char callsign[7];
  callsign[6] = '\0';
  callsign[5] = charn(n % 27, 4);
  n /= 27;
  callsign[4] = charn(n % 27, 4);
  n /= 27;
  callsign[3] = charn(n % 27, 4);
  n /= 27;
  callsign[2] = charn(n % 10, 3);
  n /= 10;
  callsign[1] = charn(n % 36, 2);
  n /= 36;
  callsign[0] = charn(n % 37, 1);

  // Skip trailing and leading whitespace in case of a short callsign
  strcpy(result, trim(callsign));
  if (strlen(result) == 0) return -1;

  // Check if we should append /R or /P suffix
  if (ip) {
    if (i3 == 1) {
      strcat(result, "/R");
    }
    else if (i3 == 2) {
      strcat(result, "/P");
    }
  }

  return 0;   // Success
}

int unpack_type1(const uint8_t *a77, uint8_t i3, char *field1, char *field2, char *field3) {
  uint32_t n28a, n28b;
  uint16_t igrid4;
  uint8_t  ir;

  // Extract packed fields
  // read(c77,1000) n28a,ipa,n28b,ipb,ir,igrid4,i3
  // 1000 format(2(b28,b1),b1,b15,b3)
  n28a  = (a77[0] << 21);
  n28a |= (a77[1] << 13);
  n28a |= (a77[2] << 5);
  n28a |= (a77[3] >> 3);
  n28b  = ((a77[3] & 0x07) << 26);
  n28b |= (a77[4] << 18);
  n28b |= (a77[5] << 10);
  n28b |= (a77[6] << 2);
  n28b |= (a77[7] >> 6);
  ir      = ((a77[7] & 0x20) >> 5);
  igrid4  = ((a77[7] & 0x1F) << 10);
  igrid4 |= (a77[8] << 2);
  igrid4 |= (a77[9] >> 6);

  // Unpack both callsigns
  if (unpack28(n28a >> 1, n28a & 0x01, i3, field1) < 0) {
    return -1;
  }
  if (unpack28(n28b >> 1, n28b & 0x01, i3, field2) < 0) {
    return -2;
  }
  // Fix "CQ_" to "CQ " -> already done in unpack28()

  // TODO: add to recent calls
  // if (field1[0] != '<' && strlen(field1) >= 4) {
  //     save_hash_call(field1)
  // }
  // if (field2[0] != '<' && strlen(field2) >= 4) {
  //     save_hash_call(field2)
  // }

  if (igrid4 <= MAXGRID4) {
    // Extract 4 symbol grid locator
    char *dst = field3;
    uint16_t n = igrid4;
    if (ir > 0) {
      // In case of ir=1 add an "R" before grid
      dst = stpcpy(dst, "R ");
    }

    dst[4] = '\0';
    dst[3] = '0' + (n % 10);
    n /= 10;
    dst[2] = '0' + (n % 10);
    n /= 10;
    dst[1] = 'A' + (n % 18);
    n /= 18;
    dst[0] = 'A' + (n % 18);
    // if(msg(1:3).eq.'CQ ' .and. ir.eq.1) unpk77_success=.false.
    // if (ir > 0 && strncmp(field1, "CQ", 2) == 0) return -1;
  }
  else {
    // Extract report
    int irpt = igrid4 - MAXGRID4;

    // Check special cases first
    if (irpt == 1) field3[0] = '\0';
    else if (irpt == 2) strcpy(field3, "RRR");
    else if (irpt == 3) strcpy(field3, "RR73");
    else if (irpt == 4) strcpy(field3, "73");
    else if (irpt >= 5) {
      char *dst = field3;
      // Extract signal report as a two digit number with a + or - sign
      if (ir > 0) {
        *dst++ = 'R'; // Add "R" before report
      }
      int_to_dd(dst, irpt - 35, 2, true);
    }
    // if(msg(1:3).eq.'CQ ' .and. irpt.ge.2) unpk77_success=.false.
    // if (irpt >= 2 && strncmp(field1, "CQ", 2) == 0) return -1;
  }

  return 0;       // Success
}

int unpack_text(const uint8_t *a71, char *text) {
  // TODO: test
  uint8_t b71[9];

  uint8_t carry = 0;
  for (int i = 0; i < 9; ++i) {
    b71[i] = carry | (a71[i] >> 1);
    carry = (a71[i] & 1) ? 0x80 : 0;
  }

	char c14[14];
	c14[13] = 0;
  for (int idx = 12; idx >= 0; --idx) {
    // Divide the long integer in b71 by 42
    uint16_t rem = 0;
    for (int i = 0; i < 9; ++i) {
      rem = (rem << 8) | b71[i];
      b71[i] = rem / 42;
      rem    = rem % 42;
    }
    c14[idx] = charn(rem, 0);
  }

	strcpy(text, trim(c14));
  return 0;       // Success
}

int unpack_telemetry(const uint8_t *a71, char *telemetry) {
  uint8_t b71[9];

  // Shift bits in a71 right by 1
  uint8_t carry = 0;
  for (int i = 0; i < 9; ++i) {
    b71[i] = (carry << 7) | (a71[i] >> 1);
    carry = (a71[i] & 0x01);
  }

  // Convert b71 to hexadecimal string
  for (int i = 0; i < 9; ++i) {
    uint8_t nibble1 = (b71[i] >> 4);
    uint8_t nibble2 = (b71[i] & 0x0F);
    char c1 = (nibble1 > 9) ? (nibble1 - 10 + 'A') : nibble1 + '0';
    char c2 = (nibble2 > 9) ? (nibble2 - 10 + 'A') : nibble2 + '0';
    telemetry[i * 2] = c1;
    telemetry[i * 2 + 1] = c2;
  }

  telemetry[18] = '\0';
  return 0;
}

//none standard for wsjt-x 2.0
//by KD8CEC
int unpack_nonstandard(const uint8_t *a77, char *field1, char *field2, char *field3) {
/*
	wsjt-x 2.1.0 rc5
     	read(c77,1050) n12,n58,iflip,nrpt,icq
	1050 format(b12,b58,b1,b2,b1)
*/
	uint32_t n12, iflip, nrpt, icq;
	uint64_t n58;
	n12 = (a77[0] << 4);   //11 ~4  : 8
	n12 |= (a77[1] >> 4);  //3~0 : 12

	n58 = ((uint64_t)(a77[1] & 0x0F) << 54); //57 ~ 54 : 4
	n58 |= ((uint64_t)a77[2] << 46); //53 ~ 46 : 12
	n58 |= ((uint64_t)a77[3] << 38); //45 ~ 38 : 12
	n58 |= ((uint64_t)a77[4] << 30); //37 ~ 30 : 12
	n58 |= ((uint64_t)a77[5] << 22); //29 ~ 22 : 12
	n58 |= ((uint64_t)a77[6] << 14); //21 ~ 14 : 12
	n58 |= ((uint64_t)a77[7] << 6);  //13 ~ 6 : 12
	n58 |= ((uint64_t)a77[8] >> 2);  //5 ~ 0 : 765432 10

	iflip = (a77[8] >> 1) & 0x01;	//76543210
	nrpt  = ((a77[8] & 0x01) << 1);
	nrpt  |= (a77[9] >> 7);	//76543210
	icq   = ((a77[9] >> 6) & 0x01);

	char c11[12];
	c11[11] = '\0';

  for (int i = 10; /* no condition */ ; --i) {
    c11[i] = charn(n58 % 38, 5);
      if (i == 0) break;
    n58 /= 38;
  }

	char call_3[15];
  // should replace with hash12(n12, call_3);
  // strcpy(call_3, "<...>");
  call_3[0] = '<';
  int_to_dd(call_3 + 1, n12, 4, true);
  call_3[5] = '>';
  call_3[6] = '\0';

	char * call_1 = (iflip) ? c11 : call_3;
  char * call_2 = (iflip) ? call_3 : c11;
	//save_hash_call(c11_trimmed);

	if (icq == 0) {
		strcpy(field1, trim(call_1));
		if (nrpt == 1)
			strcpy(field3, "RRR");
		else if (nrpt == 2)
			strcpy(field3, "RR73");
		else if (nrpt == 3)
			strcpy(field3, "73");
        else {
            field3[0] = '\0';
        }
	} else {
		strcpy(field1, "CQ");
        field3[0] = '\0';
	}
  strcpy(field2, trim(call_2));

  return 0;
}

const char * trim_front(const char *str) {
  // Skip leading whitespace
  while (*str == ' ') {
    str++;
  }
  return str;
}

void trim_back(char *str) {
  // Skip trailing whitespace by replacing it with '\0' characters
  int idx = strlen(str) - 1;
  while (idx >= 0 && str[idx] == ' ') {
    str[idx--] = '\0';
  }
}

// 1) trims a string from the back by changing whitespaces to '\0'
// 2) trims a string from the front by skipping whitespaces
char * trim(char *str) {
  str = (char *)trim_front(str);
  trim_back(str);
  // return a pointer to the first non-whitespace character
  return str;
}

// Convert a 2 digit integer to string
void int_to_dd(char *str, int value, int width, bool full_sign) {
  if (value < 0) {
    *str = '-';
    ++str;
    value = -value;
  }
  else if (full_sign) {
    *str = '+';
    ++str;
  }

  int divisor = 1;
  for (int i = 0; i < width - 1; ++i) {
    divisor *= 10;
  }

  while (divisor >= 1) {
    int digit = value / divisor;

    *str = '0' + digit;
    ++str;

    value -= digit * divisor;
    divisor /= 10;
  }
  *str = 0;   // Add zero terminator
}

// convert integer index to ASCII character according to one of 6 tables:
// table 0: " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./?"
// table 1: " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
// table 2: "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
// table 3: "0123456789"
// table 4: " ABCDEFGHIJKLMNOPQRSTUVWXYZ"
// table 5: " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/"
char charn(int c, int table_idx) {
  if (table_idx != 2 && table_idx != 3) {
    if (c == 0) return ' ';
    c -= 1;
  }
  if (table_idx != 4) {
    if (c < 10) return '0' + c;
    c -= 10;
  }
  if (table_idx != 3) {
    if (c < 26) return 'A' + c;
    c -= 26;
  }

  if (table_idx == 0) {
    if (c < 5) return "+-./?" [c];
  }
  else if (table_idx == 5) {
    if (c == 0) return '/';
  }

  return '_'; // unknown character, should never get here
}

//-------------------------------------------------------------------------------------------------------------
// other
//-------------------------------------------------------------------------------------------------------------

FLASHMEM bool setupFT8() {
  initalize_constants();
  return init_DSP();
}

FLASHMEM bool setupFT8Wav() {
  int result;
  uint32_t slot_period = 15;
  uint32_t sample_rate = 12000;
  uint32_t num_samples = slot_period * sample_rate;

  result = load_wav(num_samples);

  if(result != 0) {
    Serial.println("Invalid wave file!");
    return false;
  }

  FT_8_counter = 0;
  return true;
}

FLASHMEM void exitFT8() {
  delete[] export_fft_power;

  // reset FT8 flags and counters
  syncFlag = false;
  ft8_decode_flag = 0;
  FT_8_counter = 0;
  ft8_flag = 0;
}

File f;
unsigned long position, sizeWav;

int readInt(int size) {
  char tmp[10] = {0,0,0,0,0,0,0,0,0,0};

  for(int i = 0; i < size; i++) {
//  for(int i = size - 1; i >= 0; i--) {
    tmp[i] = f.read();
  }
  return (int)*tmp;
}

// modified from: ft8_lib wave.c
// Load signal in floating point format (-1 .. +1) as a WAVE file using 16-bit signed integers.
int load_wav(uint32_t num_samples) {
  char subChunk1ID[4];    // = {'f', 'm', 't', ' '};
  uint32_t subChunk1Size; // = 16;    // 16 for PCM
  uint16_t audioFormat;   // = 1;     // PCM = 1
  uint16_t numChannels;   // = 1;
  uint16_t bitsPerSample; // = 16;
  uint32_t sampleRate;
  uint16_t blockAlign; // = numChannels * bitsPerSample / 8;
  uint32_t byteRate;   // = sampleRate * blockAlign;

  char subChunk2ID[4];    // = {'d', 'a', 't', 'a'};
  uint32_t subChunk2Size; // = num_samples * blockAlign;

  char chunkID[4];    // = {'R', 'I', 'F', 'F'};
  uint32_t chunkSize; // = 4 + (8 + subChunk1Size) + (8 + subChunk2Size);
  char format[4];     // = {'W', 'A', 'V', 'E'};
  char tmp[15];

  //FILE* f = fopen(path, "rb");
  f = SD.open("test.wav", FILE_READ);

  if (!f)
    return -1;

  f.seek(0);
  sizeWav = f.size();
  //ltoa(sizeWav, tmp, DEC);
  //Serial.println(tmp);

  f.read(tmp, sizeof(chunkID));

  f.read(tmp, sizeof(chunkSize));

  f.read(tmp, sizeof(format));

  f.read(tmp, sizeof(subChunk1ID));

  subChunk1Size = readInt(sizeof(subChunk1Size));
  //Serial.println(subChunk1Size);
  if (subChunk1Size != 16) {
    //Serial.println(subChunk1Size);
    return -2;
  }

  audioFormat = readInt(sizeof(audioFormat));
  numChannels = readInt(sizeof(numChannels));
  sampleRate = readInt(sizeof(sampleRate));
  byteRate = readInt(sizeof(byteRate));
  blockAlign = readInt(sizeof(blockAlign));
  bitsPerSample = readInt(sizeof(bitsPerSample));

  if (audioFormat != 1 || numChannels != 1 || bitsPerSample != 16)
    return -3;

  f.read(tmp, sizeof(subChunk2ID));
  //Serial.println(tmp);

  subChunk2Size = readInt(sizeof(subChunk2Size));

  if (subChunk2Size / blockAlign > num_samples)
    return -4;

  position = f.position();

  //int16_t* raw_data = (int16_t*)malloc(*num_samples * blockAlign);

  //fread((void*)raw_data, blockAlign, *num_samples, f);
  //for (int i = 0; i < *num_samples; i++) {
  //  //signal[i] = raw_data[i] / 32768.0f;
  //  f.read(tmp, sizeof(int16_t));
  //  signal[i] = atoi(tmp) / 32768.0f;
  //}

  //free(raw_data);

  //fclose(f);
  //f.close();

  return 0;
}

bool readWave(float32_t *buf, int sizeBuf) {
  unsigned long currentPos = f.position();
  int16_t raw_data[sizeBuf];

  // close file if we're done
  // *** likely missing the end of file here ***
  if(currentPos + sizeBuf >= sizeWav) {
    //f.seek(position); // reset wave file to beginning of data if needed
    f.close();
    return false;
  }

  f.read((void*)raw_data, sizeBuf*2);
  for (int i = 0; i < sizeBuf; i++) {
    buf[i] = raw_data[i] / 32768.0f;
  }
  return true;
}

#endif // #ifdef FT8
