#ifndef _ONTIM_HWINFO_H
#define _ONTIM_HWINFO_H

#define KOBJ_ATTR_RO(_name) struct kobj_attribute dev_attr_##_name = __ATTR_RO(_name)
#define KOBJ_ATTR_WO(_name) struct kobj_attribute dev_attr_##_name = __ATTR_WO(_name)
#define KOBJ_ATTR_RW(_name) struct kobj_attribute dev_attr_##_name = __ATTR_RW(_name)

extern struct kobject *hwinfo;

const char *hwinfo_get_prop(const char *prop_name);

#endif
