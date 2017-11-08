

#ifdef BEAMFORMING_SUPPORT

VOID
Beamforming_GidPAid(
	struct rtl8192cd_priv *priv,
	struct stat_info	*pstat
);

enum _BEAMFORMING_CAP

Beamforming_GetEntryBeamCapByMacId(
	struct rtl8192cd_priv *priv,
	IN	u1Byte		MacId
);

BOOLEAN
Beamforming_InitEntry(
	struct rtl8192cd_priv	*priv,
	struct stat_info		*pSTA,
	pu1Byte					Idx	
);

VOID
Beamforming_DeInitEntry(
	struct rtl8192cd_priv *priv,
	pu1Byte			RA
);

VOID
Beamforming_Enter(
	struct rtl8192cd_priv *priv,
	struct stat_info	*pstat
);

VOID
Beamforming_TimerCallback(
	struct rtl8192cd_priv *priv
);

VOID
Beamforming_AutoTest(
	struct rtl8192cd_priv *priv,
	u1Byte					Idx, 
	struct _RT_BEAMFORMING_ENTRY *pBeamformEntry
);

VOID
Beamforming_End(
	struct rtl8192cd_priv *priv,
	BOOLEAN			Status	
);

VOID
Beamforming_Leave(
	struct rtl8192cd_priv *priv,
	pu1Byte			RA
);
VOID
Beamforming_Release(
	struct rtl8192cd_priv *priv
);

VOID
Beamforming_Init(
	struct rtl8192cd_priv *priv
);

VOID Beamforming_SetTxBFen(
	struct rtl8192cd_priv *priv, u1Byte MacId, BOOLEAN	bTxBF
);

PRT_BEAMFORMING_ENTRY
Beamforming_GetEntryByMacId(
	struct rtl8192cd_priv *priv,
	u1Byte		MacId,
	pu1Byte		Idx
	);

PRT_BEAMFORMING_ENTRY
Beamforming_GetFreeEntry(
	struct rtl8192cd_priv *priv,
	pu1Byte		Idx
	);

VOID
Beamforming_GetNDPAFrame(
	struct rtl8192cd_priv *priv,
	pu1Byte					pNDPAFrame
	);


#endif

