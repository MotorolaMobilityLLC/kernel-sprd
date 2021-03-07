
#include <linux/notifier.h>
#include <uapi/drm/drm_mode.h>

#define ADF_EVENT_BLANK    0x10
#define ADF_EVENT_MODE_SET    0x11
/**
 *	adf_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int adf_register_client(struct notifier_block *nb);

/**
 *	adf_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int adf_unregister_client(struct notifier_block *nb);

struct adf_notifier_event {
	void *info;
	void *data;
};
