#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <linux/types.h>
#include <endian.h>
#include <sys/ioctl.h>
#include <sys/shm.h>
#include <linux/types.h>
#include <linux/hdreg.h>
#include <linux/major.h>
#include <asm/byteorder.h>
#include <sys/fcntl.h>
#if __BYTE_ORDER == __BIG_ENDIAN
#define __USE_XOPEN
#endif

#include "hdparm.h"

/* device types */
/* ------------ */
#define NO_DEV                  0xffff
#define ATA_DEV                 0x0000
#define ATAPI_DEV               0x0001

/* word definitions */
/* ---------------- */
#define GEN_CONFIG		0   /* general configuration */
#define LCYLS			1   /* number of logical cylinders */
#define CONFIG			2   /* specific configuration */
#define LHEADS			3   /* number of logical heads */
#define TRACK_BYTES		4   /* number of bytes/track (ATA-1) */
#define SECT_BYTES		5   /* number of bytes/sector (ATA-1) */
#define LSECTS			6   /* number of logical sectors/track */
#define START_SERIAL            10  /* ASCII serial number */
#define LENGTH_SERIAL           10  /* 10 words (20 bytes or characters) */
#define BUF_TYPE		20  /* buffer type (ATA-1) */
#define BUF_SIZE		21  /* buffer size (ATA-1) */
#define RW_LONG			22  /* extra bytes in R/W LONG cmd ( < ATA-4)*/
#define START_FW_REV            23  /* ASCII firmware revision */
#define LENGTH_FW_REV		 4  /*  4 words (8 bytes or characters) */
#define START_MODEL    		27  /* ASCII model number */
#define LENGTH_MODEL    	20  /* 20 words (40 bytes or characters) */
#define SECTOR_XFER_MAX	        47  /* r/w multiple: max sectors xfered */
#define DWORD_IO		48  /* can do double-word IO (ATA-1 only) */
#define CAPAB_0			49  /* capabilities */
#define CAPAB_1			50
#define PIO_MODE		51  /* max PIO mode supported (obsolete)*/
#define DMA_MODE		52  /* max Singleword DMA mode supported (obs)*/
#define WHATS_VALID		53  /* what fields are valid */
#define LCYLS_CUR		54  /* current logical cylinders */
#define LHEADS_CUR		55  /* current logical heads */
#define LSECTS_CUR	        56  /* current logical sectors/track */
#define CAPACITY_LSB		57  /* current capacity in sectors */
#define CAPACITY_MSB		58
#define SECTOR_XFER_CUR		59  /* r/w multiple: current sectors xfered */
#define LBA_SECTS_LSB		60  /* LBA: total number of user */
#define LBA_SECTS_MSB		61  /*      addressable sectors */
#define SINGLE_DMA		62  /* singleword DMA modes */
#define MULTI_DMA		63  /* multiword DMA modes */
#define ADV_PIO_MODES		64  /* advanced PIO modes supported */
				    /* multiword DMA xfer cycle time: */
#define DMA_TIME_MIN		65  /*   - minimum */
#define DMA_TIME_NORM		66  /*   - manufacturer's recommended	*/
				    /* minimum PIO xfer cycle time: */
#define PIO_NO_FLOW		67  /*   - without flow control */
#define PIO_FLOW		68  /*   - with IORDY flow control */
#define PKT_REL			71  /* typical #ns from PKT cmd to bus rel */
#define SVC_NBSY		72  /* typical #ns from SERVICE cmd to !BSY */
#define CDR_MAJOR		73  /* CD ROM: major version number */
#define CDR_MINOR		74  /* CD ROM: minor version number */
#define QUEUE_DEPTH		75  /* queue depth */
#define SATA_CAP_0		76  /* Serial ATA Capabilities */
#define SATA_RESERVED_77	77  /* reserved for future Serial ATA definition */
#define SATA_SUPP_0		78  /* Serial ATA features supported */
#define SATA_EN_0		79  /* Serial ATA features enabled */
#define MAJOR			80  /* major version number */
#define MINOR			81  /* minor version number */
#define CMDS_SUPP_0		82  /* command/feature set(s) supported */
#define CMDS_SUPP_1		83
#define CMDS_SUPP_2		84
#define CMDS_EN_0		85  /* command/feature set(s) enabled */
#define CMDS_EN_1		86
#define CMDS_EN_2		87
#define ULTRA_DMA		88  /* ultra DMA modes */
				    /* time to complete security erase */
#define ERASE_TIME		89  /*   - ordinary */
#define ENH_ERASE_TIME		90  /*   - enhanced */
#define ADV_PWR			91  /* current advanced power management level
				       in low byte, 0x40 in high byte. */  
#define PSWD_CODE		92  /* master password revision code	*/
#define HWRST_RSLT		93  /* hardware reset result */
#define ACOUSTIC  		94  /* acoustic mgmt values ( >= ATA-6) */
#define LBA_LSB			100 /* LBA: maximum.  Currently only 48 */
#define LBA_MID			101 /*      bits are used, but addr 103 */
#define LBA_48_MSB		102 /*      has been reserved for LBA in */
#define LBA_64_MSB		103 /*      the future. */
#define CMDS_SUPP_3		119
#define CMDS_EN_3		120
#define RM_STAT 		127 /* removable media status notification feature set support */
#define SECU_STATUS		128 /* security status */
#define CFA_PWR_MODE		160 /* CFA power mode 1 */
#define START_MEDIA             176 /* media serial number */
#define LENGTH_MEDIA            20  /* 20 words (40 bytes or characters)*/
#define START_MANUF             196 /* media manufacturer I.D. */
#define LENGTH_MANUF            10  /* 10 words (20 bytes or characters) */
#define SCT_SUPP		206 /* SMART command transport (SCT) support */
#define TRANSPORT_MAJOR		222 /* PATA vs. SATA etc.. */
#define TRANSPORT_MINOR		223 /* minor revision number */
#define INTEGRITY		255 /* integrity word */

/* bit definitions within the words */
/* -------------------------------- */

/* many words are considered valid if bit 15 is 0 and bit 14 is 1 */
#define VALID			0xc000
#define VALID_VAL		0x4000
/* many words are considered invalid if they are either all-0 or all-1 */
#define NOVAL_0			0x0000
#define NOVAL_1			0xffff

/* word 0: gen_config */
#define NOT_ATA			0x8000	
#define NOT_ATAPI		0x4000	/* (check only if bit 15 == 1) */
#define MEDIA_REMOVABLE		0x0080
#define DRIVE_NOT_REMOVABLE	0x0040  /* bit obsoleted in ATA 6 */
#define INCOMPLETE		0x0004
#define CFA_SUPPORT_VAL		0x848a	/* 848a=CFA feature set support */
#define DRQ_RESPONSE_TIME	0x0060
#define DRQ_3MS_VAL		0x0000
#define DRQ_INTR_VAL		0x0020
#define DRQ_50US_VAL		0x0040
#define PKT_SIZE_SUPPORTED	0x0003
#define PKT_SIZE_12_VAL		0x0000
#define PKT_SIZE_16_VAL		0x0001
#define EQPT_TYPE		0x1f00
#define SHIFT_EQPT		8

#define CDROM 0x0005

/* word 1: number of logical cylinders */
#define LCYLS_MAX		0x3fff /* maximum allowable value */

/* word 2: specific configureation 
 * (a) require SET FEATURES to spin-up
 * (b) require spin-up to fully reply to IDENTIFY DEVICE
 */
#define STBY_NID_VAL		0x37c8  /*     (a) and     (b) */
#define STBY_ID_VAL		0x738c	/*     (a) and not (b) */
#define PWRD_NID_VAL 		0x8c73	/* not (a) and     (b) */
#define PWRD_ID_VAL		0xc837	/* not (a) and not (b) */

/* words 47 & 59: sector_xfer_max & sector_xfer_cur */
#define SECTOR_XFER		0x00ff  /* sectors xfered on r/w multiple cmds*/
#define MULTIPLE_SETTING_VALID  0x0100  /* 1=multiple sector setting is valid */

/* word 49: capabilities 0 */
#define STD_STBY  	  	0x2000  /* 1=standard values supported (ATA);
					   0=vendor specific values */
#define IORDY_SUP		0x0800  /* 1=support; 0=may be supported */
#define IORDY_OFF		0x0400  /* 1=may be disabled */
#define LBA_SUP			0x0200  /* 1=Logical Block Address support */
#define DMA_SUP			0x0100  /* 1=Direct Memory Access support */
#define DMA_IL_SUP		0x8000  /* 1=interleaved DMA support (ATAPI) */
#define CMD_Q_SUP		0x4000  /* 1=command queuing support (ATAPI) */
#define OVLP_SUP		0x2000  /* 1=overlap operation support (ATAPI) */
#define SWRST_REQ		0x1000  /* 1=ATA SW reset required (ATAPI, obsolete */

/* word 50: capabilities 1 */
#define MIN_STANDBY_TIMER	0x0001  /* 1=device specific standby timer value minimum */

/* words 51 & 52: PIO & DMA cycle times */
#define MODE			0xff00  /* the mode is in the MSBs */

/* word 53: whats_valid */
#define OK_W88     	   	0x0004	/* the ultra_dma info is valid */
#define OK_W64_70		0x0002  /* see above for word descriptions */
#define OK_W54_58		0x0001  /* current cyl, head, sector, cap. info valid */

/*word 63,88: dma_mode, ultra_dma_mode*/
#define MODE_MAX		7	/* bit definitions force udma <=7 (when
					 * udma >=8 comes out it'll have to be
					 * defined in a new dma_mode word!) */

/* word 64: PIO transfer modes */
#define PIO_SUP			0x00ff  /* only bits 0 & 1 are used so far,  */
#define PIO_MODE_MAX		8       /* but all 8 bits are defined        */

/* word 75: queue_depth */
#define DEPTH_BITS		0x001f  /* bits used for queue depth */

/* words 80-81: version numbers */
/* NOVAL_0 or  NOVAL_1 means device does not report version */

/* word 81: minor version number */
#define MINOR_MAX		0x22

/* words 82-84: cmds/feats supported */
#define CMDS_W82		0x77ff  /* word 82: defined command locations*/
#define CMDS_W83		0x3fff  /* word 83: defined command locations*/
#define CMDS_W84		0x27ff  /* word 84: defined command locations*/
#define SUPPORT_48_BIT		0x0400  
#define NUM_CMD_FEAT_STR	48

static const char unknown[8] = "obsolete";
//static const char unknown[8] = "unknown";
#define unknown "unknown-"

/* words 85-87: cmds/feats enabled */
/* use cmd_feat_str[] to display what commands and features have
 * been enabled with words 85-87 
 */

/* words 89, 90, SECU ERASE TIME */
#define ERASE_BITS		0x00ff

/* word 92: master password revision */
/* NOVAL_0 or  NOVAL_1 means no support for master password revision */

/* word 93: hw reset result */
#define CBLID			0x2000  /* CBLID status */
#define RST0			0x0001  /* 1=reset to device #0 */
#define DEV_DET			0x0006  /* how device num determined */
#define JUMPER_VAL		0x0002  /* device num determined by jumper */
#define CSEL_VAL		0x0004  /* device num determined by CSEL_VAL */

/* word 127: removable media status notification feature set support */
#define RM_STAT_BITS 		0x0003
#define RM_STAT_SUP		0x0001
	
/* word 128: security */
#define SECU_ENABLED		0x0002
#define SECU_LEVEL		0x0100	/* was 0x0010 */
#define NUM_SECU_STR		6
const char *secu_str[] = {
	"supported",			/* word 128, bit 0 */
	"enabled",			/* word 128, bit 1 */
	"locked",			/* word 128, bit 2 */
	"frozen",			/* word 128, bit 3 */
	"count expired",	/* word 128, bit 4 */
	"enhanced erase"	/* word 128, bit 5 */
};

/* word 160: CFA power mode */
#define VALID_W160		0x8000  /* 1=word valid */
#define PWR_MODE_REQ		0x2000  /* 1=CFA power mode req'd by some cmds*/
#define PWR_MODE_OFF		0x1000  /* 1=CFA power moded disabled */
#define MAX_AMPS		0x0fff  /* value = max current in ma */

/* word 206: SMART command transport (SCT) */

/* word 255: integrity */
#define SIG			0x00ff  /* signature location */
#define SIG_VAL			0x00A5  /* signature value */

void print_ascii(__u16 *p, __u8 length);


#ifdef _MSC_VER
#define abs(x) ((__int64)(x) >= 0 ? (__int64)(x) : -(__int64)(x))
#endif

#ifdef O_NONBLOCK
static int open_flags = O_RDONLY|O_NONBLOCK;
#else
static int open_flags = O_RDONLY;
#endif

/* our main() routine: */
void identify (__u16 *id_supplied)
{

	__u16 val[256], ii, jj, kk;
	__u16 like_std = 1, std = 0, min_std = 0xffff;
	__u16 dev = NO_DEV, eqpt = NO_DEV;
	__u8  have_mode = 0, err_dma = 0;
	__u8  chksum = 0;
	__u32 ll, mm, nn;
	__u64 bb, bbbig; /* (:) */
	int transport;

	memcpy(val, id_supplied, sizeof(val));

	/* calculate checksum over all bytes */
	for(ii = GEN_CONFIG; ii<=INTEGRITY; ii++) {
		chksum += val[ii] + (val[ii] >> 8);
	}

	/* check if we recognise the device type */
	printf("\n");

	if(!(val[GEN_CONFIG] & NOT_ATA)) {
		dev = ATA_DEV;
	} else if(val[GEN_CONFIG]==CFA_SUPPORT_VAL) {
		dev = ATA_DEV;
		like_std = 4;
	} else if(!(val[GEN_CONFIG] & NOT_ATAPI)) {
		dev = ATAPI_DEV;
		eqpt = (val[GEN_CONFIG] & EQPT_TYPE) >> SHIFT_EQPT;
		like_std = 3;
	} else {
		exit(EINVAL);
	}

	/* Info from the specific configuration word says whether or not the
	 * ID command completed correctly.  It is only defined, however in
	 * ATA/ATAPI-5 & 6; it is reserved (value theoretically 0) in prior 
	 * standards.  Since the values allowed for this word are extremely
	 * specific, it should be safe to check it now, even though we don't
	 * know yet what standard this device is using.
	 */
	if((val[CONFIG]==STBY_NID_VAL) || (val[CONFIG]==STBY_ID_VAL) ||
	   (val[CONFIG]==PWRD_NID_VAL) || (val[CONFIG]==PWRD_ID_VAL) ) {
	   	like_std = 5;
		if((val[CONFIG]==STBY_NID_VAL) || (val[CONFIG]==STBY_ID_VAL))
			printf("powers-up in standby; SET FEATURES subcmd spins-up.\n");
		if(((val[CONFIG]==STBY_NID_VAL) || (val[CONFIG]==PWRD_NID_VAL)) &&
		   (val[GEN_CONFIG] & INCOMPLETE)) 
			printf("\n\tWARNING: ID response incomplete.\n\tWARNING: Following data may be incorrect.\n\n");
	}

	if(val[START_MODEL]) {
		printf("%-32s %-2s","HDD Model Number",": ");
		print_ascii(&val[START_MODEL], LENGTH_MODEL);
	}
	if(val[START_SERIAL]) {
		printf("%-32s %-2s","HDD Serial Number",": ");
		print_ascii( &val[START_SERIAL], LENGTH_SERIAL);
	}
	if(val[START_FW_REV]) {
		printf("%-32s %-2s","HDD Firmware Revision",": ");
		print_ascii(&val[START_FW_REV], LENGTH_FW_REV);
	}
	/* security */
		if(val[PSWD_CODE] && (val[PSWD_CODE] != NOVAL_1))
			printf("\tMaster password revision code = %u\n",val[PSWD_CODE]);
			for(ii = 0,jj = val[SECU_STATUS]; ii < NUM_SECU_STR; ii++) {
				printf("Security %-24s",secu_str[ii]);
				if(!(jj & 0x0001)) printf("%-4s\n",": No");
				else		   printf("%-4s\n",": Yes");
				jj >>=1;
			}
			if(val[SECU_STATUS] & SECU_ENABLED) {
				if(val[SECU_STATUS] & SECU_LEVEL) printf("Security level %-32s",": maximum\n");
				else				  printf("Security level %-32s",": high\n");
			}
		
}


int main(int argc, char **argv)
{
		 __u16 *id;
                unsigned char args[4+512] = {WIN_IDENTIFY,0,0,1,}; // FIXME?
                unsigned i;
		int fd;
		if (argc!=2)
		{
			printf ("device name is required\n");
			exit(1);
		}
		char * devname = *++argv;
		printf ("%s",devname);
		fd = open (devname, open_flags);
		if (fd < 0) {
			perror(devname);
			exit(errno);
		}

                if (ioctl(fd, HDIO_DRIVE_CMD, &args)) {
                        args[0] = WIN_PIDENTIFY;
                        if (ioctl(fd, HDIO_DRIVE_CMD, &args)) {
                                perror(" HDIO_DRIVE_CMD(identify) failed");
                                goto identify_abort;
                        }
                }
                id = (__u16 *)&args[4];
               
                        for(i = 0; i < 0x100; ++i) {
                                __le16_to_cpus(&id[i]);
                        }
                        identify((void *)id);
		exit(0);
identify_abort:
			;
		exit(1);

}
void print_ascii(__u16 *p, __u8 length) {
	__u8 ii;
	char cl;
	
	/* find first non-space & print it */
	for(ii = 0; ii< length; ii++) {
		if(((char) 0x00ff&((*p)>>8)) != ' ') break;
		if((cl = (char) 0x00ff&(*p)) != ' ') {
			if(cl != '\0') printf("%c",cl);
			p++; ii++;
			break;
		}
		p++;
	}
	/* print the rest */
	for(; ii< length; ii++) {
		unsigned char c;
		/* some older devices have NULLs */
		c = (*p) >> 8;
		if (c) putchar(c);
		c = (*p);
		if (c) putchar(c);
		p++;
	}
	printf("\n");
}
 