#ifndef _LSI_H
#define _LSI_H

#include <linux/types.h>
#include <scsi/sg.h>
#include <sys/uio.h>

#include "disk.h"

/* barrowed from smartctl megaraid.h and linux.h */

/*======================================================
 * PERC2/3/4 Passthrough SCSI Command Interface
 *
 * Contents from:
 *  drivers/scsi/megaraid/megaraid_ioctl.h
 *  drivers/scsi/megaraid/mbox_defs.h
 *======================================================*/
#define MEGAIOC_MAGIC   'm'
#define MEGAIOCCMD      _IOWR(MEGAIOC_MAGIC, 0, struct uioctl_t)

/* Following subopcode work for opcode == 0x82 */
#define MKADAP(adapno)   (MEGAIOC_MAGIC << 8 | adapno)
#define MEGAIOC_QNADAP     'm'
#define MEGAIOC_QDRVRVER   'e'
#define MEGAIOC_QADAPINFO  'g'

#define MEGA_MBOXCMD_PASSTHRU 0x03

#define MAX_REQ_SENSE_LEN  0x20
#define MAX_CDB_LEN 10

typedef struct
{
        __u8 timeout : 3;
        __u8  ars : 1;
        __u8  reserved : 3;
        __u8  islogical : 1;
        __u8  logdrv;
        __u8  channel;
        __u8  target;
        __u8  queuetag;
        __u8  queueaction;
        __u8  cdb[MAX_CDB_LEN];
        __u8  cdblen;
        __u8  reqsenselen;
        __u8  reqsensearea[MAX_REQ_SENSE_LEN];
        __u8  numsgelements;
        __u8  scsistatus;
        __u32 dataxferaddr;
        __u32 dataxferlen;
} __attribute__((packed)) mega_passthru;


typedef struct
{
        __u8   cmd;
        __u8   cmdid;
        __u8   opcode;
        __u8   subopcode;
        __u32  lba;
        __u32  xferaddr;
        __u8   logdrv;
        __u8   resvd[3];
        __u8   numstatus;
        __u8   status;
} __attribute__((packed)) megacmd_t;

typedef union {
        __u8   *pointer;
        __u8    pad[8];
} ptr_t;

// The above definition assumes sizeof(void*) <= 8.
// This assumption also exists in the linux megaraid device driver.
// So define a macro to check expected size of ptr_t at compile time using
// a dummy typedef.  On size mismatch, compiler reports a negative array
// size.  If you see an error message of this form, it means that
// you have an unexpected pointer size on your platform and can not
// use megaraid support in smartmontools.
typedef char assert_sizeof_ptr_t[sizeof(ptr_t) == 8 ? 1 : -1];

struct uioctl_t
{
        __u32       inlen;
        __u32       outlen;
        union {
                __u8      fca[16];
                struct {
                        __u8  opcode;
                        __u8  subopcode;
                        __u16 adapno;
                        ptr_t    buffer;
                        __u32 length;
                } __attribute__((packed)) fcs;
        } __attribute__((packed)) ui;

        megacmd_t     mbox;
        mega_passthru pthru;
        ptr_t         data;
} __attribute__((packed));


/*===================================================
 * PERC5/6 Passthrough SCSI Command Interface
 *
 * Contents from:
 *  drivers/scsi/megaraid/megaraid_sas.h
 *===================================================*/
#define MEGASAS_MAGIC          'M'
#define MEGASAS_IOC_FIRMWARE   _IOWR(MEGASAS_MAGIC, 1, struct megasas_iocpacket)

#define MFI_CMD_PD_SCSI_IO        0x04
#define MFI_FRAME_SGL64           0x02
#define MFI_FRAME_DIR_READ        0x10

#define MAX_IOCTL_SGE                   16


struct megasas_sge32 {

        __u32 phys_addr;
        __u32 length;

} __attribute__ ((packed));

struct megasas_sge64 {

        __u64 phys_addr;
        __u32 length;

} __attribute__ ((packed));

union megasas_sgl {

        struct megasas_sge32 sge32[1];
        struct megasas_sge64 sge64[1];

} __attribute__ ((packed));

struct megasas_header {

        __u8 cmd;                 /*00h */
        __u8 sense_len;           /*01h */
        __u8 cmd_status;          /*02h */
        __u8 scsi_status;         /*03h */

        __u8 target_id;           /*04h */
        __u8 lun;                 /*05h */
        __u8 cdb_len;             /*06h */
        __u8 sge_count;           /*07h */

        __u32 context;            /*08h */
        __u32 pad_0;              /*0Ch */

        __u16 flags;              /*10h */
        __u16 timeout;            /*12h */
        __u32 data_xferlen;       /*14h */

} __attribute__ ((packed));


struct megasas_pthru_frame {

        __u8 cmd;                 /*00h */
        __u8 sense_len;           /*01h */
        __u8 cmd_status;          /*02h */
        __u8 scsi_status;         /*03h */

        __u8 target_id;           /*04h */
        __u8 lun;                 /*05h */
        __u8 cdb_len;             /*06h */
        __u8 sge_count;           /*07h */

        __u32 context;            /*08h */
        __u32 pad_0;              /*0Ch */

        __u16 flags;              /*10h */
        __u16 timeout;            /*12h */
        __u32 data_xfer_len;      /*14h */

        __u32 sense_buf_phys_addr_lo;     /*18h */
        __u32 sense_buf_phys_addr_hi;     /*1Ch */

        __u8 cdb[16];             /*20h */
        union megasas_sgl sgl;  /*30h */

} __attribute__ ((packed));


struct megasas_dcmd_frame {

        __u8 cmd;                 /*00h */
        __u8 reserved_0;          /*01h */
        __u8 cmd_status;          /*02h */
        __u8 reserved_1[4];       /*03h */
        __u8 sge_count;           /*07h */

        __u32 context;            /*08h */
        __u32 pad_0;              /*0Ch */

        __u16 flags;              /*10h */
        __u16 timeout;            /*12h */

        __u32 data_xfer_len;      /*14h */
        __u32 opcode;             /*18h */

        union {                 /*1Ch */
                __u8 b[12];
                __u16 s[6];
                __u32 w[3];
        } mbox;

        union megasas_sgl sgl;  /*28h */

} __attribute__ ((packed));


struct megasas_iocpacket {

        __u16 host_no;
        __u16 __pad1;
        __u32 sgl_off;
        __u32 sge_count;
        __u32 sense_off;
        __u32 sense_len;
        union {
                __u8 raw[128];
                struct megasas_header hdr;
                struct megasas_pthru_frame pthru;
        } frame;

        struct iovec sgl[MAX_IOCTL_SGE];

} __attribute__ ((packed));


// from drivers/scsi/megaraid/megaraid_sas.h
#define MFI_CMD_STATUS_POLL_MODE                0xFF

enum MFI_STAT {
        MFI_STAT_OK = 0x00,
        MFI_STAT_INVALID_CMD = 0x01,
        MFI_STAT_INVALID_DCMD = 0x02,
        MFI_STAT_INVALID_PARAMETER = 0x03,
        MFI_STAT_INVALID_SEQUENCE_NUMBER = 0x04,
        MFI_STAT_ABORT_NOT_POSSIBLE = 0x05,
        MFI_STAT_APP_HOST_CODE_NOT_FOUND = 0x06,
        MFI_STAT_APP_IN_USE = 0x07,
        MFI_STAT_APP_NOT_INITIALIZED = 0x08,
        MFI_STAT_ARRAY_INDEX_INVALID = 0x09,
        MFI_STAT_ARRAY_ROW_NOT_EMPTY = 0x0a,
        MFI_STAT_CONFIG_RESOURCE_CONFLICT = 0x0b,
        MFI_STAT_DEVICE_NOT_FOUND = 0x0c,
        MFI_STAT_DRIVE_TOO_SMALL = 0x0d,
        MFI_STAT_FLASH_ALLOC_FAIL = 0x0e,
        MFI_STAT_FLASH_BUSY = 0x0f,
        MFI_STAT_FLASH_ERROR = 0x10,
        MFI_STAT_FLASH_IMAGE_BAD = 0x11,
        MFI_STAT_FLASH_IMAGE_INCOMPLETE = 0x12,
        MFI_STAT_FLASH_NOT_OPEN = 0x13,
        MFI_STAT_FLASH_NOT_STARTED = 0x14,
        MFI_STAT_FLUSH_FAILED = 0x15,
        MFI_STAT_HOST_CODE_NOT_FOUNT = 0x16,
        MFI_STAT_LD_CC_IN_PROGRESS = 0x17,
        MFI_STAT_LD_INIT_IN_PROGRESS = 0x18,
        MFI_STAT_LD_LBA_OUT_OF_RANGE = 0x19,
        MFI_STAT_LD_MAX_CONFIGURED = 0x1a,
        MFI_STAT_LD_NOT_OPTIMAL = 0x1b,
        MFI_STAT_LD_RBLD_IN_PROGRESS = 0x1c,
        MFI_STAT_LD_RECON_IN_PROGRESS = 0x1d,
        MFI_STAT_LD_WRONG_RAID_LEVEL = 0x1e,
        MFI_STAT_MAX_SPARES_EXCEEDED = 0x1f,
        MFI_STAT_MEMORY_NOT_AVAILABLE = 0x20,
        MFI_STAT_MFC_HW_ERROR = 0x21,
        MFI_STAT_NO_HW_PRESENT = 0x22,
        MFI_STAT_NOT_FOUND = 0x23,
        MFI_STAT_NOT_IN_ENCL = 0x24,
        MFI_STAT_PD_CLEAR_IN_PROGRESS = 0x25,
        MFI_STAT_PD_TYPE_WRONG = 0x26,
        MFI_STAT_PR_DISABLED = 0x27,
        MFI_STAT_ROW_INDEX_INVALID = 0x28,
        MFI_STAT_SAS_CONFIG_INVALID_ACTION = 0x29,
        MFI_STAT_SAS_CONFIG_INVALID_DATA = 0x2a,
        MFI_STAT_SAS_CONFIG_INVALID_PAGE = 0x2b,
        MFI_STAT_SAS_CONFIG_INVALID_TYPE = 0x2c,
        MFI_STAT_SCSI_DONE_WITH_ERROR = 0x2d,
        MFI_STAT_SCSI_IO_FAILED = 0x2e,
        MFI_STAT_SCSI_RESERVATION_CONFLICT = 0x2f,
        MFI_STAT_SHUTDOWN_FAILED = 0x30,
        MFI_STAT_TIME_NOT_SET = 0x31,
        MFI_STAT_WRONG_STATE = 0x32,
        MFI_STAT_LD_OFFLINE = 0x33,
        MFI_STAT_PEER_NOTIFICATION_REJECTED = 0x34,
        MFI_STAT_PEER_NOTIFICATION_FAILED = 0x35,
        MFI_STAT_RESERVATION_IN_PROGRESS = 0x36,
        MFI_STAT_I2C_ERRORS_DETECTED = 0x37,
        MFI_STAT_PCI_ERRORS_DETECTED = 0x38,

        MFI_STAT_INVALID_STATUS = 0xFF
};

int lsi_open( const char *device, struct device_handle *drive, const enum driver_type driver );
int lsi_isvalidname( const char *device );
char *lsi_validnames( void );

#endif
