#include <metal/machine/platform.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// Pump operating mode and UART buffer
uint8_t OPERATING_MODE = 0;
uint8_t FloatBuffer[32];

/// Sample simulation and time frame of the controller
#define SIMULATION_TIME 24
#define SAMPLE_TIME 145

/// System Target
#define TARGET 85

/// PID bias
#define BIAS 0

/// PID upper and lower limits
#define OP_HI 100.0
#define OP_LO 0.0

/// Controller output
static float op[SAMPLE_TIME];

/// Process Variable
static float pv[SAMPLE_TIME];

/// Error
static float e[SAMPLE_TIME];

/// Error integral
static float ie[SAMPLE_TIME];

/// Derivative of the error
static float dpv[SAMPLE_TIME];

/// Proportional
static float P[SAMPLE_TIME];

/// Integral
static float IT[SAMPLE_TIME];

/// Derivative
static float D[SAMPLE_TIME];

/// Set Point
static float sp[SAMPLE_TIME];

/// Time
static float t[SAMPLE_TIME] = {
    0.0,  0.16666667,  0.33333333,  0.5,  0.66666667,  0.83333333,
    1.0,  1.16666667,  1.33333333,  1.5,  1.66666667,  1.83333333,
    2.0,  2.16666667,  2.33333333,  2.5,  2.66666667,  2.83333333,
    3.0,  3.16666667,  3.33333333,  3.5,  3.66666667,  3.83333333,
    4.0,  4.16666667,  4.33333333,  4.5,  4.66666667,  4.83333333,
    5.0,  5.16666667,  5.33333333,  5.5,  5.66666667,  5.83333333,
    6.0,  6.16666667,  6.33333333,  6.5,  6.66666667,  6.83333333,
    7.0,  7.16666667,  7.33333333,  7.5,  7.66666667,  7.83333333,
    8.0,  8.16666667,  8.33333333,  8.5,  8.66666667,  8.83333333,
    9.0,  9.16666667,  9.33333333,  9.5,  9.66666667,  9.83333333,
    10.0, 10.16666667, 10.33333333, 10.5, 10.66666667, 10.83333333,
    11.0, 11.16666667, 11.33333333, 11.5, 11.66666667, 11.83333333,
    12.0, 12.16666667, 12.33333333, 12.5, 12.66666667, 12.83333333,
    13.0, 13.16666667, 13.33333333, 13.5, 13.66666667, 13.83333333,
    14.0, 14.16666667, 14.33333333, 14.5, 14.66666667, 14.83333333,
    15.0, 15.16666667, 15.33333333, 15.5, 15.66666667, 15.83333333,
    16.0, 16.16666667, 16.33333333, 16.5, 16.66666667, 16.83333333,
    17.0, 17.16666667, 17.33333333, 17.5, 17.66666667, 17.83333333,
    18.0, 18.16666667, 18.33333333, 18.5, 18.66666667, 18.83333333,
    19.0, 19.16666667, 19.33333333, 19.5, 19.66666667, 19.83333333,
    20.0, 20.16666667, 20.33333333, 20.5, 20.66666667, 20.83333333,
    21.0, 21.16666667, 21.33333333, 21.5, 21.66666667, 21.83333333,
    22.0, 22.16666667, 22.33333333, 22.5, 22.66666667, 22.83333333,
    23.0, 23.16666667, 23.33333333, 23.5, 23.66666667, 23.83333333,
    24.0};

/// Time value iterator
uint32_t T_iterator = 0;

/// Manual Configurable Bolus
float BolusConfig = 0;

/// Configurable Controller PID Value
/// Proportionnal
static float Kc = -0.07;

/// Integration
static float Tau_I = 1;

/// Derivative
static float Tau_D = 1.2;

/// FIX: Helper function
int UARTGetChar(char *C);
int UARTRead(char *Buffer, int MaxChars);

/// Function declaration
void ResetController();
void InsulinController(uint8_t *GlucoseBuffer);
void InsulinManualController(uint8_t *GlucoseBuffer);

/// Controller Reset
void ResetController() {
  int i;

  /// Reset tables
  for (i = 0; i < SAMPLE_TIME; ++i) {
    pv[i] = 0;
    e[i] = 0;
    ie[i] = 0;
    dpv[i] = 0;
    P[i] = 0;
    IT[i] = 0;
    D[i] = 0;
    sp[i] = TARGET;
    op[i] = BIAS;
  }
}

/// Controller
void InsulinController(uint8_t *GlucoseBuffer) {
  float MeasuredGlucose;
  float DeltaT;

  char MeasuredGlucoseBuff[32];
  char AmountOfInsulinBuff[32];

  /// Iterator Modulo
  T_iterator = T_iterator % SAMPLE_TIME;

  if (T_iterator == 0) {
    ResetController();
  }

  /// scanf("%255s", FloatBuffer);
  MeasuredGlucose = atof((char *)GlucoseBuffer);
  gcvt(MeasuredGlucose, 7, MeasuredGlucoseBuff);
  printf("[G]:%s\n\r", MeasuredGlucoseBuff);

  /// The delta is always the same, it is for purpose
  if (T_iterator > 0) {
    DeltaT = t[T_iterator] - t[T_iterator - 1];
  } else {
    DeltaT = t[1] - t[0];
  }

  pv[T_iterator] = MeasuredGlucose;
  e[T_iterator] = sp[T_iterator] - pv[T_iterator];

  if (T_iterator > 0) {
    dpv[T_iterator] = (pv[T_iterator] - pv[T_iterator - 1]) / DeltaT;
    ie[T_iterator] = ie[T_iterator - 1] + e[T_iterator] * DeltaT;
  }

  /// PID Tuning
  P[T_iterator] = Kc * e[T_iterator];
  IT[T_iterator] = Kc / Tau_I * ie[T_iterator];
  D[T_iterator] = -Kc * Tau_D * dpv[T_iterator];
  op[T_iterator] = op[0] + P[T_iterator] + IT[T_iterator] + D[T_iterator];

  /// DEBUG:PID
  //   printf("P: %.4f\r\nI: %.4f\r\nD: %.4f\r\nDelta: %.4f\r\n", P[T_iterator],
  //          IT[T_iterator], D[T_iterator], DeltaT);

  /// Anti-reset windup
  if (op[T_iterator] > OP_HI) {
    op[T_iterator] = OP_HI;
    ie[T_iterator] = ie[T_iterator] - e[T_iterator] * DeltaT;
  } else if (op[T_iterator] < OP_LO) {
    op[T_iterator] = OP_LO;
    ie[T_iterator] = ie[T_iterator] - e[T_iterator] * DeltaT;
  }

  /// Send the amount of insulin to inject
  gcvt(op[T_iterator], 7, AmountOfInsulinBuff);
  printf("[u]:%s\n\r", AmountOfInsulinBuff);

  /// Iterate the time
  T_iterator++;
  memset(MeasuredGlucoseBuff, 0x00, sizeof(FloatBuffer));
  memset(AmountOfInsulinBuff, 0x00, sizeof(FloatBuffer));
}

/// Manual Controller
void InsulinManualController(uint8_t *GlucoseBuffer) {
  float MeasuredGlucose;

  char MeasuredGlucoseBuff[32];
  char AmountOfInsulinBuff[32];

  MeasuredGlucose = atof((char *)GlucoseBuffer);
  gcvt(MeasuredGlucose, 7, MeasuredGlucoseBuff);
  printf("[G]:%s\r\n", MeasuredGlucoseBuff);

  /// Send the amount of insulin to inject
  gcvt(BolusConfig, 7, AmountOfInsulinBuff);
  printf("[u]:%s\r\n", AmountOfInsulinBuff);

  memset(MeasuredGlucoseBuff, 0x00, sizeof(FloatBuffer));
  memset(AmountOfInsulinBuff, 0x00, sizeof(FloatBuffer));
}

int main(int argc, char **argv) {
  printf("==== SECPUMP RISC-V INIT ====\r\n");
  /// ResetController();

  int i = 0;

  while (1) {

    UARTRead(FloatBuffer, 32);

    printf("ACK:%s\n", FloatBuffer);

    if (!strncmp(FloatBuffer, "00", 2)) {
      printf("[*] Configuration Mode Request\r\n");
      if (!strncmp(&FloatBuffer[2], "0", 1)) {
        printf("[*] Switching to Auto MODE\r\n");

        OPERATING_MODE = 0;
      } else if (!strncmp(&FloatBuffer[2], "1", 1)) {

        printf("[*] Switching to Manual MODE\r\n");

        OPERATING_MODE = 1;
      }
    } else if (!strncmp(FloatBuffer, "01", 2)) {
      printf("[*] Setting Manual Bolus Injection Value\r\n");
      BolusConfig = atof((const char *)&FloatBuffer[2]);

    } else if (!strncmp(FloatBuffer, "10", 2)) {
      printf("[*] Glucose Measure\r\n");
      /// Pump MODE
      switch (OPERATING_MODE) {
      case 0:
        InsulinController(&FloatBuffer[2]);
        break;

      case 1:
        InsulinManualController(&FloatBuffer[2]);
        break;

      default:
        printf("[!] ERROR UNKNOWN MODE\r\n");
        break;
      }
    }

    memset(FloatBuffer, 0x00, sizeof(FloatBuffer));
  }

  return 0;
}

int UARTGetChar(char *C) {
  uint32_t *reg;

  reg = (uint32_t *)(METAL_SIFIVE_UART0_0_BASE_ADDRESS +
                     METAL_SIFIVE_UART0_RXDATA);
  if (!(*reg & (1 << 31))) {
    // printf("value at reg:%0.8x ", *reg);
    (*C) = (char)((*reg) & 0xff);
  } else {
    return -1;
  }

  return 0;
}

int UARTRead(char *Buffer, int MaxChars) {
  int i;
  for (i = 0; i < MaxChars; i++) {
    while (0 != UARTGetChar(&Buffer[i])) {
    }
  }

  /// NB of char
  return i + 1;
}
