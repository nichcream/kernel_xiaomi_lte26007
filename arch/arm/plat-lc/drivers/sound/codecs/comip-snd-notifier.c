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
 *	Version		Date		Author		Description
 *	 1.0.0		2013-12-14	yangshunlong	created
 *
 */
#include <plat/comip-snd-notifier.h>

RAW_NOTIFIER_HEAD(audio_notifier_list);
RAW_NOTIFIER_HEAD(call_notifier_list);
RAW_NOTIFIER_HEAD(fm_notifier_list);


int register_audio_state_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&audio_notifier_list, nb);
}

EXPORT_SYMBOL_GPL(register_audio_state_notifier);

int unregister_audio_state_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&audio_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(unregister_audio_state_notifier);


int register_call_state_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&call_notifier_list, nb);
}

EXPORT_SYMBOL_GPL(register_call_state_notifier);

int unregister_call_state_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&call_notifier_list, nb);
}

EXPORT_SYMBOL_GPL(unregister_call_state_notifier);

int register_fm_state_notifier(struct notifier_block * nb)
{
	return raw_notifier_chain_register(&fm_notifier_list,nb);
}

EXPORT_SYMBOL_GPL(register_fm_state_notifier);

int unregister_fm_state_notifier(struct notifier_block * nb)
{
	return raw_notifier_chain_unregister(&fm_notifier_list,nb);
}

EXPORT_SYMBOL_GPL(unregister_fm_state_notifier);

