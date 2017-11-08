#ifndef __ASM_ARCH_REGS_USB_H
#define __ASM_ARCH_REGS_USB_H

/* start:register define for lc1810 usb-otg controller. */

#define USB_BASE			(USB0_BASE)

/*USB Global registers*/
#ifdef CONFIG_USB_COMIP_HSIC
#define USB_GOTGCTL			(USB_BASE + 0x0)
#define USB_GOTGINT			(USB_BASE + 0x04)
#endif
#define USB_GAHBCFG			(USB_BASE + 0x08)
#define USB_GAHBCFG_PTXFEL		(1<<8)/*Periodical TX FIFO empty level,0:half;1:full */
#define USB_GAHBCFG_NPTXFEL		(1<<7)/*None-Periodical TX FIFO empty level,0:half;1:full */
#define USB_GAHBCFG_DMAEN		(1<<5)/*0:slave 1:DMA*/
#define USB_GAHBCFG_HBLEN		(0x7<<1)/*AHB burst type*/
#define USB_GAHBCFG_GLBINTMSK		(1<<0)/*Global interrupt Mask bit,0:mask 1:cancle mask*/

#define USB_GUSBCFG			(USB_BASE + 0x0C)

#define USB_GUSBCFG_FRCDM		(1<<30)/*0:normal  1:force Device Modle*/
#define USB_GUSBCFG_FRCHM		(1<<29)/*0:normal  1:force Host Modle*/
#define USB_GUSBCFG_TRDTIM		(0x5<<10)/*Turnaround time set*/
#define USB_GUSBCFG_HNPCAP		(1<<9)/*0:HNP disable 1:HNP enable*/
#define USB_GUSBCFG_SRPCAP		(1<<8)/*0:SRP disable 1:SRP disable*/
#define USB_GUSBCFG_PHYSEL		(1<<6)/*Now always 0*/
#define USB_GUSBCFG_UTMISEL		(1<<4)/*Now always 0*/
#define USB_GUSBCFG_PHYIF		(1<<3)/*PHY bit width 1:16bit*/
#define USB_GUSBCFG_TOUTCAL		(0x5<<0)/*Timeout set between package*/

#define USB_GRSTCTL			(USB_BASE + 0x10)
#define USB_GRSTCTL_AHBIDLE		(1<<31)/*0:AHB interface busy,1:AHB interface idle*/
#define USB_GRSTCTL_DMAREQ		(1<<30)/*DMA request*/
#define USB_GRSTCTL_TXFNUM		(0x1f<<6)/*Tx FIFO Number,0x10 :all Tx FIFO*/

#define USB_GRSTCTL_TXFFLSH		(1<<5)/*Tx FIFO Flush*/
#define USB_GRSTCTL_RXFFLSH		(1<<4)/*Rx FIFO Flush*/
#define USB_GRSTCTL_FRMCNTRST		(1<<2)/*Host frame counter reset*/
#define USB_GRSTCTL_CSRST		(1<<0)/*USB Controller soft reset*/

#define USB_GINTSTS			(USB_BASE + 0x14)
#define USB_GINTSTS_WKUPINT		(1<<31)/*Assert when resume in USB Device*/
#define USB_GINTSTS_SREQINT		(1<<30)/*SRP in OTG*/
#define USB_GINTSTS_DISCONNINT		(1<<29)/*Assert when disconnet from Device in Host mode*/
#define USB_GINTSTS_CONIDSTSCHNG	(1<<28)/*ID change in OTG*/
#define USB_GINTSTS_PTXFEMP		(1<<26)/*Periodical Tx FIFO empty interrupt only used in Host*/
#define USB_GINTSTS_HCHINT		(1<<25)/*Channel interrupt used in Host*/
#define USB_GINTSTS_PRTINT		(1<<24)/*Port status interrupt used in Host*/
#define USB_GINTSTS_FETSUSP		(1<<22)/*Fetch data form TxFIFO pause interrupt only used in DMA*/
#define USB_GINTSTS_INCOMPISOOUT	(1<<21)/*At lease one ISO OUT EP incomplete */
#define USB_GINTSTS_INCOMPISOIN 	(1<<20)/*At lease one ISO IN EP incomplete */
#define USB_GINTSTS_OEPINT		(1<<19)/*OUT EP interrupt*/
#define USB_GINTSTS_IEPINT		(1<<18)/*IN EP interrupt*/
#define USB_GINTSTS_EOPF		(1<<15)/*Piodical frames finish interrupt*/
#define USB_GINTSTS_ISOOUTDROP		(1<<14)/*ISO OUT package drop interrupt*/
#define USB_GINTSTS_ENUMDONE		(1<<13)/*Speed enum finished interrupt*/
#define USB_GINTSTS_USBRST		(1<<12)/*USB bus reset*/
#define USB_GINTSTS_USBSUSP		(1<<11)/*USB bus suspend*/
#define USB_GINTSTS_ERLYSUSP		(1<<10)/*USB Early suspend :BUS IDLE >=3ms*/
#define USB_GINTSTS_GOUTNAKEFF		(1<<7) /*Global OUT NAK effective interrupt*/
#define USB_GINTSTS_GINNAKEFF		(1<<6) /*Global IN NAK effective interrupt*/
#define USB_GINTSTS_NPTXFEMP		(1<<5) /*None-Periodical Tx FIFO empty int*/
#define USB_GINTSTS_RXFLVL		(1<<4) /*Rx FIFO No-empty int*/
#define USB_GINTSTS_SOF 		(1<<3) /*SOF*/
#define USB_GINTSTS_OTGINT		(1<<2) /*OTG int*/
#define USB_GINTSTS_MODEMIS		(1<<1) /*Mode mismatch int*/
#define USB_GINTSTS_CURMOD		(1<<0) /*1:Host  0:Device*/

#define USB_GINTMSK			(USB_BASE + 0x18)/*Int Mask as above USB_GINTSTS*/
#define USB_GINTMSK_WKUPINT		(1<<31)
#define USB_GINTMSK_SREQINT		(1<<30)
#define USB_GINTMSK_DISCONNINT		(1<<29)
#define USB_GINTMSK_CONIDSTSCHNG	(1<<28)
#define USB_GINTMSK_PTXFEMP		(1<<26)
#define USB_GINTMSK_HCHINT		(1<<25)
#define USB_GINTMSK_PRTINT		(1<<24)
#define USB_GINTMSK_FETSUSP		(1<<22)
#define USB_GINTMSK_INCOMPISOOUT	(1<<21)
#define USB_GINTMSK_INCOMPISOIN 	(1<<20)
#define USB_GINTMSK_OEPINT		(1<<19)
#define USB_GINTMSK_IEPINT		(1<<18)
#define USB_GINTMSK_EOPF		(1<<15)
#define USB_GINTMSK_ISOOUTDROP		(1<<14)
#define USB_GINTMSK_ENUMDONE		(1<<13)
#define USB_GINTMSK_USBRST		(1<<12)
#define USB_GINTMSK_USBSUSP		(1<<11)
#define USB_GINTMSK_ERLYSUSP		(1<<10)
#define USB_GINTMSK_GOUTNAKEFF		(1<<7)
#define USB_GINTMSK_GINNAKEFF		(1<<6)
#define USB_GINTMSK_NPTXFEMP		(1<<5)
#define USB_GINTMSK_RXFLVL		(1<<4)
#define USB_GINTMSK_SOF 		(1<<3)
#define USB_GINTMSK_OTGINT		(1<<2)
#define USB_GINTMSK_MODEMIS		(1<<1)

#define USB_GRXSTSR			(USB_BASE + 0x1C)
#define USB_GRXSTSR_FN			(0xf<<21) /*Frame Number*/
#define USB_GRXSTSR_PKTSTS		(0xf<<17) /*Package status*/
#define USB_GRXSTSR_DPID		(0x3<<15) /*DATA PID 00:DATA0 ,10:DATA1 ,01:DATA2*/
#define USB_GRXSTSR_BCNT		(0x7ff<<4)/*Byte count of this received package*/
#define USB_GRXSTSR_EPNUM		(0xf<<0)	/*EP number of this received package*/
#define USB_GRXSTSP			(USB_BASE + 0x20)

#define USB_GRXSTSP_FN			(0xf<<21) /*Frame Number*/
#define USB_GRXSTSP_PKTSTS		(0xf<<17) /*Package status*/
#define USB_GRXSTSP_DPID		(0x3<<15) /*DATA PID 00:DATA0 ,10:DATA1 ,01:DATA2*/
#define USB_GRXSTSP_BCNT		(0x7ff<<4)/*Byte count of this received package*/
#define USB_GRXSTSP_EPNUM		(0xf<<0)	/*EP number of this received package*/

#define USB_GRXFSIZ			(USB_BASE + 0x24)
#define USB_GRXFSIZ_RXFDEP		(0xffff<<0)/*Rx FIFO depth*/

#define USB_GNPTXFSIZ			(USB_BASE + 0x28)
#define USB_GNPTXFSIZ_NPTXFDEP		(0xffff<<16)/*Nperiodical Tx FIFO depth */
#define USB_GNPTXFSIZ_NPTXFSAD		(0xffff<<0) /*Nperiodical Tx FIFO address*/

#define USB_GNPTXSTS			(USB_BASE + 0x2C)
#define USB_GNPTXSTS_NPTXQTOP		(24)
#define USB_GNPTXSTS_NPTXQSPCAVAIL	(16)
#define USB_GNPTXSTS_NPTXFSPCAVAIL	(0)
#define USB_HPTXFSIZ			(USB_BASE + 0x100)

#define USB_DIEPTXF0			(USB_BASE + 0x104)
#define USB_DIEPTXF(x)			(USB_DIEPTXF0+(x-1)*0x4)/*x=EP No.:frome ep1 to ep7*/
#define USB_DIEPTXF_INEPTXFDEP		(0xffff<<16) /*IN ep FIFO depth*/
#define USB_DIEPTXF_INEPTXFSTAD 	(0xffff<<0) /*IN ep FIFO RAM address*/

/*USB Host registers*/
#define USB_HCFG			(USB_BASE + 0x400)
#define USB_HFIR			(USB_BASE + 0x404)
#define USB_HFNUM			(USB_BASE + 0x408)
#define USB_HPTXSTS			(USB_BASE + 0x410)
#define USB_HAINT			(USB_BASE + 0x414)
#define USB_HAINTEN			(USB_BASE + 0x418)
#define USB_HPRT			(USB_BASE + 0x440)
#define USB_HCCHAR0			(USB_BASE + 0x500)
#define USB_HCINT0			(USB_BASE + 0x508)
#define USB_HCINTEN0			(USB_BASE + 0x50C)
#define USB_HCTSIZ0			(USB_BASE + 0x510)
#define USB_HCDMA0			(USB_BASE + 0x514)

/*USB Device registers*/
#define USB_DCFG			(USB_BASE + 0x800)
#define USB_DCFG_PERSCHINTVL		(0x3<<24) /*Periodical Schedule interval*/
#define USB_DCFG_DESCDMA		(1<<23)/*Scatter/gather DMA enable*/
/*
 This bit is only valid in version v2.93a and higher.
 This bit enables setting NAK for Bulk OUT endpoints after the transfer is completed
 when the core is operating in Device Descriptor DMA mode
 1'b0: The core does not set NAK after Bulk OUT transfer complete
 1'b1: The core sets NAK after Bulk OUT transfer complete
 This bit is valid only when OTG_EN_DESC_DMA == 1'b1
*/
#define USB_DCFG_ENOUTNAK		(1<<13)/*Enable Device OUT NAK (EnDevOutNak)*/
#define USB_DCFG_PERFRINT		(0x3<<11)/*Periodical frame interval*/
#define USB_DCFG_DEVADDR		(0x7f<<4) /*Device Address*/
#define USB_DCFG_NZSTSOUTHSHK		(1<<2) /*Nonzero status OUT handshake*/
#define USB_DCFG_DEVSPD 		(3<<0) /*0:High 1:Full*/

#define USB_DCTL			(USB_BASE + 0x804)
#define USB_DCTL_IGFRMNUM		(1<<15) /*Ignore frame number for ISO EP*/
#define USB_DCTL_GMC			(3<<13) /*Global package count */
#define USB_DCTL_CGOUTNAK		(1<<10) /*Clear Global OUT NAK*/
#define USB_DCTL_SGOUTNAK		(1<<9) /*Set Global OUT NAK*/
#define USB_DCTL_CGNPINNAK		(1<<8) /*Clear Global NP IN NAK*/
#define USB_DCTL_SGNPINNAK		(1<<7) /*Set Global NP IN NAK*/
#define USB_DCTL_TSTCTL 		(7<<4) /*Test control*/
#define USB_DCTL_TSTCTL_SHIFT	   4
#define USB_DCTL_GOUTNAKSTS		(1<<3) /*Global OUT NAK status*/
#define USB_DCTL_GNPINNAKSTS		(1<<2) /*Global NP IN NAK status*/
#define USB_DCTL_SFTDISC		(1<<1) /*Soft disconnect*/
#define USB_DCTL_WKUPSIG		(1<<0) /*Remote Wakeup signal*/

#define USB_DSTS			(USB_BASE + 0x808)
#define USB_DSTS_SOFFN			(0x3fff<<8) /*Frame number of SOF*/
#define USB_DSTS_ERRTIC 		(1<<3) /*Babble Error*/
#define USB_DSTS_ENUMSPD		(3<<1)  /*Enum Speed*/
#define USB_DSTS_SUSPSTS		(1<<0) /*Suspend status*/

#define USB_DIEPMSK			(USB_BASE + 0x810)
#define USB_DIEPMSK_BNAMSK		(1<<9) /*Buffer can't be accessed by only in S/G DMA*/
#define USB_DIEPMSK_TXFUDNMSK		(1<<8) /*Tx FIFO underrun INT mask*/
#define USB_DIEPMSK_INNAKMSK		(1<<6) /*IN NAK effective INT mask*/
#define USB_DIEPMSK_INTXFEMSK		(1<<4) /*IN Token INT mask when TxFIFO empty*/
#define USB_DIEPMSK_TMOUTMSK		(1<<3) /*Timeout INT mask*/
#define USB_DIEPMSK_AHBERRMSK		(1<<2) /*AHB Error INT mask*/
#define USB_DIEPMSK_EPDISMSK		(1<<1) /*EP disable INT mask*/
#define USB_DIEPMSK_XFCOMPLMSK		(1<<0) /*Thransfer complet INT mask*/

#define USB_DOEPMSK			(USB_BASE + 0x814)
#define USB_DOEPMSK_BNAMSK		(1<<9) /*Buffer can't be accessed INT mask*/
#define USB_DOEPMSK_OUTERRMSK		(1<<8) /*OUT package Error INT mask*/
#define USB_DOEPMSK_B2BSUPMSK		(1<<6) /*B2B Setup package INT mask*/
#define USB_DOEPMSK_SPRMSK		(1<<5) /*Status Phase Receive mask*/
#define USB_DOEPMSK_OUTEPDISMSK 	(1<<4) /*OUT token INT mask when EP disable*/
#define USB_DOEPMSK_SETUPMSK		(1<<3) /*SETUP finish INT mask*/
#define USB_DOEPMSK_AHBERRMSK		(1<<2) /*AHB Error INT mask*/
#define USB_DOEPMSK_EPDISMSK		(1<<1) /*EP disable INT mask*/
#define USB_DOEPMSK_XFCOMPLMSK		(1<<0) /*Xfer finish INT mask*/

#define USB_DAINT			(USB_BASE + 0x818)
#define USB_DAINT_OUTEPINT		(0xff<<16) /*OUT EP INT 24~30:EP8~EP14*/
#define USB_DAINT_INEPINT		(0xff<<0)  /*IN EP INT 0~7:EP0~EP7*/

#define USB_DAINTMSK			(USB_BASE + 0x81C)
#define USB_DAINTMSK_OUTEPMSK		(0xff<<16) /*OUT EP INT MSK 24~30:EP8~EP14*/
#define USB_DAINTMSK_INEPMSK		(0xff<<0)  /*IN EP INT MSK 0~7:EP0~EP7*/
#define USB_DVBUSDIS			(USB_BASE + 0x828)
#define USB_DVBUSDIS_DVBUSDIS		(0xffff<<0)/*Device Vbus Discharge*/

#define USB_DVBUSPLUSE			(USB_BASE + 0x82C)
#define USB_DVBUSPLUSE_DVBPUL		(0xfff<<0) /*Device Vbus Pulsing*/

#define USB_DTHRCTL			(USB_BASE + 0x830)
#define USB_DTHRCTL_ARBPRKEN		(1<<27) /*Arbitrator pause enable*/
#define USB_DTHRCTL_RXTHRLEN		(0x1ff<<17)/*Rx FIFO THR length*/
#define USB_DTHRCTL_RXTHREN		(1<<16) /*Rx FIFO THR enable*/
#define USB_DTHRCTL_TXTHRLEN		(0x1ff<<2) /*Tx FIFO THR length*/
#define USB_DTHRCTL_ISOTHREN		(1<<1) /*ISO IN EP THR enable*/
#define USB_DTHRCTL_NISOTHREN		(1<<0) /*Non-ISO IN THR enable*/

#define USB_DIEPEMPMSK			(USB_BASE + 0x834)
#define USB_DIEPEMPMSK_INTXFEMSK	(0x7f<<8) /*IN EP FIFO empty INT mask*/
#define USB_DIEPEMPMSK_INTXFEMSK0	(1<<0) /*IN EP0 FIFO empty INT mask*/

#define USB_DIEPCTL0			(USB_BASE + 0x900)
#define USB_DIEPCTL0_EPEN		(1<<31) /*EP enable*/
#define USB_DIEPCTL0_EPDIS		(1<<30) /*EP disable*/
#define USB_DIEPCTL0_SNAK		(1<<27) /*Set NAK*/
#define USB_DIEPCTL0_CNAK		(1<<26) /*Clear NAK*/
#define USB_DIEPCTL0_STALL		(1<<21) /*STALL*/
#define USB_DIEPCTL0_SNP		(1<<20) /*Snoop mode*/
#define USB_DIEPCTL0_EPTYPE		(0x3<<18) /*EP Type :0 control EP*/
#define USB_DIEPCTL0_NAKSTS		(1<<17) /*NAK Status*/
#define USB_DIEPCTL0_ACTEP		(1<<15)  /*EP0 always active 1*/
#define USB_DIEPCTL0_MPS		(0x3<<0) /*MPS*/

#define USB_DIEPSIZ0			(USB_BASE + 0x910)
#define USB_DIEPSIZ0_PKTCNT		(1<<19) /*Package count*/
#define USB_DIEPSIZ0_XFERSIZE		(0x7f<<0) /*Xfer size*/

#define USB_DOEPCTL0			(USB_BASE + 0xB00)
#define USB_DOEPCTL0_EPEN		(1<<31) /*EP enable*/
#define USB_DOEPCTL0_EPDIS		(1<<30) /*EP disable*/
#define USB_DOEPCTL0_SNAK		(1<<27) /*Set NAK*/
#define USB_DOEPCTL0_CNAK		(1<<26) /*Clear NAK*/
#define USB_DOEPCTL0_TXFNUM		(0xf<<22) /*Tx FIFO number*/
#define USB_DOEPCTL0_STALL		(1<<21) /*STALL*/
#define USB_DOEPCTL0_EPTYPE		(0x3<<18) /*EP Type :0 control EP*/
#define USB_DOEPCTL0_NAKSTS		(1<<17) /*NAK Status*/
#define USB_DOEPCTL0_ACTEP		(1<<15) /*EP0 always active 1*/
#define USB_DOEPCTL0_MPS		(0x3<<0) /*MPS*/

#define USB_DOEPSIZ0			(USB_BASE + 0xB10)
#define USB_DOEPSIZ0_SUPCNT		(0x3<<29) /*SETUP package count*/
#define USB_DOEPSIZ0_XFERSIZE		(0x7f<<0) /*Xfer size*/

#define USB_DIEPCTL(x)			(USB_DIEPCTL0+(x)*0x20) /*x=EP No.:frome ep1 to ep7*/
#define USB_DOEPCTL(x)			(USB_DOEPCTL0+(x)*0x20) /*x=EP No.:frome ep8 to ep14*/
#define USB_DIOEPCTL_EPEN		(1<<31) /*IN/OUT EP enable*/
#define USB_DIOEPCTL_EPDIS		(1<<30) /*IN/OUT EP disable*/
#define USB_DIOEPCTL_SETD1PID		(1<<29) /*Set DATA1 PID used in bulk/interrupt ep*/
#define USB_DIOEPCTL_SETODDFR		(1<<29) /*Set odd frame used in ISO IN/OUT*/
#define USB_DIOEPCTL_SETD0PID		(1<<28) /*Set DATA0 PID used in bulk/interrupt ep*/
#define USB_DIOEPCTL_SETEVNFR		(1<<28) /*Set even frame used in ISO IN/OUT*/
#define USB_DIOEPCTL_SNAK		(1<<27) /*Set NAK*/
#define USB_DIOEPCTL_CNAK		(1<<26) /*clear NAK*/
#define USB_DIOEPCTL_TXFNUM		(0xf<<22) /*Tx FIFO number IN*/
#define USB_DIOEPCTL_STALL		(1<<21) /*STALL*/
#define USB_DIOEPCTL_SNP		(1<<20) /*Snoop mode*/
#define USB_DIOEPCTL_EPTYPE		(0x3<<18) /*Ep type*/
#define USB_DIOEPCTL_NAKSTS		(1<<17) /*NAK status*/
#define USB_DIOEPCTL_DPID		(1<<16) /*DATA0/DATA1 PID in bulk/interrupt ep*/
#define USB_DIOEPCTL_EOFRNUM		(1<<16) /*Even or odd frame in ISO EP*/
#define USB_DIOEPCTL_ACTEP		(1<<15) /*Active ?*/
#define USB_DIOEPCTL_MPS		(0x7ff<<0) /*MPS*/

#define USB_DIEPINT0			(USB_BASE + 0x908)
#define USB_DOEPINT0			(USB_BASE + 0xB08)
#define USB_DIEPINT(x)			(USB_DIEPINT0+(x)*0x20) /*x=EP No.:from ep0,ep1 to ep7*/
#define USB_DOEPINT(x)			(USB_DOEPINT0+(x)*0x20) /*x=EP No.:from ep0,ep8 to ep14*/
#define USB_DIOEPINT_BNA		(1<<9) /*BNA INT*/
#define USB_DIOEPINT_TXFUNDN		(1<<8) /*TxFIFO underrun INT--IN ep*/
#define USB_DIOEPINT_OUTPKTERR		(1<<8) /*Package CRC Error/overflow INT--OUT ep*/
#define USB_DIOEPINT_TXFEMP		(1<<7) /*Tx FIFO empty INT*/
#define USB_DIOEPINT_INNAKEFF		(1<<6) /*IN EP NAK effective*/
#define USB_DIOEPINT_B2BSETUP		(1<<6) /*OUT EP B2B SETUP*/
#define USB_DIOEPINT_SPR		(1<<5) /*OUT EP Status phase Rec*/
#define USB_DIOEPINT_INTXFEMP		(1<<4) /*IN token when TxFIFO empty*/
#define USB_DIOEPINT_OUTEPDIS		(1<<4) /*EP disabled when recieve OUT token*/
#define USB_DIOEPINT_TIMEOUT		(1<<3) /*Timeout--IN*/
#define USB_DIOEPINT_SETUP		(1<<3) /*Setup finished--OUT*/
#define USB_DIOEPINT_AHBERR		(1<<2) /*AHB Error*/
#define USB_DIOEPINT_EPDIS		(1<<1) /*EP disabled*/
#define USB_DIOEPINT_XFCOMPL		(1<<0) /*Xfer finished*/

#define USB_DIEPSIZ(x)			(USB_DIEPSIZ0+(x)*0x20) /*x=EP No.:from ep1 to ep7*/
#define USB_DOEPSIZ(x)			(USB_DOEPSIZ0+(x)*0x20) /*x=EP No.:from ep8 to ep14*/
#define USB_DIOEPSIZ_MC 		(0x3<<29) /*IN ep Package count*/
#define USB_DIOEPSIZ_RXDPID		(0x3<<29) /*OUT ep data PID*/
#define USB_DIOEPSIZ_PKTCNT		(0x3ff<<19)  /*Package count decrease every Read/write FIFO*/
#define USB_DIOEPSIZ_XFERSIZ		(0x7ffff<<0) /*Length byte unit*/

#define USB_DIEPDMA0			(USB_BASE + 0x914)
#define USB_DOEPDMA0			(USB_BASE + 0xB14)
#define USB_DIEPDMA(x)			(USB_DIEPDMA0+(x)*0x20) /*x=EP No.:from ep0,ep1 to ep7*/
#define USB_DOEPDMA(x)			(USB_DOEPDMA0+(x)*0x20) /*x=EP No.:from ep0,ep8 to ep14*/
#define USB_DEOEPDMA_DMAADDR		(0xffffffff<<0)/*DMA address*/

#define USB_DIEPDMAB0			(USB_BASE + 0x91c)
#define USB_DOEPDMAB0			(USB_BASE + 0xB1c)
#define USB_DIEPDMAB(x) 		(USB_DIEPDMAB0+(x)*0x20)/*x=EP No.:from ep0,ep1 to ep7*/
#define USB_DOEPDMAB(x) 		(USB_DOEPDMAB0+(x)*0x20)/*x=EP No.:from ep0,ep8 to ep14*/
#define USB_DIOEPDMAB_DMABADDR		(0xffffffff<<0) /*DMA buffer address*/

#define USB_DTXFSTS0			(USB_BASE + 0x918)
#define USB_DTXFSTS(x)			(USB_DTXFSTS0+(x)*0x20) /*x=EP No.:from ep0,ep1 to ep7*/
#define USB_DTXFSTS_INTXFSPCAVAIL	(0xffff<<0)  /*Tx FIFO avail space IN */

/*others register and FIFO address*/
#define USB_PCGCCTL			(USB_BASE + 0xE00)
#define USB_PCGCCTL_GATEHCLK		(1<<1) /*HCLK gate control*/
#define USB_PCGCCTL_STPPCLK		(1<<0) /*PCLK stop*/
#define USB_DFIFO0			(USB_BASE + 0x1000)
#define USB_DFIFO(x)			(USB_DFIFO0+(x)*0x1000)/*Frome ep1 to ep14*/

/* register define for lc1860 usb-otg controller. :end */
#ifdef CONFIG_USB_COMIP_HSIC
/* register define for lc1860 usb-HSIC controller. :begin */
#define USB_BASE1			(USB_HSIC_BASE)

#define USB1_GOTGCTL			(USB_BASE1 + 0x0)
#define USB1_GOTGINT			(USB_BASE1 + 0x04)
#define USB1_GAHBCFG			(USB_BASE1 + 0x08)
#define USB1_GUSBCFG			(USB_BASE1 + 0x0C)
#define USB1_GRSTCTL			(USB_BASE1 + 0x10)
#define USB1_GINTSTS			(USB_BASE1 + 0x14)
#define USB1_GINTMSK			(USB_BASE1 + 0x18)/*Int Mask as above USB_GINTSTS*/
#define USB1_GRXSTSR			(USB_BASE1 + 0x1C)
#define USB1_GRXSTSP			(USB_BASE1 + 0x20)
#define USB1_GRXFSIZ			(USB_BASE1 + 0x24)
#define USB1_GNPTXFSIZ  			(USB_BASE1 + 0x28)
#define USB1_GNPTXSTS	  		(USB_BASE1+ 0x2C)
#define USB1_GLPMCFG			(USB_BASE1 + 0x0054)
#define USB1_GPWRDN				(USB_BASE1 + 0x58)
#define USB1_GPWRDN_PMUINTSEL    (1<<0)
#define USB1_GPWRDN_PMUACTV    (1<<1)/*PmuActv*/
#define USB1_GPWRDN_DIS_VBUS     (1<<6)
#define USB1_GPWRDN_DISCONN_DET_MSK     (1<<12)
#define USB1_GPWRDN_LNSTCHNG_MSK     (1<<8)
#define USB1_GPWRDN_STS_CHNGINT_MSK     (1<<18)
#define USB1_GPWRDN_PWRDNCLMP     (1<<3)
#define USB1_GPWRDN_PWRDNSWTCH     (1<<5)

#define USB1_HPTXFSIZ	  		(USB_BASE1 + 0x100)
#define USB1_DIEPTXF0	  		(USB_BASE1 + 0x104)
#define USB1_HCFG			  (USB_BASE1 + 0x400)
#define USB1_HFIR			  (USB_BASE1 + 0x404)
#define USB1_HFNUM			(USB_BASE1 + 0x408)
#define USB1_HPTXSTS			(USB_BASE1 + 0x410)
#define USB1_HAINT			(USB_BASE1 + 0x414)
#define USB1_HAINTEN			(USB_BASE1 + 0x418)
#define USB1_HPRT			  (USB_BASE1 + 0x440)
#define USB1_HPRT_PRTENA	(1<<2)/*PrtEna*/
#define USB1_HPRT_PRTSUSP	(1<<7)/*PrtSusp*/
#define USB1_HPRT_PRTRES	(1<<8)/*PrtRes*/
#define USB1_HCCHAR0			(USB_BASE1 + 0x500)
#define USB1_HCINT0		  	(USB_BASE1 + 0x508)
#define USB1_HCINTEN0	  		(USB_BASE1 + 0x50C)
#define USB1_HCTSIZ0			(USB_BASE1 + 0x510)
#define USB1_HCDMA0		  	(USB_BASE1 + 0x514)
#define USB1_DCFG			  (USB_BASE1 + 0x800)
#define USB1_DCTL			  (USB_BASE1 + 0x804)
#define USB1_DSTS			  (USB_BASE1 + 0x808)
#define USB1_DIEPMSK			(USB_BASE1 + 0x810)
#define USB1_DOEPMSK			(USB_BASE1 + 0x814)
#define USB1_DAINT			(USB_BASE1 + 0x818)
#define USB1_DAINTMSK	  		(USB_BASE1 + 0x81C)
#define USB1_DVBUSDIS	  		(USB_BASE1 + 0x828)
#define USB1_DVBUSPLUSE	  		(USB_BASE1 + 0x82C)
#define USB1_DTHRCTL			(USB_BASE1 + 0x830)
#define USB1_DIEPEMPMSK	 		(USB_BASE1 + 0x834)
#define USB1_DIEPCTL0	  		(USB_BASE1 + 0x900)
#define USB1_DIEPSIZ0	  		(USB_BASE1 + 0x910)
#define USB1_DOEPCTL0	  		(USB_BASE1 + 0xB00)
#define USB1_DOEPSIZ0	  		(USB_BASE1 + 0xB10)
#define USB1_DIEPINT0	  		(USB_BASE1 + 0x908)
#define USB1_DOEPINT0	  		(USB_BASE1 + 0xB08)
#define USB1_DIEPDMA0	  		(USB_BASE1 + 0x914)
#define USB1_DOEPDMA0	  		(USB_BASE1 + 0xB14)
#define USB1_DIEPDMAB0  			(USB_BASE1 + 0x91c)
#define USB1_DOEPDMAB0  			(USB_BASE1 + 0xB1c)
#define USB1_DTXFSTS0	  		(USB_BASE1 + 0x918)
#define USB1_PCGCCTL			(USB_BASE1 + 0xE00)
#define USB1_PCGCCTL_STOPPCLK	(1<<0)
#define USB1_PCGCCTL_GATEHCLK	(1<<1)
#define USB1_DFIFO0		  	(USB_BASE1 + 0x1000)
/* register define for lc1860 usb-HSIC controller. :end */
#endif

#endif /* __ASM_ARCH_REGS_USB_H */
