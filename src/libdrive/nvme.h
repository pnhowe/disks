#ifndef _NVME_H
#define _NVME_H

#include "disk.h"

#define NVME_SERIAL_NUMBER_LEN 20
#define NVME_MODEL_NUMBER_LEN 40
#define NVME_FIRMREV_LEN 8


struct nvme_id_power_state {
        __le16                  max_power;      /* centiwatts */
        __u8                    rsvd2;
        __u8                    flags;
        __le32                  entry_lat;      /* microseconds */
        __le32                  exit_lat;       /* microseconds */
        __u8                    read_tput;
        __u8                    read_lat;
        __u8                    write_tput;
        __u8                    write_lat;
        __le16                  idle_power;
        __u8                    idle_scale;
        __u8                    rsvd19;
        __le16                  active_power;
        __u8                    active_work_scale;
        __u8                    rsvd23[9];
};

enum {
        NVME_PS_FLAGS_MAX_POWER_SCALE   = 1 << 0,
        NVME_PS_FLAGS_NON_OP_STATE      = 1 << 1,
};

struct nvme_identity_controller {
        __le16                  vid;
        __le16                  ssvid;
        char                    serial_number[NVME_SERIAL_NUMBER_LEN];
        char                    model_number[NVME_MODEL_NUMBER_LEN];
        char                    firmware_rev[NVME_FIRMREV_LEN];
        __u8                    rab;
        __u8                    ieee[3];
        __u8                    cmic;
        __u8                    mdts;
        __le16                  cntlid;
        __le32                  ver;
        __le32                  rtd3r;
        __le32                  rtd3e;
        __le32                  oaes;
        __le32                  ctratt;
        __u8                    rsvd100[156];
        __le16                  oacs;
        __u8                    acl;
        __u8                    aerl;
        __u8                    frmw;
        __u8                    lpa;
        __u8                    elpe;
        __u8                    npss;
        __u8                    avscc;
        __u8                    apsta;
        __le16                  wctemp;
        __le16                  cctemp;
        __le16                  mtfa;
        __le32                  hmpre;
        __le32                  hmmin;
        __u8                    tnvmcap[16];
        __u8                    unvmcap[16];
        __le32                  rpmbs;
        __le16                  edstt;
        __u8                    dsto;
        __u8                    fwug;
        __le16                  kas;
        __le16                  hctma;
        __le16                  mntmt;
        __le16                  mxtmt;
        __le32                  sanicap;
        __le32                  hmminds;
        __le16                  hmmaxd;
        __u8                    rsvd338[174];
        __u8                    sqes;
        __u8                    cqes;
        __le16                  maxcmd;
        __le32                  nn;
        __le16                  oncs;
        __le16                  fuses;
        __u8                    fna;
        __u8                    vwc;
        __le16                  awun;
        __le16                  awupf;
        __u8                    nvscc;
        __u8                    rsvd531;
        __le16                  acwu;
        __u8                    rsvd534[2];
        __le32                  sgls;
        __u8                    rsvd540[228];
        char                    subnqn[256];
        __u8                    rsvd1024[768];
        __le32                  ioccsz;
        __le32                  iorcsz;
        __le16                  icdoff;
        __u8                    ctrattr;
        __u8                    msdbd;
        __u8                    rsvd1804[244];
        struct nvme_id_power_state      psd[32];
        __u8                    vs[1024];
};

struct nvme_lbaf { // Figure 246
__le16                  ms;
__u8                    ds;
__u8                    rp;
};

struct nvme_identity_ns {
        __le64                  nsze;
        __le64                  ncap;
        __le64                  nuse;
        __u8                    nsfeat;
        __u8                    nlbaf;
        __u8                    flbas;
        __u8                    mc;
        __u8                    dpc;
        __u8                    dps;
        __u8                    nmic;
        __u8                    rescap;
        __u8                    fpi;
        __u8                    rsvd33;
        __le16                  nawun;
        __le16                  nawupf;
        __le16                  nacwu;
        __le16                  nabsn;
        __le16                  nabo;
        __le16                  nabspf;
        __le16                  noiob;
        __u8                    nvmcap[16];
        __u8                    rsvd64[40];
        __u8                    nguid[16];
        __u8                    eui64[8];
        struct nvme_lbaf        lbaf[16];
        __u8                    rsvd192[192];
        __u8                    vs[3712];
};

struct nvme_identity
{
  __u8 data[ sizeof( struct drive_info ) ];
};

typedef enum
{
  NVME_OP_AMDIN_GET_LOG_PAGE     = 0x02,

  NVME_OP_AMDIN_GET_LOG_IDENTIFY = 0x06,

  NVME_OP_AMDIN_KEEP_ALIVE       = 0x18, // used to probe/ping the controller
} NVME_COMMANDS;


struct nvme_cdb {
  NVME_COMMANDS  opcode;
  unsigned int   nsid;
  unsigned int   cdw10;
  unsigned int   cdw11;
  unsigned int   cdw12;
  unsigned int   cdw13;
  unsigned int   cdw14;
  unsigned int   cdw15;
};

void loadInfoNVME( struct device_handle *drive, const struct nvme_identity *identity );
int exec_cmd_nvme( struct device_handle *drive, const enum cdb_command command, const enum cdb_rw rw, void *data, const unsigned int data_len, __u64 parms[], const unsigned int parm_count, const unsigned int timeout );

int nvme_open( const char *device, struct device_handle *drive );
int nvme_isvalidname( const char *device );
char *nvme_validnames( void );

#endif
