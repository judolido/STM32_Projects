#ifndef W5500_REGS_H_
#define W5500_REGS_H_

/* ------------------------------------------------------------------ */
/*  W5500 SPI frame: [ 16-bit address ][ 8-bit control ][ N data bytes ] */
/*  Control byte = [ BSB(5 bits) | RWB(1 bit) | OM(2 bits) ]            */
/*  RWB: 0 = read, 1 = write                                            */
/*  OM : 00 = Variable-length Data Mode (what we use throughout)        */
/* ------------------------------------------------------------------ */
#define W5500_BSB_COMMON        0x00u   /* Common register block      */
#define W5500_RWB_READ          0x00u
#define W5500_RWB_WRITE         0x04u
#define W5500_OM_VDM            0x00u

#define W5500_CTRL_COMMON_READ  ((W5500_BSB_COMMON << 3) | W5500_RWB_READ  | W5500_OM_VDM)
#define W5500_CTRL_COMMON_WRITE ((W5500_BSB_COMMON << 3) | W5500_RWB_WRITE | W5500_OM_VDM)

/* Common register block addresses (see W5500 datasheet ch.4) */
#define W5500_REG_MR         0x0000u  /* Mode Register                 */
#define W5500_REG_GAR        0x0001u  /* Gateway Address   (4 bytes)   */
#define W5500_REG_SUBR       0x0005u  /* Subnet Mask       (4 bytes)   */
#define W5500_REG_SHAR       0x0009u  /* Source MAC Addr   (6 bytes)   */
#define W5500_REG_SIPR       0x000Fu  /* Source IP Addr    (4 bytes)   */
#define W5500_REG_PHYCFGR    0x002Eu  /* PHY Configuration (1 byte)    */
#define W5500_REG_VERSIONR   0x0039u  /* Chip version, always 0x04     */

/* PHYCFGR bit fields */
#define W5500_PHYCFGR_RST    (1u << 7) /* write 0 to reset PHY, 1 = normal */
#define W5500_PHYCFGR_LNK    (1u << 0) /* 1 = link up                     */
#define W5500_PHYCFGR_SPD    (1u << 1) /* 1 = 100Mbps, 0 = 10Mbps          */
#define W5500_PHYCFGR_DPX    (1u << 2) /* 1 = full duplex, 0 = half        */

#endif /* W5500_REGS_H_ */
