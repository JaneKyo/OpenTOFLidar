//Written for TDC-GP21
// Laser current detector is connected to "STOP1" line
// Photosensor is connected to "STOP2" line

// Includes ------------------------------------------------------------------
#include "tdc_driver.h"
#include "spi_driver.h"
#include "math.h"
#include "main.h"
#include "hardware.h"


// Private typedef -----------------------------------------------------------
// Private define ------------------------------------------------------------

// TDC Opcodes
#define OPCODE_INIT             0x70
#define OPCODE_RESET            0x50
#define OPCODE_READ_REG         0xB0
#define OPCODE_WRITE_REG        0x80
#define OPCODE_START_TOF        0x01

// Values shift in bits

// ****** REGISTER 0 *********************************************************

// Sets number of pulses generated by fire pulse generator. [0-15]
#define REG0_ANZ_FIRE           28

// Sets predivider for internal clock signal of fire pulse generator [0-15]
#define REG0_DIV_FIRE           24

// Sets number of periods used for calibrating the ceramic resonator [0-3]
#define REG0_ANZ_PER_CALRES     22

// Sets predivider for CLKHS [0-3]
#define REG0_DIV_CLKHS          20

// Defines the time interval the chip waits after switching on the oscillator before making a measurement [0-3]
#define REG0_START_CLKHS_1_0    18

// Enables/disables calibration calculation in the ALU
#define REG0_CALIBRATE          13

// Enables/disables auto-calibration run in the TDC, 1 = auto-calibration disabled
#define REG0_NO_CAL_AUTO        12

// 1 = measurement mode 2
#define REG0_MESSB2             11

// 0 = non-inverted input signal � rising edge
#define REG0_NEG_STOP2          10

// 0 = non-inverted input signal � rising edge
#define REG0_NEG_STOP1          9

// 0 = non-inverted input signal � rising edge
#define REG0_NEG_START          8

// ****** REGISTER 1 *********************************************************

// Defines operator for ALU data post-processing [0-15]
#define REG1_HIT2               28

// Defines operator for ALU data post-processing [0-15]
#define REG1_HIT1               24

//1 - Enables fast init operation
#define REG1_EN_FAST_INIT       23

// [0-7]
#define REG1_HITIN2             19

// [0-7]
#define REG1_HITIN1             16

// Low current option for 32 kHz oscillator.
#define REG1_CURR32K            15

// Fire pulse is used as TDC start. The START input is disabled.
#define REG1_SEL_START_FIRE     14

//Defines functionality of EN_START pin. [0-7]
#define REG1_SEL_TSTO2          11

//Defines functionality of FIRE_IN pin. [0-7]
#define REG1_SEL_TSTO1          8

// ****** REGISTER 2 *********************************************************
// Activates interrupt sources [0-7]
#define REG2_EN_INT_2_0         29

// Edge sensitivity channel 2 -> 1 = rising and falling edge
#define REG2_RFEDGE2            28

// Edge sensitivity channel 1 -> 1 = rising and falling edge
#define REG2_RFEDGE1            27

//Delay value for internal stop enable unit, hit 1 channel 1
#define REG2_DELVAL1            8

// ****** REGISTER 3 *********************************************************
// Timeout forces ALU to write �hFFFFFFFF
#define REG3_EN_ERR_VAL         29

// Select predivider for timeout in measurement mode 2 [0-3]
#define REG3_SEL_TIMO_MB2       27

// Delay value for internal stop enable unit, hit 2 channel 1
#define REG3_DELVAL2            8

// ****** REGISTER 4 *********************************************************
// Delay value for internal stop enable unit, hit 3 channel 1.
#define REG4_DELVAL3            8

// ****** REGISTER 5 *********************************************************
// Output configuration for pulse generator [0-7]
#define REG5_CONF_FIRE          29

// Enables additional noise for start channel
#define REG5_EN_STARTNOISE      28

// Phase noise unit - 1 = disables phase noise
#define REG5_DIS_PHASESHIFT     27

//[0-7]
#define REG5_REPEAT_FIRE        24

// Enables phase reversing for each pulse of a sequence of up to 15 possible pulses
#define REG5_PHFIRE             8

// ****** REGISTER 6 *********************************************************
// Activates the analog part
#define REG6_EN_ANALOG          31

// Sets comparator offset [0-15]
#define REG6_DA_KORR            25

// Timer to charge up the capacitor
#define REG6_TW2                22

// Specifies the default level of the inactive fire buffer. 1 = LOW
#define REG6_FIREO_DEF          14

// Option to improve the resolution
#define REG6_QUAD_RES           13

// Option to improve the resolution
#define REG6_DOUBLE_RES         12

// Highest 3 bits of the number of fire pulses.
#define REG6_ANZ_FIRE_6_4         12

// Private variables ---------------------------------------------------------

// Value readed from 
uint16_t tmp_res0 = 0;
uint16_t tmp_res1 = 0;

// Value readed from TDC STATE register
volatile uint16_t tdc_debug_status = 0;

extern uint16_t device_state_mask;

// Private function prototypes -----------------------------------------------
void configure_reg1_start(void);
void configure_reg1_width(void);
uint8_t tdc_quick_check_status(void);

// Private functions ---------------------------------------------------------

void tdc_configure(void)
{
  uint32_t reg0 = 0;
  //uint32_t reg1 = 0;
  uint32_t reg2 = 0;
  uint32_t reg3 = 0;
  uint32_t reg5 = 0;
  uint32_t reg6 = 0;
  
  //REG 0
  reg0|= (uint32_t)1 << REG0_ANZ_FIRE; // 1 fire pulse
  reg0|= 1 << REG0_START_CLKHS_1_0; // 1 = Oscillator continuously on
  reg0|= 0 << REG0_CALIBRATE; // Disable calibration
  reg0|= 1 << REG0_NO_CAL_AUTO;// 1 = auto-calibration disabled
  reg0|= 7 << REG0_DIV_FIRE; //7.= divided by 8
  reg0|= 1 << REG0_DIV_CLKHS;//1 = clk divided by 2 
  tdc_write_register(OPCODE_WRITE_REG + 0,  reg0);
  
  configure_reg1_start();
  
  // *********************** REG 2
  reg2|= (uint32_t)(1+4) << REG2_EN_INT_2_0; // 1 - timeout interr, 4 - ALU interr enable
  reg2|= 1 << REG2_RFEDGE2;//1 = rising and falling edge at channel2
  tdc_write_register(OPCODE_WRITE_REG + 2,  reg2);
  
  //REG 3
  reg3|= 1 << REG3_EN_ERR_VAL; // Timeout forces ALU to write �hFFFFFFFF
  tdc_write_register(OPCODE_WRITE_REG + 3,  reg3);
  
  //REG 5
  reg5|= 2 << REG5_CONF_FIRE; //Bit 30 = 1: enable output FIRE_UP
  reg5|= 1 << REG5_DIS_PHASESHIFT;//Disable phase shift noise
  tdc_write_register(OPCODE_WRITE_REG + 5,  reg5);
  
  //REG 6
  //reg6|= (uint32_t)1 << REG6_EN_ANALOG;
  reg6|= 1 << REG6_FIREO_DEF; //1 = LOW
  tdc_write_register(OPCODE_WRITE_REG + 6,  reg6);
  
}

void tdc_send_reset(void)
{
  send_opcode_to_tdc(OPCODE_RESET);
  dwt_delay_ms(100);
}

// Check measurement state
// Return 1 if NO timeout
uint8_t tdc_quick_check_status(void) //called from tdc_read_three_registers() ONLY
{
  uint16_t status = (uint16_t)tdc_read_n_bytes(2, OPCODE_READ_REG + 4);
  
  if ((status & (1 << 9)) != 0)//timeout
    return 0;
  else
    return 1;
}

//Called from capture_ctr_make_measurement and dist_measurement_do_batch_meas
void tdc_start_pulse(void)
{
  configure_reg1_start();
  send_opcode_to_tdc(OPCODE_INIT);
  send_opcode_to_tdc(OPCODE_START_TOF);
}

//Not used
uint16_t tdc_read_raw_value(void)
{
  uint32_t value = tdc_read_n_bytes(4, OPCODE_READ_REG + 0);
  return (uint16_t)(value >> 16);
}

//Called from capture_ctr_make_measurement and dist_measurement_do_batch_meas
tdc_point_t tdc_read_three_registers(void)
{
  // time of flight
  tmp_res0 = (uint16_t)tdc_read_register_upper(OPCODE_READ_REG + 0);
  configure_reg1_width();
  dwt_delay_us(5);//working good without waiting for ALU
  // pulse width
  tmp_res1 = (uint16_t)tdc_read_register_upper(OPCODE_READ_REG + 1);
  uint8_t tdc_result = tdc_quick_check_status();
  if (tdc_result == 0)
  {
    tmp_res0 = 0xFFFF;
    tmp_res1 = 0xFFFF;
  }
  
  tdc_point_t tmp_point;
  tmp_point.start_value = tmp_res0;
  tmp_point.width_value = tmp_res1;
  return tmp_point;
}

// Configure ALU for calculating Stop1 CH2 (Rising) {photo} - Stop1 CH1 {laser}
void configure_reg1_start(void)
{
  uint32_t reg1 = 0;
  //REG 1
  //mode1 -> HIT1 - HIT2
  reg1|= (uint32_t)9 << REG1_HIT1; // 0x9 -> 1. Stop Ch2
  reg1|= 1 << REG1_HIT2; // 0x1 -> 1. Stop Ch1
  
  reg1|= 1 << REG1_HITIN1; // 1 hit on ch1 expected (laser)
  reg1|= 2 << REG1_HITIN2; // 2 hits on ch2 expected (photo)  
  
  reg1|= 1 << REG1_SEL_START_FIRE; // Fire pulse is used as TDC start
  reg1|= 7 << REG1_SEL_TSTO2;//7 = 4 kHz (32 kHz/8) clock - IMPORTANT
  
  reg1|= 3 << REG1_SEL_TSTO1;//3 = STOP2 TDC output

  tdc_write_register(OPCODE_WRITE_REG + 1,  reg1);
}

// Configure ALU for calculating Stop2 CH2 (Falling) {photo} - Stop1 CH2
// Used for calculating width
void configure_reg1_width(void)
{
  uint32_t reg1 = 0;
  //REG 1
  //mode1 -> HIT1 - HIT2
  reg1|= (uint32_t)0x0A << REG1_HIT1; // 0xA -> 2. Stop Ch2
  reg1|= (uint32_t)9 << REG1_HIT2; // 0x9 -> 1. Stop Ch2
  
  reg1|= 1 << REG1_HITIN1; // 1 hit on ch1 expected (laser)
  reg1|= 2 << REG1_HITIN2; // 2 hits on ch2 expected (photo)  
  
  reg1|= 1 << REG1_SEL_START_FIRE; // Fire pulse is used as TDC start
  reg1|= 7 << REG1_SEL_TSTO2;//7 = 4 kHz (32 kHz/8) clock - IMPORTANT
  
  reg1|= 3 << REG1_SEL_TSTO1;//3 = STOP2 TDC output
  
  tdc_write_register(OPCODE_WRITE_REG + 1,  reg1);
}



void tdc_test(void)
{
  //REG_1 - Content of highest 8 bits of write register 1, to be used for testing the communication
  uint32_t test = tdc_read_n_bytes(1, OPCODE_READ_REG + 5);
  if (test != 0x55)//If something written to the REG1, this walue will be another
  {
    //error
    device_state_mask |= TDC_STATE_INIT_FAIL_FLAG;
  }
  else
  {
  }
}
