#ifndef _HMC_VAULT_H_
#define _HMC_VAULT_H_

#include "hmc_notify.h"

class hmc_link;
class hmc_cube;

typedef enum{
  WR16      = 0x08,	/*! HMC-SIM: HMC_RQST_T: 16-BYTE WRITE REQUEST */
  WR32      = 0x09,	/*! HMC-SIM: HMC_RQST_T: 32-BYTE WRITE REQUEST */
  WR48      = 0x0A,	/*! HMC-SIM: HMC_RQST_T: 48-BYTE WRITE REQUEST */
  WR64      = 0x0B,	/*! HMC-SIM: HMC_RQST_T: 64-BYTE WRITE REQUEST */
  WR80      = 0x0C,	/*! HMC-SIM: HMC_RQST_T: 80-BYTE WRITE REQUEST */
  WR96      = 0x0D,	/*! HMC-SIM: HMC_RQST_T: 96-BYTE WRITE REQUEST */
  WR112     = 0x0E,	/*! HMC-SIM: HMC_RQST_T: 112-BYTE WRITE REQUEST */
  WR128     = 0x0F,	/*! HMC-SIM: HMC_RQST_T: 128-BYTE WRITE REQUEST */
  MD_WR     = 0x10,	/*! HMC-SIM: HMC_RQST_T: MODE WRITE REQUEST */
  BWR       = 0x11,	/*! HMC-SIM: HMC_RQST_T: BIT WRITE REQUEST */
  TWOADD8   = 0x12,	/*! HMC-SIM: HMC_RQST_T: DUAL 8-byte ADD IMMEDIATE */
  ADD16     = 0x13,	/*! HMC-SIM: HMC_RQST_T: SINGLE 16-byte ADD IMMEDIATE */
  P_WR16    = 0x18,	/*! HMC-SIM: HMC_RQST_T: 16-BYTE POSTED WRITE REQUEST */
  P_WR32    = 0x19,	/*! HMC-SIM: HMC_RQST_T: 32-BYTE POSTED WRITE REQUEST */
  P_WR48    = 0x1A,	/*! HMC-SIM: HMC_RQST_T: 48-BYTE POSTED WRITE REQUEST */
  P_WR64    = 0x1B,	/*! HMC-SIM: HMC_RQST_T: 64-BYTE POSTED WRITE REQUEST */
  P_WR80    = 0x1C,	/*! HMC-SIM: HMC_RQST_T: 80-BYTE POSTED WRITE REQUEST */
  P_WR96    = 0x1D,	/*! HMC-SIM: HMC_RQST_T: 96-BYTE POSTED WRITE REQUEST */
  P_WR112   = 0x1E,	/*! HMC-SIM: HMC_RQST_T: 112-BYTE POSTED WRITE REQUEST */
  P_WR128   = 0x1F,	/*! HMC-SIM: HMC_RQST_T: 128-BYTE POSTED WRITE REQUEST */
  P_BWR     = 0x21,	/*! HMC-SIM: HMC_RQST_T: POSTED BIT WRITE REQUEST */
  P_2ADD8   = 0x22,	/*! HMC-SIM: HMC_RQST_T: POSTED DUAL 8-BYTE ADD IMMEDIATE */
  P_ADD16   = 0x23,	/*! HMC-SIM: HMC_RQST_T: POSTED SINGLE 16-BYTE ADD IMMEDIATE */
  RD16      = 0x30,	/*! HMC-SIM: HMC_RQST_T: 16-BYTE READ REQUEST */
  RD32      = 0x31,	/*! HMC-SIM: HMC_RQST_T: 32-BYTE READ REQUEST */
  RD48      = 0x32,	/*! HMC-SIM: HMC_RQST_T: 48-BYTE READ REQUEST */
  RD64      = 0x33,	/*! HMC-SIM: HMC_RQST_T: 64-BYTE READ REQUEST */
  RD80      = 0x34,	/*! HMC-SIM: HMC_RQST_T: 80-BYTE READ REQUEST */
  RD96      = 0x35,	/*! HMC-SIM: HMC_RQST_T: 96-BYTE READ REQUEST */
  RD112     = 0x36,	/*! HMC-SIM: HMC_RQST_T: 112-BYTE READ REQUEST */
  RD128     = 0x37,	/*! HMC-SIM: HMC_RQST_T: 128-BYTE READ REQUEST */
  RD256     = 0x77,	/*! HMC-SIM: HMC_RQST_T: 256-BYTE READ REQUEST */
  MD_RD     = 0x28,	/*! HMC-SIM: HMC_RQST_T: MODE READ REQUEST */
  FLOW_NULL = 0x00,	/*! HMC-SIM: HMC_RQST_T: NULL FLOW CONTROL */
  PRET      = 0x01,	/*! HMC-SIM: HMC_RQST_T: RETRY POINTER RETURN FLOW CONTROL */
  TRET      = 0x02,	/*! HMC-SIM: HMC_RQST_T: TOKEN RETURN FLOW CONTROL */
  IRTRY     = 0x03,	/*! HMC-SIM: HMC_RQST_T: INIT RETRY FLOW CONTROL */

  /* -- version 2.0 Command Additions */
  WR256     = 0x4F,	/*! HMC-SIM: HMC_RQST_T: 256-BYTE WRITE REQUEST */
  P_WR256   = 0x5F,	/*! HMC-SIM: HMC_RQST_T: 256-BYTE POSTED WRITE REQUEST */
  TWOADDS8R = 0x52,	/*! HMC-SIM: HMC_RQST_T: */
  ADDS16R   = 0x53,	/*! HMC-SIM: HMC_RQST_T: */
  INC8      = 0x50,	/*! HMC-SIM: HMC_RQST_T: 8-BYTE ATOMIC INCREMENT */
  P_INC8    = 0x54,	/*! HMC-SIM: HMC_RQST_T: POSTED 8-BYTE ATOMIC INCREMENT */
  XOR16     = 0x40,	/*! HMC-SIM: HMC_RQST_T: 16-BYTE ATOMIC XOR */
  OR16      = 0x41,	/*! HMC-SIM: HMC_RQST_T: 16-BYTE ATOMIC OR */
  NOR16     = 0x42,	/*! HMC-SIM: HMC_RQST_T: 16-BYTE ATOMIC NOR */
  AND16     = 0x43,	/*! HMC-SIM: HMC_RQST_T: 16-BYTE ATOMIC AND */
  NAND16    = 0x44,	/*! HMC-SIM: HMC_RQST_T: 16-BYTE ATOMIC NAND */
  CASGT8    = 0x60,	/*! HMC-SIM: HMC_RQST_T: 8-BYTE COMPARE AND SWAP IF GT */
  CASGT16   = 0x62,	/*! HMC-SIM: HMC_RQST_T: 16-BYTE COMPARE AND SWAP IF GT */
  CASLT8    = 0x61,	/*! HMC-SIM: HMC_RQST_T: 8-BYTE COMPARE AND SWAP IF LT */
  CASLT16   = 0x63,	/*! HMC-SIM: HMC_RQST_T: 16-BYTE COMPARE AND SWAP IF LT */
  CASEQ8    = 0x64,	/*! HMC-SIM: HMC_RQST_T: 8-BYTE COMPARE AND SWAP IF EQ */
  CASZERO16 = 0x65,	/*! HMC-SIM: HMC_RQST_T: 16-BYTE COMPARE AND SWAP IF ZERO */
  EQ8       = 0x69,	/*! HMC-SIM: HMC_RQST_T: 8-BYTE ATOMIC EQUAL */
  EQ16      = 0x68,	/*! HMC-SIM: HMC_RQST_T: 16-BYTE ATOMIC EQUAL */
  BWR8R     = 0x51,	/*! HMC-SIM: HMC_RQST_T: 8-BYTE ATOMIC BIT WRITE WITH RETURN */
  SWAP16    = 0x6A,	/*! HMC-SIM: HMC_RQST_T: 16-BYTE ATOMIC SWAP */

  /* -- CMC Types */
  CMC04     =    4,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=4 */
  CMC05     =    5,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=5 */
  CMC06     =    6,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=6 */
  CMC07     =    7,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=7 */
  CMC20     =   20,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=20 */
  CMC21     =   21,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=21 */
  CMC22     =   22,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=22 */
  CMC23     =   23,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=23 */
  CMC32     =   32,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=32 */
  CMC36     =   36,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=36 */
  CMC37     =   37,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=37 */
  CMC38     =   38,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=38 */
  CMC39     =   39,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=39 */
  CMC41     =   41,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=41 */
  CMC42     =   42,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=42 */
  CMC43     =   43,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=43 */
  CMC44     =   44,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=44 */
  CMC45     =   45,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=45 */
  CMC46     =   46,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=46 */
  CMC47     =   47,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=47 */
  CMC56     =   56,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=56 */
  CMC57     =   57,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=57 */
  CMC58     =   58,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=58 */
  CMC59     =   59,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=59 */
  CMC60     =   60,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=60 */
  CMC61     =   61,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=61 */
  CMC62     =   62,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=62 */
  CMC63     =   63,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=63 */
  CMC69     =   69,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=69 */
  CMC70     =   70,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=70 */
  CMC71     =   71,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=71 */
  CMC72     =   72,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=72 */
  CMC73     =   73,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=73 */
  CMC74     =   74,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=74 */
  CMC75     =   75,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=75 */
  CMC76     =   76,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=76 */
  CMC77     =   77,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=77 */
  CMC78     =   78,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=78 */
  CMC85     =   85,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=85 */
  CMC86     =   86,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=86 */
  CMC87     =   87,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=87 */
  CMC88     =   88,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=88 */
  CMC89     =   89,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=89 */
  CMC90     =   90,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=90 */
  CMC91     =   91,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=91 */
  CMC92     =   92,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=92 */
  CMC93     =   93,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=93 */
  CMC94     =   94,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=94 */
  CMC102    =  102,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=102 */
  CMC103    =  103,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=103 */
  CMC107    =  107,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=107 */
  CMC108    =  108,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=108 */
  CMC109    =  109,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=109 */
  CMC110    =  110,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=110 */
  CMC111    =  111,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=111 */
  CMC112    =  112,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=112 */
  CMC113    =  113,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=113 */
  CMC114    =  114,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=114 */
  CMC115    =  115,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=115 */
  CMC116    =  116,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=116 */
  CMC117    =  117,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=117 */
  CMC118    =  118,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=118 */
  CMC120    =  120,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=120 */
  CMC121    =  121,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=121 */
  CMC122    =  122,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=122 */
  CMC123    =  123,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=123 */
  CMC124    =  124,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=124 */
  CMC125    =  125,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=125 */
  CMC126    =  126,	/*! HMC-SIM: HMC_RQST_T: CMC CMD=126 */
  CMC127    =  127	/*! HMC-SIM: HMC_RQST_T: CMC CMD=127 */
} hmc_rqst_t;

class hmc_vault {

  hmc_link *link;

  hmc_cube *cube;

protected:
  bool hmcsim_process_rqst(void *packet);
  void hmcsim_packet_resp_len(hmc_rqst_t cmd, bool *no_response, unsigned *rsp_len);

public:
  hmc_vault(unsigned id, hmc_cube *cube, hmc_notify* notify, hmc_link *link);
  ~hmc_vault(void);
#ifndef HMC_USES_BOBSIM
  void clock(void);
#endif
};

#endif /* #ifndef _HMC_VAULT_H_ */
