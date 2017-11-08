/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2013  LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the snd notifier driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version 	Date		Author		Description
 *	 1.0.0		2013-12-14	yangshunlong	created
 *
 */
#ifndef __COMIP_SND_NOTIFIER_H__
#define __COMIP_SND_NOTIFIER_H__

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <asm/io.h>


enum _SND_STATE{
   VOICE_CALL_START,
   VOICE_CALL_STOP,
   FM_START,
   FM_STOP,
   AUDIO_START,
   AUDIO_STOP
};
extern struct raw_notifier_head audio_notifier_list;
extern struct raw_notifier_head call_notifier_list;
extern struct raw_notifier_head fm_notifier_list;

extern int register_audio_state_notifier(struct notifier_block *nb);
extern int unregister_audio_state_notifier(struct notifier_block *nb);
extern int register_call_state_notifier(struct notifier_block *nb);
extern int unregister_call_state_notifier(struct notifier_block *nb);
extern int register_fm_state_notifier(struct notifier_block * nb);
extern int unregister_fm_state_notifier(struct notifier_block * nb);

#endif
